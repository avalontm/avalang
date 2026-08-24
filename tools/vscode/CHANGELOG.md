# Changelog

## 0.2.0

- Nuevo comando **AvaLang: Run File**, disponible como boton de play (arriba a la derecha del
  editor, `resourceLangId == avalang`), con `F5` (`editorLangId == avalang`) y desde la paleta de
  comandos. Corre `ava_cli` sobre el archivo `.ava` actual en una terminal integrada dedicada
  ("AvaLang"), guardando el archivo primero si tiene cambios sin guardar.
- Nueva seccion de configuracion **AvaLang** (`avalang.*`, scope `resource` -- se puede fijar por
  workspace/carpeta ademas de global):
  - `avalang.executablePath`: ruta al ejecutable `ava_cli` (por nombre si esta en el PATH, o ruta
    absoluta/relativa). Default: `ava_cli`.
  - `avalang.workingDirectory`: directorio de trabajo para la ejecucion. Default: `${fileDirname}`.
  - `avalang.modulesPath`: override opcional de `modules/` (equivalente a `--modules` /
    `AVA_MODULES_PATH`).
  - `avalang.args`: argumentos extra para el script (quedan en el global `args` de AvaLang).
  - `avalang.clearTerminalBeforeRun`: limpiar la terminal antes de cada corrida (default `true`).
  - Todas admiten `${workspaceFolder}`, `${fileDirname}`, `${fileBasename}`,
    `${fileBasenameNoExtension}`, `${file}`.
- La extension deja de ser puramente declarativa: agrega `extension.js` (JS plano, sin
  dependencias ni paso de build) que activa con `onLanguage:avalang` y registra el comando
  `avalang.runFile`.

## 0.1.3

- Resaltado propio (`support.class.avalang`) para los namespaces del modulo nativo `system`
  (`System`, `Console`, `DateTime`, `Environment`, `IO`, `File`, `Directory`, `Diagnostics`,
  `Process`), tanto en codigo normal como dentro de f-strings interpoladas. Solo se activa cuando
  el identificador va seguido de un punto, para no pisar variables propias con el mismo nombre.
- Snippets nuevos para toda la API expuesta por `runtime/avalang/src/builtins/system_module.cpp`:
  `import system`, `System.Console.*` (Write/WriteLine/WriteError/ReadLine/ForegroundColor con
  eleccion de color/ResetColor), `System.DateTime.*` (Now/UtcNow/ToString/Sleep),
  `System.Environment.*` (GetEnvironmentVariable/SetEnvironmentVariable/GetCurrentDirectory/
  SetCurrentDirectory/GetCommandLineArgs), `System.IO.File.*` (ReadAllText/WriteAllText/Delete/
  Exists/Size), `System.IO.Directory.*` (Create/Delete/Exists/Enumerate) y
  `System.Diagnostics.Process.*` (Start/GetCurrentId).
- `examples/example.ava` ahora tambien ejercita el modulo `system` para verificar el resaltado
  nuevo.

## 0.1.2

- Dictionary keys now highlight like JSON keys, both bare (`{key: v}`) and quoted (`{"key": v}`).
- Property access (`this._name`, `obj.attr`) and named call arguments (`foo(x = 1)`) get their own
  scopes instead of falling back to plain identifiers.
- Line continuation (`\` at end of line) is highlighted.
- Marker-based code folding (`if/while/for/func/class/try/extern` ... `end`) instead of relying
  only on indentation.
- Comments auto-continue with `# ` when pressing Enter inside a comment line.
- Default `.ava` editor settings (tabSize 4, no indentation auto-detection).
- Added GitHub Actions workflow to package the `.vsix` on every push and publish on tag `v*`.
- Verified the grammar with an automated tokenizer check (vscode-textmate) confirming no unclosed
  string/dict/extern regions and correct scopes for the new rules.

## 0.1.1

- Added language snippets (`snippets/avalang.json`) for `if`, `if/else`, `if/elif/else`, `while`,
  `for/in`, `func`, `class`, `class` with heritage, `try/catch`, `try/catch/finally`, `extern`,
  short lambdas and f-strings, all using AvaLang's real `then ... end` block syntax instead of
  curly braces.

## 0.1.0

- Resaltado de sintaxis inicial para AvaLang, basado en `runtime/avalang/grammar/AvaLang.g4`.
- Soporte para clases, herencia, `extern` FFI, f-strings interpoladas, lambdas cortas y todos los
  operadores/palabras clave de la gramática.
- Configuración de lenguaje con auto-cierre de brackets/comillas e indentación básica.
