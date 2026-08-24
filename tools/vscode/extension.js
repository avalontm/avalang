// AvaLang for VS Code -- run support.
//
// This is the one file in the extension that isn't pure declarative JSON.
// It wires an actual "Run" command (play button + F5) that shells out to
// ava_cli, the same way you'd run it from a terminal:
//   ava_cli [--modules <dir>] <script.ava> [extra args...]
// (see runtime/avacli/src/main.cpp -- there's no separate "run" subcommand,
// `build` is the only subcommand and it's for packaging, not for this).
//
// Everything the user can configure (which ava_cli binary, which working
// directory, extra --modules override, extra script args) lives under the
// "avalang.*" settings contributed in package.json, scoped as "resource" so
// they can be set per-workspace/per-folder, not just globally.

const vscode = require('vscode');
const path = require('path');

// One shared terminal, reused across runs (recreated if the user closes it),
// same UX as most "run current file" extensions.
let terminal;

function getConfig(resource) {
    return vscode.workspace.getConfiguration('avalang', resource);
}

// Expands the handful of VS Code-style path variables our settings support.
// Deliberately a small subset (not the full launch.json variable set) --
// just enough to point at a binary/working dir relative to the workspace or
// the file being run.
function resolveVariables(value, fileUri) {
    if (!value) return value;

    const filePath = fileUri.fsPath;
    const fileDir = path.dirname(filePath);
    const fileBase = path.basename(filePath);
    const fileBaseNoExt = path.basename(filePath, path.extname(filePath));
    const workspaceFolder = vscode.workspace.getWorkspaceFolder(fileUri);
    const workspacePath = workspaceFolder ? workspaceFolder.uri.fsPath : fileDir;

    return value
        .replace(/\$\{workspaceFolder\}/g, workspacePath)
        .replace(/\$\{fileDirname\}/g, fileDir)
        .replace(/\$\{fileBasenameNoExtension\}/g, fileBaseNoExt)
        .replace(/\$\{fileBasename\}/g, fileBase)
        .replace(/\$\{file\}/g, filePath);
}

// Quotes a path/arg only if it actually needs it, so simple values in the
// terminal stay readable instead of always wrapped in quotes.
function quoteIfNeeded(value) {
    if (value === '') return '""';
    if (/\s/.test(value) && !(value.startsWith('"') && value.endsWith('"'))) {
        return `"${value}"`;
    }
    return value;
}

function ensureTerminal() {
    if (!terminal || terminal.exitStatus !== undefined) {
        terminal = vscode.window.createTerminal('AvaLang');
    }
    return terminal;
}

async function runFile(uriArg) {
    const editor = vscode.window.activeTextEditor;
    const targetUri = uriArg instanceof vscode.Uri ? uriArg : (editor && editor.document.uri);

    if (!targetUri) {
        vscode.window.showErrorMessage('AvaLang: no hay ningun archivo .ava activo para ejecutar.');
        return;
    }

    // Save first so ava_cli always runs what's actually on disk, not a stale
    // version -- same expectation as VS Code's own "Run" for other languages.
    const dirtyEditor = vscode.window.visibleTextEditors.find(
        (e) => e.document.uri.toString() === targetUri.toString() && e.document.isDirty
    );
    if (dirtyEditor) {
        await dirtyEditor.document.save();
    }

    const config = getConfig(targetUri);
    const executablePath = config.get('executablePath') || 'ava_cli';
    const workingDirectorySetting = config.get('workingDirectory') || '${fileDirname}';
    const modulesPath = config.get('modulesPath') || '';
    const extraArgs = config.get('args') || [];
    const clearBeforeRun = config.get('clearTerminalBeforeRun', true);

    const resolvedExecutable = resolveVariables(executablePath, targetUri);
    const resolvedCwd = resolveVariables(workingDirectorySetting, targetUri) || path.dirname(targetUri.fsPath);
    const resolvedModules = modulesPath ? resolveVariables(modulesPath, targetUri) : '';

    const commandParts = [quoteIfNeeded(resolvedExecutable)];
    if (resolvedModules) {
        commandParts.push('--modules', quoteIfNeeded(resolvedModules));
    }
    commandParts.push(quoteIfNeeded(targetUri.fsPath));
    if (Array.isArray(extraArgs) && extraArgs.length > 0) {
        commandParts.push(...extraArgs.map((arg) => quoteIfNeeded(String(arg))));
    }

    const term = ensureTerminal();
    term.show(true);
    if (clearBeforeRun) {
        await vscode.commands.executeCommand('workbench.action.terminal.clear');
    }
    term.sendText(`cd ${quoteIfNeeded(resolvedCwd)}`);
    term.sendText(commandParts.join(' '));
}

function activate(context) {
    context.subscriptions.push(
        vscode.commands.registerCommand('avalang.runFile', runFile)
    );
    context.subscriptions.push(
        vscode.window.onDidCloseTerminal((closed) => {
            if (closed === terminal) terminal = undefined;
        })
    );
}

function deactivate() {
    if (terminal) terminal.dispose();
}

module.exports = { activate, deactivate };
