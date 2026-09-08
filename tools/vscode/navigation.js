const vscode = require('vscode');
const fs = require('fs');
const path = require('path');
const langService = require('./language_service');
const {
    buildIndexFor,
    resolveChain,
    flattenedMembers,
    workspaceRootOf,
    resolveModulesPath,
    inferVariableClass,
} = require('./providers');

function locationFor(filePath, line) {
    const uri = vscode.Uri.file(filePath);
    return new vscode.Location(uri, new vscode.Position(Math.max(0, line), 0));
}

function importLineAt(document, position) {
    const line = document.lineAt(position.line).text;
    const match = /^[ \t]*import\s+([A-Za-z_]\w*(?:\.[A-Za-z_]\w*)*)(?:\s+as\s+([A-Za-z_]\w*))?/.exec(line);
    if (!match) return null;
    const startCol = line.indexOf(match[1]);
    const endCol = startCol + match[1].length;
    if (position.character < startCol || position.character > endCol) return null;
    return { parts: match[1].split('.') };
}

function resolveDefinitionAtWord(document, position, index) {
    const text = document.getText();
    const range = document.getWordRangeAtPosition(position);
    if (!range) return null;
    const word = document.getText(range);

    const importInfo = importLineAt(document, position);
    if (importInfo) {
        const modulesPath = resolveModulesPath(document);
        const workspaceRoot = workspaceRootOf(document);
        const resolved = langService.resolveModuleFile(importInfo.parts, path.dirname(document.uri.fsPath), modulesPath, workspaceRoot);
        return resolved ? locationFor(resolved, 0) : null;
    }

    const wordStartOffset = document.offsetAt(range.start);
    const hasDotBefore = wordStartOffset > 0 && text[wordStartOffset - 1] === '.';
    if (hasDotBefore) {
        const segments = langService.extractChainSegments(text, wordStartOffset - 1);
        const resolved = segments ? resolveChain(document, position, segments, index) : null;
        if (!resolved) return null;
        if (resolved.kind === 'module') {
            if (resolved.module.functions.has(word)) {
                const fn = resolved.module.functions.get(word);
                return locationFor(fn.file, fn.line);
            }
            if (resolved.module.classes.has(word)) {
                const cls = resolved.module.classes.get(word);
                return locationFor(cls.file, cls.line);
            }
            return null;
        }
        const { methods, attributes } = flattenedMembers(index.classes, resolved.className);
        if (methods.has(word)) {
            const m = methods.get(word);
            return locationFor(m.file, m.line);
        }
        if (attributes.has(word)) {
            const a = attributes.get(word);
            return locationFor(a.file, a.line);
        }
        return null;
    }

    if (word === 'this' || word === 'self') {
        const className = langService.enclosingClassAt(text, position.line);
        if (!className || !index.classes.has(className)) return null;
        const cls = index.classes.get(className);
        return locationFor(cls.file, cls.line);
    }

    if (index.importedModules.has(word)) {
        return locationFor(index.importedModules.get(word).path, 0);
    }

    if (index.functions.has(word)) {
        const fn = index.functions.get(word);
        return locationFor(fn.file, fn.line);
    }
    if (index.classes.has(word)) {
        const cls = index.classes.get(word);
        return locationFor(cls.file, cls.line);
    }

    const enclosingClass = langService.enclosingClassAt(text, position.line);
    if (enclosingClass && index.classes.has(enclosingClass)) {
        const { methods, attributes } = flattenedMembers(index.classes, enclosingClass);
        if (methods.has(word)) {
            const m = methods.get(word);
            return locationFor(m.file, m.line);
        }
        if (attributes.has(word)) {
            const a = attributes.get(word);
            return locationFor(a.file, a.line);
        }
    }

    return null;
}

const definitionProvider = {
    provideDefinition(document, position) {
        const index = buildIndexFor(document);
        return resolveDefinitionAtWord(document, position, index);
    },
};

function findWordOccurrences(text, word) {
    const re = new RegExp('\\b' + word.replace(/[.*+?^${}()|[\]\\]/g, '\\$&') + '\\b', 'g');
    const lines = text.split(/\r\n|\r|\n/);
    const occurrences = [];
    for (let i = 0; i < lines.length; ++i) {
        let match;
        while ((match = re.exec(lines[i])) !== null) {
            occurrences.push({ line: i, character: match.index });
        }
    }
    return occurrences;
}

async function findAvaFiles(document) {
    const workspaceRoot = workspaceRootOf(document);
    const pattern = new vscode.RelativePattern(workspaceRoot, '**/*.ava');
    return vscode.workspace.findFiles(pattern, '**/{node_modules,dist,out}/**');
}

async function readAvaFile(document, uri) {
    if (uri.fsPath === document.uri.fsPath) return document.getText();
    try {
        return fs.readFileSync(uri.fsPath, 'utf8');
    } catch (e) {
        return null;
    }
}

const referenceProvider = {
    async provideReferences(document, position) {
        const range = document.getWordRangeAtPosition(position);
        if (!range) return [];
        const word = document.getText(range);
        const uris = await findAvaFiles(document);
        const locations = [];
        for (const uri of uris) {
            const text = await readAvaFile(document, uri);
            if (text == null) continue;
            for (const occ of findWordOccurrences(text, word)) {
                locations.push(new vscode.Location(uri, new vscode.Position(occ.line, occ.character)));
            }
        }
        return locations;
    },
};

const renameProvider = {
    prepareRename(document, position) {
        const range = document.getWordRangeAtPosition(position);
        if (!range) throw new Error('No hay ningun simbolo de AvaLang bajo el cursor.');
        return range;
    },
    async provideRenameEdits(document, position, newName) {
        if (!/^[A-Za-z_]\w*$/.test(newName)) {
            throw new Error('AvaLang: el nombre nuevo debe ser un identificador valido.');
        }
        const range = document.getWordRangeAtPosition(position);
        if (!range) return null;
        const oldName = document.getText(range);
        const uris = await findAvaFiles(document);
        const edit = new vscode.WorkspaceEdit();
        for (const uri of uris) {
            const text = await readAvaFile(document, uri);
            if (text == null) continue;
            for (const occ of findWordOccurrences(text, oldName)) {
                const start = new vscode.Position(occ.line, occ.character);
                const end = start.translate(0, oldName.length);
                edit.replace(uri, new vscode.Range(start, end), newName);
            }
        }
        return edit;
    },
};

module.exports = { definitionProvider, referenceProvider, renameProvider };
