const vscode = require('vscode');
const path = require('path');
const fs = require('fs');
const { completionProvider, hoverProvider, signatureHelpProvider } = require('./providers');
const { definitionProvider, referenceProvider, renameProvider } = require('./navigation');
const { createDiagnosticsManager } = require('./diagnostics');
const { documentSymbolProvider, foldingRangeProvider } = require('./structure');
const { createStatusBarManager } = require('./statusbar');
const { codeActionProvider, createLintManager } = require('./codeactions');
const { inlayHintsProvider } = require('./inlayhints');
const { documentFormattingProvider } = require('./formatter');

let terminal;

function getConfig(resource) {
    return vscode.workspace.getConfiguration('avalang', resource);
}

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

function looksLikeRepoRoot(dir) {
    return fs.existsSync(path.join(dir, 'CMakeLists.txt')) &&
           fs.existsSync(path.join(dir, 'runtime', 'avapack', 'CMakeLists.txt'));
}

function detectRepoRoot(startDir) {
    let dir = startDir;
    for (let i = 0; i < 8 && dir; i++) {
        if (looksLikeRepoRoot(dir)) return dir;
        const parent = path.dirname(dir);
        if (parent === dir) break;
        dir = parent;
    }
    return undefined;
}

async function buildExecutable(uriArg) {
    const editor = vscode.window.activeTextEditor;
    const targetUri = uriArg instanceof vscode.Uri ? uriArg : (editor && editor.document.uri);

    if (!targetUri) {
        vscode.window.showErrorMessage('AvaLang: no hay ningun archivo .ava activo para empaquetar.');
        return;
    }

    const dirtyEditor = vscode.window.visibleTextEditors.find(
        (e) => e.document.uri.toString() === targetUri.toString() && e.document.isDirty
    );
    if (dirtyEditor) {
        await dirtyEditor.document.save();
    }

    const target = await vscode.window.showQuickPick(
        [
            { label: 'desktop', description: 'Windows/macOS/Linux .exe (default)' },
            { label: 'barekernel', description: 'AppHeader .exe for litekernel (requires a compiler path)' },
        ],
        { placeHolder: 'AvaLang build target' }
    );
    if (!target) return;

    const config = getConfig(targetUri);
    const executablePath = config.get('executablePath') || 'ava_cli';
    const outDirSetting = config.get('build.outDir') || '${workspaceFolder}/dist';
    const repoRootSetting = config.get('build.repoRoot') || '';
    const compilerPathSetting = target.label === 'barekernel'
        ? (config.get('build.compilerPathBarekernel') || '')
        : (config.get('build.compilerPathDesktop') || '');
    const keyFileSetting = config.get('build.keyFile') || '';

    const workspaceFolder = vscode.workspace.getWorkspaceFolder(targetUri);
    const projectDir = workspaceFolder ? workspaceFolder.uri.fsPath : path.dirname(targetUri.fsPath);
    const entryRelative = workspaceFolder
        ? path.relative(projectDir, targetUri.fsPath)
        : path.basename(targetUri.fsPath);

    const resolvedExecutable = resolveVariables(executablePath, targetUri);
    const resolvedOutDir = resolveVariables(outDirSetting, targetUri);
    const resolvedRepoRoot = repoRootSetting
        ? resolveVariables(repoRootSetting, targetUri)
        : (detectRepoRoot(projectDir) || detectRepoRoot(path.dirname(targetUri.fsPath)));
    const resolvedCompilerPath = compilerPathSetting ? resolveVariables(compilerPathSetting, targetUri) : '';
    const resolvedKeyFile = keyFileSetting ? resolveVariables(keyFileSetting, targetUri) : '';

    if (!resolvedRepoRoot) {
        vscode.window.showErrorMessage(
            'AvaLang: no se pudo detectar la raiz del repo. Configura "avalang.build.repoRoot" en settings.json.'
        );
        return;
    }

    if (target.label === 'barekernel' && !resolvedCompilerPath) {
        vscode.window.showErrorMessage(
            'AvaLang: --target barekernel requiere "avalang.build.compilerPathBarekernel" apuntando al ' +
            'toolchain i686-elf (i686-elf-gcc/g++/ld/objcopy/nm).'
        );
        return;
    }

    const commandParts = [
        quoteIfNeeded(resolvedExecutable),
        'build',
        '--project', quoteIfNeeded(projectDir),
        '--entry', quoteIfNeeded(entryRelative),
        '--out', quoteIfNeeded(resolvedOutDir),
        '--repo-root', quoteIfNeeded(resolvedRepoRoot),
        '--target', target.label,
    ];

    if (resolvedCompilerPath) {
        commandParts.push('--compiler-path', quoteIfNeeded(resolvedCompilerPath));
    }
    if (target.label === 'desktop' && resolvedKeyFile) {
        commandParts.push('--key-file', quoteIfNeeded(resolvedKeyFile));
    }

    const term = ensureTerminal();
    term.show(true);
    term.sendText(`cd ${quoteIfNeeded(resolvedRepoRoot)}`);
    term.sendText(commandParts.join(' '));
}

function activate(context) {
    context.subscriptions.push(
        vscode.commands.registerCommand('avalang.runFile', runFile)
    );
    context.subscriptions.push(
        vscode.commands.registerCommand('avalang.buildExecutable', buildExecutable)
    );
    context.subscriptions.push(
        vscode.window.onDidCloseTerminal((closed) => {
            if (closed === terminal) terminal = undefined;
        })
    );

    context.subscriptions.push(
        vscode.languages.registerCompletionItemProvider('avalang', completionProvider, '.')
    );

    context.subscriptions.push(
        vscode.languages.registerHoverProvider('avalang', hoverProvider)
    );

    context.subscriptions.push(
        vscode.languages.registerSignatureHelpProvider('avalang', signatureHelpProvider, '(', ',')
    );

    context.subscriptions.push(
        vscode.languages.registerDefinitionProvider('avalang', definitionProvider)
    );

    context.subscriptions.push(
        vscode.languages.registerReferenceProvider('avalang', referenceProvider)
    );

    context.subscriptions.push(
        vscode.languages.registerRenameProvider('avalang', renameProvider)
    );

    context.subscriptions.push(
        vscode.languages.registerDocumentSymbolProvider('avalang', documentSymbolProvider)
    );

    context.subscriptions.push(
        vscode.languages.registerFoldingRangeProvider('avalang', foldingRangeProvider)
    );

    context.subscriptions.push(
        vscode.languages.registerCodeActionsProvider('avalang', codeActionProvider, {
            providedCodeActionKinds: [vscode.CodeActionKind.QuickFix, vscode.CodeActionKind.RefactorExtract],
        })
    );

    context.subscriptions.push(
        vscode.languages.registerInlayHintsProvider('avalang', inlayHintsProvider)
    );

    context.subscriptions.push(
        vscode.languages.registerDocumentFormattingEditProvider('avalang', documentFormattingProvider)
    );

    createDiagnosticsManager(context);
    createLintManager(context);
    createStatusBarManager(context);
}

function deactivate() {
    if (terminal) terminal.dispose();
}

module.exports = { activate, deactivate };
