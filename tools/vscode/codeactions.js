const vscode = require('vscode');
const langService = require('./language_service');
const { buildIndexFor, flattenedMembers } = require('./providers');

const LINT_SOURCE = 'avalang-lint';

function computeLintDiagnostics(document) {
    const text = document.getText();
    const diagnostics = [];

    for (const issue of langService.findMissingThen(text)) {
        const range = new vscode.Range(issue.line, issue.column, issue.line, issue.column);
        const diagnostic = new vscode.Diagnostic(range, "Falta 'then' al final de esta linea.", vscode.DiagnosticSeverity.Warning);
        diagnostic.source = LINT_SOURCE;
        diagnostic.code = 'missing-then';
        diagnostics.push(diagnostic);
    }

    const { unclosed, unmatchedEnds } = langService.analyzeBlocks(text);
    if (unclosed.length) {
        const lastLine = document.lineCount - 1;
        const lastLineLength = document.lineAt(lastLine).text.length;
        const range = new vscode.Range(lastLine, 0, lastLine, lastLineLength);
        const names = unclosed.map((b) => b.keyword).join(', ');
        const diagnostic = new vscode.Diagnostic(range, `Faltan ${unclosed.length} 'end' por cerrar (${names}).`, vscode.DiagnosticSeverity.Warning);
        diagnostic.source = LINT_SOURCE;
        diagnostic.code = 'missing-end';
        diagnostics.push(diagnostic);
    }
    for (const extra of unmatchedEnds) {
        const lineText = document.lineAt(extra.line).text;
        const range = new vscode.Range(extra.line, 0, extra.line, lineText.length);
        const diagnostic = new vscode.Diagnostic(range, "Este 'end' no cierra ningun bloque abierto.", vscode.DiagnosticSeverity.Warning);
        diagnostic.source = LINT_SOURCE;
        diagnostic.code = 'extra-end';
        diagnostics.push(diagnostic);
    }

    return diagnostics;
}

function createLintManager(context) {
    const collection = vscode.languages.createDiagnosticCollection(LINT_SOURCE);
    context.subscriptions.push(collection);

    const timers = new Map();

    function refresh(document) {
        if (document.languageId !== 'avalang') return;
        collection.set(document.uri, computeLintDiagnostics(document));
    }

    function scheduleRefresh(document) {
        if (document.languageId !== 'avalang') return;
        const key = document.uri.toString();
        clearTimeout(timers.get(key));
        timers.set(key, setTimeout(() => refresh(document), 300));
    }

    context.subscriptions.push(
        vscode.workspace.onDidOpenTextDocument((doc) => refresh(doc)),
        vscode.workspace.onDidChangeTextDocument((e) => scheduleRefresh(e.document)),
        vscode.workspace.onDidCloseTextDocument((doc) => collection.delete(doc.uri))
    );

    for (const doc of vscode.workspace.textDocuments) refresh(doc);

    return collection;
}

function fixMissingThen(document, diagnostic) {
    const edit = new vscode.WorkspaceEdit();
    edit.insert(document.uri, diagnostic.range.start, ' then');
    const action = new vscode.CodeAction("Agregar 'then'", vscode.CodeActionKind.QuickFix);
    action.edit = edit;
    action.diagnostics = [diagnostic];
    return action;
}

function fixMissingEnd(document) {
    const text = document.getText();
    const { unclosed } = langService.analyzeBlocks(text);
    if (!unclosed.length) return null;
    const lastLine = document.lineCount - 1;
    const lastLineText = document.lineAt(lastLine).text;
    const insertPosition = new vscode.Position(lastLine, lastLineText.length);
    const pieces = unclosed
        .slice()
        .reverse()
        .map((block) => '\n' + ' '.repeat(block.indent) + 'end');
    const edit = new vscode.WorkspaceEdit();
    edit.insert(document.uri, insertPosition, pieces.join(''));
    const label = unclosed.length === 1 ? "Agregar 'end' faltante" : `Agregar ${unclosed.length} 'end' faltantes`;
    const action = new vscode.CodeAction(label, vscode.CodeActionKind.QuickFix);
    action.edit = edit;
    return action;
}

function fixExtraEnd(document, diagnostic) {
    const line = diagnostic.range.start.line;
    const edit = new vscode.WorkspaceEdit();
    edit.delete(document.uri, document.lineAt(line).rangeIncludingLineBreak);
    const action = new vscode.CodeAction("Quitar 'end' sobrante", vscode.CodeActionKind.QuickFix);
    action.edit = edit;
    action.diagnostics = [diagnostic];
    return action;
}

function extractQuotedIdentifier(message) {
    const match = /["']([A-Za-z_]\w*)["']/.exec(message);
    return match ? match[1] : null;
}

function symbolPool(document, line) {
    const index = buildIndexFor(document);
    const names = new Set(langService.PLAIN_KEYWORDS);
    for (const name of Object.keys(langService.BUILTIN_SIGNATURES)) names.add(name);
    for (const name of index.functions.keys()) names.add(name);
    for (const name of index.classes.keys()) names.add(name);
    for (const name of index.importedModules.keys()) names.add(name);
    const enclosingClass = langService.enclosingClassAt(document.getText(), line);
    if (enclosingClass && index.classes.has(enclosingClass)) {
        const { methods, attributes } = flattenedMembers(index.classes, enclosingClass);
        for (const name of methods.keys()) names.add(name);
        for (const name of attributes.keys()) names.add(name);
    }
    return Array.from(names);
}

function fixesForUnknownIdentifier(document, diagnostic) {
    const identifier = extractQuotedIdentifier(diagnostic.message);
    if (!identifier) return [];
    const line = diagnostic.range.start.line;
    const lineText = document.lineAt(line).text;
    const wordRe = new RegExp('\\b' + identifier.replace(/[.*+?^${}()|[\]\\]/g, '\\$&') + '\\b');
    const wordMatch = wordRe.exec(lineText);
    if (!wordMatch) return [];
    const range = new vscode.Range(line, wordMatch.index, line, wordMatch.index + identifier.length);

    const candidates = langService.closestNames(identifier, symbolPool(document, line), 2, 3);
    return candidates.map((candidate) => {
        const edit = new vscode.WorkspaceEdit();
        edit.replace(document.uri, range, candidate);
        const action = new vscode.CodeAction(`Reemplazar por '${candidate}'`, vscode.CodeActionKind.QuickFix);
        action.edit = edit;
        action.diagnostics = [diagnostic];
        return action;
    });
}

function capitalize(name) {
    return name.charAt(0).toUpperCase() + name.slice(1);
}

function accessorActions(document, position) {
    const text = document.getText();
    const line = position.line;
    const block = langService.directEnclosingBlockAt(text, line);
    if (!block || block.type !== 'class') return [];
    const attr = langService.parseAttributeLine(document.lineAt(line).text);
    if (!attr) return [];

    const receiver = attr.isStatic ? block.name : 'this';
    const accessorName = capitalize(attr.name);
    const indent = ' '.repeat(block.indent);
    const body = indent + '\n' +
        indent + `func Get${accessorName}()${attr.type ? ' as ' + attr.type : ''}\n` +
        indent + `    return ${receiver}.${attr.name}\n` +
        indent + `end\n` +
        indent + `\n` +
        indent + `func Set${accessorName}(value${attr.type ? ' as ' + attr.type : ''})\n` +
        indent + `    ${receiver}.${attr.name} = value\n` +
        indent + `end`;

    const lineText = document.lineAt(line).text;
    const insertPosition = new vscode.Position(line, lineText.length);
    const edit = new vscode.WorkspaceEdit();
    edit.insert(document.uri, insertPosition, body);
    const action = new vscode.CodeAction(`Generar Get${accessorName}/Set${accessorName}`, vscode.CodeActionKind.RefactorExtract);
    action.edit = edit;
    return [action];
}

const codeActionProvider = {
    provideCodeActions(document, range, context) {
        const actions = [];
        for (const diagnostic of context.diagnostics) {
            if (diagnostic.source === LINT_SOURCE) {
                if (diagnostic.code === 'missing-then') actions.push(fixMissingThen(document, diagnostic));
                else if (diagnostic.code === 'missing-end') {
                    const action = fixMissingEnd(document);
                    if (action) actions.push(action);
                } else if (diagnostic.code === 'extra-end') actions.push(fixExtraEnd(document, diagnostic));
                continue;
            }
            if (diagnostic.severity === vscode.DiagnosticSeverity.Error) {
                actions.push(...fixesForUnknownIdentifier(document, diagnostic));
            }
        }
        actions.push(...accessorActions(document, range.start));
        return actions;
    },
};

module.exports = { codeActionProvider, createLintManager };
