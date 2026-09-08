const vscode = require('vscode');
const { execFile } = require('child_process');
const fs = require('fs');
const path = require('path');
const os = require('os');

function getConfig(resource) {
    return vscode.workspace.getConfiguration('avalang', resource);
}

function resolveVariables(value, fileUri) {
    if (!value) return value;
    const filePath = fileUri.fsPath;
    const fileDir = path.dirname(filePath);
    const workspaceFolder = vscode.workspace.getWorkspaceFolder(fileUri);
    const workspacePath = workspaceFolder ? workspaceFolder.uri.fsPath : fileDir;
    return value
        .replace(/\$\{workspaceFolder\}/g, workspacePath)
        .replace(/\$\{fileDirname\}/g, fileDir)
        .replace(/\$\{fileBasenameNoExtension\}/g, path.basename(filePath, path.extname(filePath)))
        .replace(/\$\{fileBasename\}/g, path.basename(filePath))
        .replace(/\$\{file\}/g, filePath);
}

const ERROR_WITH_COL = /error at .+?:(\d+):(\d+):\s*(.*)/;
const ERROR_LINE_ONLY = /error at .+?:(\d+):\s*(.*)/;
const ERROR_PLAIN = /^(?:compile|runtime) error:\s*(.*)/;

function parseErrors(stderrText) {
    const results = [];
    for (const rawLine of stderrText.split(/\r?\n/)) {
        const line = rawLine.trim();
        if (!line) continue;
        let match = ERROR_WITH_COL.exec(line);
        if (match) {
            results.push({ line: parseInt(match[1], 10), column: parseInt(match[2], 10), message: match[3] });
            continue;
        }
        match = ERROR_LINE_ONLY.exec(line);
        if (match) {
            results.push({ line: parseInt(match[1], 10), column: 1, message: match[2] });
            continue;
        }
        match = ERROR_PLAIN.exec(line);
        if (match) {
            results.push({ line: 1, column: 1, message: match[1] });
        }
    }
    return results;
}

function createDiagnosticsManager(context) {
    const collection = vscode.languages.createDiagnosticCollection('avalang');
    context.subscriptions.push(collection);

    const timers = new Map();
    const tempFiles = new Set();

    function cleanupTempFiles() {
        for (const p of tempFiles) {
            try { fs.unlinkSync(p); } catch (e) {}
        }
        tempFiles.clear();
    }
    context.subscriptions.push({ dispose: cleanupTempFiles });

    function runCheck(document, checkPath, cwd) {
        const config = getConfig(document.uri);
        const executablePath = config.get('executablePath') || 'ava_cli';
        const resolvedExecutable = resolveVariables(executablePath, document.uri);
        const modulesPath = config.get('modulesPath') || '';
        const args = [];
        if (modulesPath) args.push('--modules', resolveVariables(modulesPath, document.uri));
        args.push('--check', checkPath);

        execFile(resolvedExecutable, args, { cwd, timeout: 10000 }, (err, stdout, stderr) => {
            const diagnostics = [];
            const output = (stderr || '') + '\n' + (stdout || '');

            if (err && err.code === 'ENOENT') return;

            for (const found of parseErrors(output)) {
                const line = Math.max(0, found.line - 1);
                const col = Math.max(0, found.column - 1);
                let endCol = col + 20;
                try {
                    const lineText = document.lineAt(Math.min(line, document.lineCount - 1)).text;
                    endCol = Math.min(lineText.length, col + 30) || lineText.length;
                    if (endCol <= col) endCol = lineText.length;
                } catch (e) {}
                const range = new vscode.Range(line, col, line, Math.max(endCol, col + 1));
                diagnostics.push(new vscode.Diagnostic(range, found.message, vscode.DiagnosticSeverity.Error));
            }
            collection.set(document.uri, diagnostics);
        });
    }

    function checkNow(document) {
        if (document.languageId !== 'avalang') return;
        const config = getConfig(document.uri);
        if (!config.get('diagnostics.enable', true)) {
            collection.delete(document.uri);
            return;
        }

        if (!document.isDirty) {
            runCheck(document, document.uri.fsPath, path.dirname(document.uri.fsPath));
            return;
        }

        const dir = path.dirname(document.uri.fsPath);
        const base = path.basename(document.uri.fsPath, path.extname(document.uri.fsPath));
        const tempPath = path.join(dir, `.${base}.avacheck.${process.pid}.ava`);
        try {
            fs.writeFileSync(tempPath, document.getText(), 'utf8');
            tempFiles.add(tempPath);
        } catch (e) {
            const fallbackPath = path.join(os.tmpdir(), `avalang-check-${process.pid}-${Date.now()}.ava`);
            try {
                fs.writeFileSync(fallbackPath, document.getText(), 'utf8');
                tempFiles.add(fallbackPath);
            } catch (e2) { return; }
            runCheck(document, fallbackPath, dir);
            setTimeout(() => { try { fs.unlinkSync(fallbackPath); tempFiles.delete(fallbackPath); } catch (e3) {} }, 5000);
            return;
        }
        runCheck(document, tempPath, dir);
        setTimeout(() => { try { fs.unlinkSync(tempPath); tempFiles.delete(tempPath); } catch (e3) {} }, 5000);
    }

    function scheduleCheck(document) {
        if (document.languageId !== 'avalang') return;
        const config = getConfig(document.uri);
        const runOn = config.get('diagnostics.runOn', 'onSave');
        if (runOn === 'off') {
            collection.delete(document.uri);
            return;
        }
        if (runOn === 'onSave' && document.isDirty) return;

        const key = document.uri.toString();
        clearTimeout(timers.get(key));
        const debounceMs = runOn === 'onType' ? config.get('diagnostics.debounceMs', 600) : 0;
        timers.set(key, setTimeout(() => checkNow(document), debounceMs));
    }

    context.subscriptions.push(
        vscode.workspace.onDidSaveTextDocument((doc) => scheduleCheck(doc)),
        vscode.workspace.onDidOpenTextDocument((doc) => scheduleCheck(doc)),
        vscode.workspace.onDidChangeTextDocument((e) => scheduleCheck(e.document)),
        vscode.workspace.onDidCloseTextDocument((doc) => collection.delete(doc.uri))
    );

    for (const doc of vscode.workspace.textDocuments) scheduleCheck(doc);

    return collection;
}

module.exports = { createDiagnosticsManager, parseErrors };
