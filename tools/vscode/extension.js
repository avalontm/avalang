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
const fs = require('fs');

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

function looksLikeRepoRoot(dir) {
    return fs.existsSync(path.join(dir, 'CMakeLists.txt')) &&
           fs.existsSync(path.join(dir, 'runtime', 'avapack', 'CMakeLists.txt'));
}

// Same walk-up-from-here logic as DetectRepoRoot() in
// runtime/avastudio/src/panels/build_panel.cpp -- kept in sync deliberately
// so "repo root" means the same thing whether you build from Ava Studio or
// from this extension.
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

// "AvaLang: Build Executable..." -- packages the current .ava file with
// `ava_cli build` (runtime/avacli/src/build_command.cpp), the same
// subcommand the Build panel in Ava Studio shells out to. No .bat/.sh
// scripts involved, same as runFile above.
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
    // One setting per target -- picked automatically below based on
    // `target.label`, so switching the QuickPick selection uses the right
    // path without overwriting the other one.
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
    // Same flag for both targets -- see ava_cli build --help: prepended to
    // PATH for 'desktop', used as the i686-elf toolchain root for
    // 'barekernel'. Just one path either way.
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
}

function deactivate() {
    if (terminal) terminal.dispose();
}

module.exports = { activate, deactivate };
