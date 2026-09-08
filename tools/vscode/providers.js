const vscode = require('vscode');
const path = require('path');
const langService = require('./language_service');

function getConfig(resource) {
    return vscode.workspace.getConfiguration('avalang', resource);
}

function resolveModulesPath(document) {
    const config = getConfig(document.uri);
    const configured = config.get('modulesPath');
    if (configured) return configured;
    const executablePath = config.get('executablePath') || '';
    if (executablePath && path.isAbsolute(executablePath)) {
        return path.join(path.dirname(executablePath), 'modules');
    }
    const workspaceFolder = vscode.workspace.getWorkspaceFolder(document.uri);
    return workspaceFolder ? path.join(workspaceFolder.uri.fsPath, 'modules') : '';
}

function workspaceRootOf(document) {
    const folder = vscode.workspace.getWorkspaceFolder(document.uri);
    return folder ? folder.uri.fsPath : path.dirname(document.uri.fsPath);
}

function buildIndexFor(document) {
    return langService.buildIndex(
        document.uri.fsPath,
        document.getText(),
        resolveModulesPath(document),
        workspaceRootOf(document)
    );
}

function flattenedMembers(classesMap, className) {
    const methods = new Map();
    const attributes = new Map();
    let current = className;
    let depth = 0;
    while (current && classesMap.has(current) && depth < 8) {
        const info = classesMap.get(current);
        for (const [name, m] of info.methods) if (!methods.has(name)) methods.set(name, Object.assign({}, m, { file: info.file }));
        for (const [name, a] of info.attributes) if (!attributes.has(name)) attributes.set(name, Object.assign({}, a, { file: info.file }));
        current = info.base;
        depth++;
    }
    return { methods, attributes };
}

function inferVariableClass(text, varName, classesMap, beforeOffset) {
    const re = langService.buildVarAssignRegex(varName);
    let match;
    let found = null;
    while ((match = re.exec(text)) !== null) {
        if (match.index > beforeOffset) break;
        if (classesMap.has(match[1])) found = match[1];
    }
    return found;
}

function parseSegment(seg) {
    const m = /^([A-Za-z_]\w*)(\([\s\S]*\))?$/.exec(seg.trim());
    return m ? { name: m[1], isCall: !!m[2] } : null;
}

function resolveChain(document, position, segments, index) {
    const text = document.getText();
    let current = null;

    for (let i = 0; i < segments.length; ++i) {
        const parsed = parseSegment(segments[i]);
        if (!parsed) return null;
        const { name, isCall } = parsed;

        if (i === 0) {
            if (name === 'this' || name === 'self') {
                const className = langService.enclosingClassAt(text, position.line);
                if (!className) return null;
                current = { kind: 'class', className, viewerIsInside: true };
                continue;
            }
            if (index.importedModules.has(name) && !isCall) {
                current = { kind: 'module', module: index.importedModules.get(name) };
                continue;
            }
            if (isCall && index.functions.has(name)) {
                const fn = index.functions.get(name);
                if (!fn.returnType || !index.classes.has(fn.returnType)) return null;
                current = { kind: 'class', className: fn.returnType, viewerIsInside: false };
                continue;
            }
            const offset = document.offsetAt(position);
            const className = inferVariableClass(text, name, index.classes, offset);
            if (!className) return null;
            current = { kind: 'class', className, viewerIsInside: false };
            continue;
        }

        if (current.kind === 'module') {
            if (isCall && current.module.classes.has(name)) {
                current = { kind: 'class', className: name, viewerIsInside: false };
                continue;
            }
            return null;
        }

        const { methods, attributes } = flattenedMembers(index.classes, current.className);
        if (isCall) {
            if (!methods.has(name)) return null;
            const m = methods.get(name);
            if (m.isPrivate && !current.viewerIsInside) return null;
            if (!m.returnType || !index.classes.has(m.returnType)) return null;
            current = { kind: 'class', className: m.returnType, viewerIsInside: false };
        } else {
            if (!attributes.has(name)) return null;
            const a = attributes.get(name);
            if (a.isPrivate && !current.viewerIsInside) return null;
            if (!a.type || !index.classes.has(a.type)) return null;
            current = { kind: 'class', className: a.type, viewerIsInside: false };
        }
    }

    return current;
}

function getChainAndPartial(document, position) {
    const text = document.getText();
    const offset = document.offsetAt(position);
    let partialStart = offset;
    while (partialStart > 0 && /\w/.test(text[partialStart - 1])) partialStart--;
    if (partialStart === 0 || text[partialStart - 1] !== '.') return null;
    const dotOffset = partialStart - 1;
    const segments = langService.extractChainSegments(text, dotOffset);
    if (!segments) return null;
    return { segments, partial: text.slice(partialStart, offset) };
}

function constructorParams(classesMap, className) {
    const { methods } = flattenedMembers(classesMap, className);
    const ctor = methods.get(className);
    return ctor ? ctor.params : [];
}

function snippetEscape(text) {
    return text.replace(/[$}\\]/g, '\\$&');
}

function buildConstructorSnippet(className, params, alreadyHasNew) {
    const parts = [];
    let tabIndex = 1;
    for (const raw of params) {
        const trimmed = raw.trim();
        if (trimmed.startsWith('*')) break;
        const match = /^([A-Za-z_]\w*)/.exec(trimmed);
        if (!match) continue;
        parts.push('${' + tabIndex + ':' + snippetEscape(match[1]) + '}');
        tabIndex++;
    }
    const inner = parts.length ? parts.join(', ') : '$0';
    const prefix = alreadyHasNew ? '' : 'new ';
    return new vscode.SnippetString(`${prefix}${className}(${inner})`);
}

function applyConstructorSnippet(item, config, classesMap, className, alreadyHasNew) {
    if (!config.get('completion.constructorSnippets', true)) return;
    const params = constructorParams(classesMap, className);
    item.insertText = buildConstructorSnippet(className, params, alreadyHasNew);
    item.filterText = className;
    if (params.length) item.detail = `${item.detail}  ·  new ${className}(${params.join(', ')})`;
}

function memberCompletionItems(resolved, index, config) {
    const items = [];
    if (resolved.kind === 'module') {
        for (const [name, fn] of resolved.module.functions) {
            const item = new vscode.CompletionItem(name, vscode.CompletionItemKind.Function);
            item.detail = `${name}(${fn.params.join(', ')})`;
            items.push(item);
        }
        for (const className of resolved.module.classes.keys()) {
            const item = new vscode.CompletionItem(className, vscode.CompletionItemKind.Class);
            applyConstructorSnippet(item, config, resolved.module.classes, className);
            items.push(item);
        }
        return items;
    }
    const { methods, attributes } = flattenedMembers(index.classes, resolved.className);
    for (const [name, m] of methods) {
        if (m.isPrivate && !resolved.viewerIsInside) continue;
        const item = new vscode.CompletionItem(name, vscode.CompletionItemKind.Method);
        const prefix = m.isStatic ? 'static ' : '';
        item.detail = `${prefix}func ${name}(${m.params.join(', ')})`;
        items.push(item);
    }
    for (const [name, a] of attributes) {
        if (a.isPrivate && !resolved.viewerIsInside) continue;
        const item = new vscode.CompletionItem(name, vscode.CompletionItemKind.Field);
        item.detail = a.isStatic ? 'static' : 'atributo';
        items.push(item);
    }
    return items;
}

const completionProvider = {
    provideCompletionItems(document, position) {
        const config = getConfig(document.uri);
        const line = document.lineAt(position.line).text;
        const beforeCursor = line.slice(0, position.character);

        const importMatch = /^\s*import\s+([A-Za-z_][\w.]*)?$/.exec(beforeCursor);
        if (importMatch) {
            const modulesPath = resolveModulesPath(document);
            const workspaceRoot = workspaceRootOf(document);
            return langService.listAvailableModules(modulesPath, workspaceRoot).map((name) => {
                const item = new vscode.CompletionItem(name, vscode.CompletionItemKind.Module);
                item.detail = 'modulo AvaLang';
                return item;
            });
        }

        const index = buildIndexFor(document);

        const chainInfo = getChainAndPartial(document, position);
        if (chainInfo) {
            const resolved = resolveChain(document, position, chainInfo.segments, index);
            if (resolved) return memberCompletionItems(resolved, index, config);
            return [];
        }

        const items = [];
        for (const kw of langService.PLAIN_KEYWORDS) {
            items.push(new vscode.CompletionItem(kw, vscode.CompletionItemKind.Keyword));
        }
        for (const [name, sig] of Object.entries(langService.BUILTIN_SIGNATURES)) {
            const item = new vscode.CompletionItem(name, vscode.CompletionItemKind.Function);
            item.detail = `${name}(${sig.params.join(', ')})`;
            item.documentation = new vscode.MarkdownString(sig.doc);
            items.push(item);
        }
        for (const [name, fn] of index.functions) {
            const item = new vscode.CompletionItem(name, vscode.CompletionItemKind.Function);
            item.detail = `func ${name}(${fn.params.join(', ')})`;
            items.push(item);
        }
        const alreadyHasNew = /\bnew\s+[A-Za-z_][A-Za-z0-9_]*$/.test(beforeCursor);
        for (const [name, info] of index.classes) {
            const item = new vscode.CompletionItem(name, vscode.CompletionItemKind.Class);
            item.detail = info.base ? `class ${name} : ${info.base}` : `class ${name}`;
            applyConstructorSnippet(item, config, index.classes, name, alreadyHasNew);
            items.push(item);
        }
        for (const alias of index.importedModules.keys()) {
            items.push(new vscode.CompletionItem(alias, vscode.CompletionItemKind.Module));
        }
        return items;
    },
};

function keywordHover(word) {
    const doc = langService.KEYWORD_DOCS[word];
    if (!doc) return null;
    const md = new vscode.MarkdownString();
    for (const syntax of doc.syntax) md.appendCodeblock(syntax, 'avalang');
    md.appendMarkdown(doc.doc);
    return new vscode.Hover(md);
}

function builtinHover(word) {
    const sig = langService.BUILTIN_SIGNATURES[word];
    if (!sig) return null;
    const md = new vscode.MarkdownString();
    md.appendCodeblock(`${word}(${sig.params.join(', ')})`, 'avalang');
    md.appendMarkdown(sig.doc);
    return new vscode.Hover(md);
}

const hoverProvider = {
    provideHover(document, position) {
        const range = document.getWordRangeAtPosition(position);
        if (!range) return null;
        const word = document.getText(range);

        const keyword = keywordHover(word);
        if (keyword) return keyword;

        const builtin = builtinHover(word);
        if (builtin) return builtin;

        const index = buildIndexFor(document);

        const text = document.getText();
        const wordStartOffset = document.offsetAt(range.start);
        const hasDotBefore = wordStartOffset > 0 && text[wordStartOffset - 1] === '.';
        if (hasDotBefore) {
            const segments = langService.extractChainSegments(text, wordStartOffset - 1);
            const resolved = segments ? resolveChain(document, position, segments, index) : null;
            if (resolved) {
                if (resolved.kind === 'module') {
                    if (resolved.module.functions.has(word)) {
                        const fn = resolved.module.functions.get(word);
                        const md = new vscode.MarkdownString();
                        md.appendCodeblock(`${word}(${fn.params.join(', ')})`, 'avalang');
                        return new vscode.Hover(md);
                    }
                } else {
                    const { methods, attributes } = flattenedMembers(index.classes, resolved.className);
                    if (methods.has(word)) {
                        const m = methods.get(word);
                        const md = new vscode.MarkdownString();
                        const prefix = [m.isStatic ? 'static' : null, m.isPrivate ? 'private' : null].filter(Boolean).join(' ');
                        md.appendCodeblock(`${prefix ? prefix + ' ' : ''}func ${word}(${m.params.join(', ')})`, 'avalang');
                        return new vscode.Hover(md);
                    }
                    if (attributes.has(word)) {
                        const a = attributes.get(word);
                        const md = new vscode.MarkdownString();
                        md.appendCodeblock(`${a.isStatic ? 'static ' : ''}${word}`, 'avalang');
                        return new vscode.Hover(md);
                    }
                }
            }
        }

        if (index.functions.has(word)) {
            const fn = index.functions.get(word);
            const md = new vscode.MarkdownString();
            md.appendCodeblock(`${fn.isAsync ? 'async ' : ''}func ${word}(${fn.params.join(', ')})`, 'avalang');
            return new vscode.Hover(md);
        }
        if (index.classes.has(word)) {
            const info = index.classes.get(word);
            const md = new vscode.MarkdownString();
            md.appendCodeblock(info.base ? `class ${word} : ${info.base}` : `class ${word}`, 'avalang');
            if (info.methods.size) {
                md.appendMarkdown('\n**Metodos:** ' + Array.from(info.methods.keys()).join(', '));
            }
            return new vscode.Hover(md);
        }
        return null;
    },
};

function findCallContext(text, offset) {
    let depth = 0;
    for (let i = offset - 1; i >= 0; --i) {
        const ch = text[i];
        if (ch === ')') depth++;
        else if (ch === '(') {
            if (depth === 0) {
                let j = i;
                while (j > 0 && /\s/.test(text[j - 1])) j--;
                let start = j;
                let d = 0;
                while (start > 0) {
                    const c = text[start - 1];
                    if (c === ')') { d++; start--; continue; }
                    if (c === '(') { if (d === 0) break; d--; start--; continue; }
                    if (d > 0) { start--; continue; }
                    if (/[A-Za-z0-9_.]/.test(c)) { start--; continue; }
                    break;
                }
                const chain = text.slice(start, j);
                return chain || null;
            }
            depth--;
        }
    }
    return null;
}

function signatureFromParams(name, params) {
    const info = new vscode.SignatureInformation(`${name}(${params.join(', ')})`);
    info.parameters = params.map((p) => new vscode.ParameterInformation(p));
    return info;
}

const signatureHelpProvider = {
    provideSignatureHelp(document, position) {
        const offset = document.offsetAt(position);
        const text = document.getText();
        const callee = findCallContext(text, offset);
        if (!callee) return null;

        const index = buildIndexFor(document);
        let signature = null;

        if (callee.includes('.')) {
            const segments = langService.splitChainString(callee);
            const memberName = segments.pop();
            const resolved = resolveChain(document, position, segments, index);
            if (resolved) {
                if (resolved.kind === 'module' && resolved.module.functions.has(memberName)) {
                    signature = signatureFromParams(memberName, resolved.module.functions.get(memberName).params);
                } else if (resolved.kind === 'class') {
                    const { methods } = flattenedMembers(index.classes, resolved.className);
                    if (methods.has(memberName)) signature = signatureFromParams(memberName, methods.get(memberName).params);
                }
            }
        } else if (langService.BUILTIN_SIGNATURES[callee]) {
            signature = signatureFromParams(callee, langService.BUILTIN_SIGNATURES[callee].params);
        } else if (index.functions.has(callee)) {
            signature = signatureFromParams(callee, index.functions.get(callee).params);
        } else if (index.classes.has(callee)) {
            const { methods } = flattenedMembers(index.classes, callee);
            const ctor = methods.get(callee);
            signature = signatureFromParams(callee, ctor ? ctor.params : []);
        }

        if (!signature) return null;
        const help = new vscode.SignatureHelp();
        help.signatures = [signature];
        help.activeSignature = 0;
        help.activeParameter = 0;
        return help;
    },
};

module.exports = {
    completionProvider,
    hoverProvider,
    signatureHelpProvider,
    buildIndexFor,
    resolveChain,
    flattenedMembers,
    getChainAndPartial,
    workspaceRootOf,
    resolveModulesPath,
    inferVariableClass,
    constructorParams,
    buildConstructorSnippet,
};
