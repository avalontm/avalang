const vscode = require('vscode');
const langService = require('./language_service');
const { buildIndexFor, resolveChain, flattenedMembers } = require('./providers');

function getConfig(resource) {
    return vscode.workspace.getConfiguration('avalang', resource);
}

function splitTopLevelArgs(text) {
    const args = [];
    let depth = 0;
    let inString = null;
    let start = 0;
    for (let i = 0; i < text.length; ++i) {
        const ch = text[i];
        if (inString) {
            if (ch === '\\') { i++; continue; }
            if (ch === inString) inString = null;
            continue;
        }
        if (ch === '"' || ch === "'") { inString = ch; continue; }
        if (ch === '(' || ch === '[' || ch === '{') { depth++; continue; }
        if (ch === ')' || ch === ']' || ch === '}') { depth--; continue; }
        if (ch === ',' && depth === 0) {
            args.push({ text: text.slice(start, i), offset: start });
            start = i + 1;
        }
    }
    args.push({ text: text.slice(start), offset: start });
    return args.filter((a) => a.text.trim().length > 0);
}

function findMatchingCloseParen(text, openParenIndex) {
    let depth = 0;
    for (let i = openParenIndex; i < text.length; ++i) {
        if (text[i] === '(') depth++;
        else if (text[i] === ')') {
            depth--;
            if (depth === 0) return i;
        }
    }
    return -1;
}

function maskFuncDeclarations(line) {
    const span = langService.funcDeclarationSpan(line);
    if (!span) return line;
    return line.slice(0, span.nameIndex) +
        ' '.repeat(span.closeParenIndex - span.nameIndex + 1) +
        line.slice(span.closeParenIndex + 1);
}

function paramName(rawParam) {
    const cleaned = rawParam.replace(/^\*/, '');
    const match = /^([A-Za-z_]\w*)/.exec(cleaned.trim());
    return match ? match[1] : null;
}

function calleeParams(document, position, calleeChain, index) {
    const parts = calleeChain.split('.');
    if (parts.length === 1) {
        const name = parts[0];
        if (langService.PLAIN_KEYWORDS.includes(name)) return null;
        if (langService.BUILTIN_SIGNATURES[name]) return langService.BUILTIN_SIGNATURES[name].params;
        if (index.functions.has(name)) return index.functions.get(name).params;
        if (index.classes.has(name)) {
            const { methods } = flattenedMembers(index.classes, name);
            const ctor = methods.get(name);
            return ctor ? ctor.params : [];
        }
        return null;
    }
    const headSegments = parts.slice(0, -1);
    const methodName = parts[parts.length - 1];
    const resolved = resolveChain(document, position, headSegments, index);
    if (!resolved) return null;
    if (resolved.kind === 'module') {
        return resolved.module.functions.has(methodName) ? resolved.module.functions.get(methodName).params : null;
    }
    const { methods } = flattenedMembers(index.classes, resolved.className);
    return methods.has(methodName) ? methods.get(methodName).params : null;
}

const RE_CALL_SITE = /([A-Za-z_]\w*(?:\.[A-Za-z_]\w*)*)\s*\(/g;

function parameterHintsForLine(document, index, lineNumber) {
    const rawLine = document.lineAt(lineNumber).text;
    const scanLine = maskFuncDeclarations(rawLine);
    const hints = [];

    let match;
    RE_CALL_SITE.lastIndex = 0;
    while ((match = RE_CALL_SITE.exec(scanLine)) !== null) {
        const calleeChain = match[1];
        const openParenIndex = match.index + match[0].length - 1;
        const closeParenIndex = findMatchingCloseParen(scanLine, openParenIndex);
        if (closeParenIndex === -1) continue;

        const position = new vscode.Position(lineNumber, match.index);
        const params = calleeParams(document, position, calleeChain, index);
        if (!params || !params.length) continue;

        const argsText = rawLine.slice(openParenIndex + 1, closeParenIndex);
        const args = splitTopLevelArgs(argsText);

        for (let i = 0; i < args.length && i < params.length; ++i) {
            const name = paramName(params[i]);
            if (!name) continue;
            if (params[i].trim().startsWith('*')) break;
            const argText = args[i].text.trim();
            if (/^[A-Za-z_]\w*\s*=(?!=)/.test(argText)) continue;
            if (new RegExp('^' + name + '$', 'i').test(argText)) continue;

            const argStart = openParenIndex + 1 + args[i].offset;
            const leadingWhitespace = args[i].text.length - args[i].text.trimStart().length;
            const hintPosition = new vscode.Position(lineNumber, argStart + leadingWhitespace);
            const hint = new vscode.InlayHint(hintPosition, name + ':', vscode.InlayHintKind.Parameter);
            hint.paddingRight = true;
            hints.push(hint);
        }
    }
    return hints;
}

function typeHintsForLine(document, lineNumber, index) {
    const line = document.lineAt(lineNumber).text;
    const attr = langService.parseAttributeLine(line);
    if (!attr || !attr.type || attr.typeExplicit) return [];
    if (!index.classes.has(attr.type)) return [];
    const nameCol = line.indexOf(attr.name);
    if (nameCol === -1) return [];
    const position = new vscode.Position(lineNumber, nameCol + attr.name.length);
    const hint = new vscode.InlayHint(position, ': ' + attr.type, vscode.InlayHintKind.Type);
    return [hint];
}

function returnTypeHintsForLine(document, lineNumber, entry) {
    if (!entry || entry.returnTypeExplicit || !entry.returnType) return [];
    const line = document.lineAt(lineNumber).text;
    const span = langService.funcDeclarationSpan(line);
    if (!span) return [];
    const position = new vscode.Position(lineNumber, span.closeParenIndex + 1);
    const hint = new vscode.InlayHint(position, ' as ' + entry.returnType, vscode.InlayHintKind.Type);
    return [hint];
}

function collectReturnTypeHints(document) {
    const parsed = langService.parseFileText(document.getText());
    const hints = [];
    for (const [, entry] of parsed.functions) {
        hints.push(...returnTypeHintsForLine(document, entry.line, entry));
    }
    for (const [, cls] of parsed.classes) {
        for (const [, entry] of cls.methods) {
            hints.push(...returnTypeHintsForLine(document, entry.line, entry));
        }
    }
    return hints;
}

const inlayHintsProvider = {
    provideInlayHints(document, range) {
        const config = getConfig(document.uri);
        if (!config.get('inlayHints.enable', true)) return [];
        const showParameterNames = config.get('inlayHints.parameterNames', true);
        const showTypes = config.get('inlayHints.types', true);

        const hints = [];
        const index = (showParameterNames || showTypes) ? buildIndexFor(document) : null;

        for (let line = range.start.line; line <= range.end.line && line < document.lineCount; ++line) {
            if (showTypes) hints.push(...typeHintsForLine(document, line, index));
            if (showParameterNames) hints.push(...parameterHintsForLine(document, index, line));
        }
        if (showTypes) {
            for (const hint of collectReturnTypeHints(document)) {
                if (hint.position.line >= range.start.line && hint.position.line <= range.end.line) hints.push(hint);
            }
        }
        return hints;
    },
};

module.exports = { inlayHintsProvider };
