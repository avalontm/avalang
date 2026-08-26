# Changelog

## 0.3.3

- `README.md` reescrito enfocado en el usuario final en vez del mantenedor: arriba explica qué
  hace la extensión, cómo instalarla y cómo correr un archivo con el botón "Run File" (con
  ejemplos de configuración genéricos, sin rutas de desarrollador hardcodeadas). Todo lo de
  mantenimiento (modo desarrollo, empaquetar, publicar al Marketplace, cómo escalar la gramática)
  se movió a una sección plegable al final, "Para quienes quieran modificar la extensión".

## 0.3.2

- `package.json`: translated all `avalang.*` setting descriptions to English (they were in
  Spanish, inconsistent with the rest of the manifest/marketplace listing). `executablePath`'s
  example path was also a hardcoded developer path (`D:/_CODE_/avalang/...`); replaced with
  generic cross-platform examples.

## 0.3.1

- **Fix: `async`/`await` no tenian resaltado.** `asyncFuncDeclaration` ('async' funcDeclaration) y
  `awaitAtom` ('await' expr) existen en `AvaLang.g4` desde antes pero nunca se habian agregado a
  `syntaxes/avalang.tmLanguage.json`; el texto caia en el resaltado por defecto (sin color). Ahora:
  - `async` -> `storage.modifier.async.avalang` (mismo criterio que `storage.modifier.async` en
    gramaticas JS/TS, para que temas existentes ya lo coloreen razonablemente distinto de
    `static`/`private`).
  - `await` -> `keyword.control.flow.await.avalang`, agregado antes que `#function-call` en el
    arreglo de `patterns` para que `await(x)` (sin espacio) no se interprete como una llamada a una
    funcion llamada `await`. Tambien se agrego dentro del bloque de expresion embebida de f-strings.
- **Fix: `language-configuration.json` no indentaba/plegaba bien varias formas de la gramatica:**
  - `increaseIndentPattern` exigia que la linea empezara literalmente con `func`, asi que
    `async func x()`, `static func x()` y `private func x()` no auto-indentaban el cuerpo. Ahora
    acepta cualquier combinacion de `static`/`private`/`async` antes de `func`.
  - `select ... case ... then ... end` (VB6-style switch, ya resaltado como keywords desde antes)
    nunca se habia agregado a `increaseIndentPattern`/`decreaseIndentPattern`/`folding.markers`:
    `select` y `case ... then` ahora indentan como `if`/`elif`, y `case` desindenta como `elif`
    antes de la siguiente rama.
  - `folding.markers.start` no reconocia lineas que empiezan con `async`/`static`/`private`/`select`
    como apertura de bloque (aunque el `end` de cierre sea el mismo de siempre).
- `snippets/avalang.json`: nuevos `asyncfunc` (`async func ... end`), `await` (`await expr`),
  `staticfunc`/`privatefunc` (`modifiedFuncDeclaration`, tampoco tenian snippet) y `select`
  (`select ... case ... then ... else ... end`, tampoco tenia snippet).
- `examples/example.ava` ahora ejercita `async func`, `await`, y `static func`/`private func`
  dentro de una clase (el ejemplo real de `samples/test/fase3_02_await_metodo_con_this.ava`).
- Verificado con `vscode-textmate`: tokenizado sin errores de los 17 archivos `.ava` de
  `samples/test/` (incluyendo el que reporto el bug) y confirmacion puntual de que `async`/`await`/
  `select` producen los scopes esperados.

## 0.3.0

- Resaltado nuevo para la sintaxis de `as Type` de `AvaLang_Plan_Sistema_de_Tipos.md`: anotacion de
  variable (`age as int`, con o sin `= expr`), tipos de parametro (`func f(a as int)`, incluyendo
  `externParam` dentro de un bloque `extern`) y tipo de retorno (`returnType`, tanto en `func` como
  en las dos formas de lambda: `(x as int) as int => expr` y `func(x as int) as int ... end`).
  Los cuatro tipos primitivos del plan (`int`, `float`, `bool`, `string`) tienen su propio scope
  (`storage.type.primitive.avalang`); cualquier otro nombre despues de `as` (una clase de usuario,
  o un typo) usa `entity.name.type.avalang`, igual que el nombre de una clase declarada con `class`.
- El `as Alias` de `extern "lib" as Alias` sigue con su resaltado propio de namespace/alias sin
  cambios -- la regla nueva lo excluye explicitamente (via lookbehind del `"` que cierra el STRING)
  para no confundir un alias de modulo con un tipo.
- `snippets/avalang.json`: nuevos `functyped` (`func` con parametros y retorno tipados), `astype`
  (`name as Type = value`), `astypedecl` (`name as Type` sin inicializar), `localtyped`
  (`local name as Type = value`, shadowing explicito), `lambdatyped` (lambda con parametro y
  retorno tipados) y `externtyped` (`extern` con un `func` tipado).
- `examples/example.ava` ahora tambien ejercita la sintaxis de tipos: variables inferidas y
  anotadas, declaracion sin inicializar, funcion tipada, `local` tipado (shadowing), lambda tipada,
  `extern` con parametros/retorno tipados y una clase con propiedad/metodo tipados.
- Verificado igual que en 0.1.2: tokenizado automatico (`vscode-textmate`) de `examples/example.ava`
  completo, confirmando que las reglas nuevas no rompen ninguna region existente (strings, f-strings,
  `extern`, diccionarios) y que el alias de `extern`/`import` sigue sin pisarse con el resaltado de
  tipos nuevo.

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
