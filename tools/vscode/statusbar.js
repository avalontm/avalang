const vscode = require('vscode');
const { execFile } = require('child_process');
const path = require('path');

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

function resolveExecutable(document) {
    const config = getConfig(document.uri);
    const executablePath = config.get('executablePath') || 'ava_cli';
    return resolveVariables(executablePath, document.uri);
}

function createStatusBarManager(context) {
    const item = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Right, 100);
    item.name = 'AvaLang';
    item.command = 'avalang.showRuntimeInfo';
    context.subscriptions.push(item);

    const versionCache = new Map();

    function fetchVersion(resolvedExecutable, cwd) {
        if (versionCache.has(resolvedExecutable)) {
            return Promise.resolve(versionCache.get(resolvedExecutable));
        }
        return new Promise((resolve) => {
            execFile(resolvedExecutable, ['--version'], { cwd, timeout: 5000 }, (err, stdout, stderr) => {
                if (err) {
                    versionCache.set(resolvedExecutable, null);
                    resolve(null);
                    return;
                }
                const output = ((stdout || '') + '\n' + (stderr || '')).trim();
                const match = /AvaLang\s+(\S+)/.exec(output);
                const info = { version: match ? match[1] : null, banner: output };
                versionCache.set(resolvedExecutable, info);
                resolve(info);
            });
        });
    }

    async function refresh() {
        const editor = vscode.window.activeTextEditor;
        if (!editor || editor.document.languageId !== 'avalang') {
            item.hide();
            return;
        }

        const document = editor.document;
        const resolvedExecutable = resolveExecutable(document);
        const cwd = path.dirname(document.uri.fsPath);

        item.text = '$(circuit-board) AvaLang...';
        item.tooltip = 'Resolviendo la version de ava_cli...';
        item.show();

        const info = await fetchVersion(resolvedExecutable, cwd);
        if (!editor || vscode.window.activeTextEditor !== editor) return;

        if (!info || !info.version) {
            item.text = '$(warning) AvaLang';
            item.tooltip = `No se pudo ejecutar "${resolvedExecutable} --version".`;
            return;
        }

        item.text = `$(circuit-board) AvaLang v${info.version}`;
        item.tooltip = `${info.banner}\n\nClick para ver el detalle completo.`;
    }

    context.subscriptions.push(
        vscode.commands.registerCommand('avalang.showRuntimeInfo', async () => {
            const editor = vscode.window.activeTextEditor;
            if (!editor || editor.document.languageId !== 'avalang') return;
            const resolvedExecutable = resolveExecutable(editor.document);
            const cwd = path.dirname(editor.document.uri.fsPath);
            versionCache.delete(resolvedExecutable);
            const info = await fetchVersion(resolvedExecutable, cwd);
            if (info && info.banner) {
                vscode.window.showInformationMessage(info.banner);
            } else {
                vscode.window.showErrorMessage(`AvaLang: no se pudo ejecutar "${resolvedExecutable} --version".`);
            }
            refresh();
        })
    );

    context.subscriptions.push(
        vscode.window.onDidChangeActiveTextEditor(() => refresh()),
        vscode.workspace.onDidChangeConfiguration((e) => {
            if (e.affectsConfiguration('avalang.executablePath') || e.affectsConfiguration('avalang.workingDirectory')) {
                versionCache.clear();
                refresh();
            }
        })
    );

    refresh();

    return item;
}

module.exports = { createStatusBarManager };
