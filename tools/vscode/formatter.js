const vscode = require('vscode');
const langService = require('./language_service');

function getConfig(resource) {
    return vscode.workspace.getConfiguration('avalang', resource);
}

function indentUnit(options) {
    return options.insertSpaces ? ' '.repeat(options.tabSize) : '\t';
}

function formatLines(lines, unit) {
    const stack = [];
    let currentDepth = 0;
    const output = new Array(lines.length);

    for (let i = 0; i < lines.length; ++i) {
        const line = lines[i];
        const trimmed = line.replace(/^[ \t]+/, '').replace(/[ \t]+$/, '');

        if (!trimmed) {
            output[i] = '';
            continue;
        }

        const info = langService.classifyBlockLine(line);

        if (info.kind === 'end') {
            const frame = stack.pop();
            currentDepth = frame ? frame.openDepth : Math.max(0, currentDepth - 1);
            output[i] = unit.repeat(currentDepth) + trimmed;
            continue;
        }

        if (info.kind === 'clause' && stack.length) {
            const frame = stack[stack.length - 1];
            output[i] = unit.repeat(frame.clauseDepth) + trimmed;
            currentDepth = frame.clauseDepth + 1;
            continue;
        }

        output[i] = unit.repeat(currentDepth) + trimmed;

        if (info.kind === 'open') {
            const insideExtern = stack.length && stack[stack.length - 1].keyword === 'extern';
            const isExternDeclaration = info.keyword === 'func' && insideExtern;
            if (!isExternDeclaration) {
                const offset = langService.CLAUSE_DEPTH_OFFSETS[info.keyword] || 0;
                stack.push({ keyword: info.keyword, openDepth: currentDepth, clauseDepth: currentDepth + offset });
                currentDepth += 1;
            }
        }
    }

    return output;
}

const documentFormattingProvider = {
    provideDocumentFormattingEdits(document, options) {
        const config = getConfig(document.uri);
        if (!config.get('format.enable', true)) return [];

        const unit = indentUnit(options);
        const lineCount = document.lineCount;
        const rawLines = [];
        for (let i = 0; i < lineCount; ++i) rawLines.push(document.lineAt(i).text);

        const formatted = formatLines(rawLines, unit);
        const edits = [];
        for (let i = 0; i < lineCount; ++i) {
            if (formatted[i] === rawLines[i]) continue;
            edits.push(vscode.TextEdit.replace(document.lineAt(i).range, formatted[i]));
        }
        return edits;
    },
};

module.exports = { documentFormattingProvider, formatLines };
