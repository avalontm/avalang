const vscode = require('vscode');
const langService = require('./language_service');

function lineRange(lines, startLine, endLine) {
    const last = endLine != null ? endLine : startLine;
    const clampedLast = Math.min(Math.max(last, startLine), lines.length - 1);
    const lastLineText = lines[clampedLast] || '';
    return new vscode.Range(startLine, 0, clampedLast, lastLineText.length);
}

function selectionRangeFor(lines, line, name) {
    const text = lines[line] || '';
    const col = Math.max(0, text.indexOf(name));
    return new vscode.Range(line, col, line, col + name.length);
}

function modifiersPrefix(info) {
    const mods = [];
    if (info.isStatic) mods.push('static');
    if (info.isPrivate) mods.push('private');
    if (info.isAsync) mods.push('async');
    return mods.length ? mods.join(' ') + ' ' : '';
}

function functionDetail(info) {
    const params = '(' + info.params.join(', ') + ')';
    const ret = info.returnType ? ' as ' + info.returnType : '';
    return modifiersPrefix(info) + params + ret;
}

function functionSymbol(name, info, lines, kind) {
    const range = lineRange(lines, info.line, info.endLine);
    const selectionRange = selectionRangeFor(lines, info.line, name);
    return new vscode.DocumentSymbol(name, functionDetail(info), kind, range, selectionRange);
}

function attributeSymbol(name, info, lines) {
    const range = selectionRangeFor(lines, info.line, name);
    const detail = ((info.isStatic ? 'static ' : '') + (info.isPrivate ? 'private ' : '') + (info.type || '')).trim();
    const kind = info.isStatic ? vscode.SymbolKind.Property : vscode.SymbolKind.Field;
    return new vscode.DocumentSymbol(name, detail, kind, range, range);
}

function classSymbol(name, info, lines) {
    const range = lineRange(lines, info.line, info.endLine);
    const selectionRange = selectionRangeFor(lines, info.line, name);
    const detail = info.base ? ': ' + info.base : '';
    const symbol = new vscode.DocumentSymbol(name, detail, vscode.SymbolKind.Class, range, selectionRange);

    const members = [];
    for (const [methodName, methodInfo] of info.methods) {
        members.push({ line: methodInfo.line, symbol: functionSymbol(methodName, methodInfo, lines, vscode.SymbolKind.Method) });
    }
    for (const [attrName, attrInfo] of info.attributes) {
        members.push({ line: attrInfo.line, symbol: attributeSymbol(attrName, attrInfo, lines) });
    }
    members.sort((a, b) => a.line - b.line);
    symbol.children = members.map((m) => m.symbol);
    return symbol;
}

const documentSymbolProvider = {
    provideDocumentSymbols(document) {
        const text = document.getText();
        const lines = text.split(/\r\n|\r|\n/);
        const parsed = langService.parseFileText(text);

        const topLevel = [];
        for (const [name, info] of parsed.functions) {
            topLevel.push({ line: info.line, symbol: functionSymbol(name, info, lines, vscode.SymbolKind.Function) });
        }
        for (const [name, info] of parsed.classes) {
            topLevel.push({ line: info.line, symbol: classSymbol(name, info, lines) });
        }
        topLevel.sort((a, b) => a.line - b.line);
        return topLevel.map((entry) => entry.symbol);
    },
};

const foldingRangeProvider = {
    provideFoldingRanges(document) {
        const ranges = langService.collectBlockRanges(document.getText());
        return ranges.map((r) => new vscode.FoldingRange(r.startLine, r.endLine, vscode.FoldingRangeKind.Region));
    },
};

module.exports = { documentSymbolProvider, foldingRangeProvider };
