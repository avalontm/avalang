# AvaLang for VS Code

Resaltado de sintaxis para archivos `.ava` del lenguaje **AvaLang**, generado a partir de la
gramática ANTLR en `runtime/avalang/grammar/AvaLang.g4`.

## Que incluye

- Gramática TextMate (`syntaxes/avalang.tmLanguage.json`) con:
  - Palabras clave de control: `if / then / elif / else / end / while / for / in / try / catch / finally / return / break / continue / pass / raise`
  - Declaraciones: `class`, `func`, `extern ... as ... end`, `import ... as ...`, `local`
  - Modificadores de miembro: `static`, `private`
  - Operadores lógicos como palabra: `and`, `or`, `not`
  - Constantes de lenguaje: `true`, `false`, `nil`, y `this` resaltado como variable especial
  - Strings simples (`"..."`, `'...'`) con escapes, y f-strings (`$"...{expr}..."`) con la
    interpolación `{ }` resaltada como código embebido (recursivo, admite anidamiento)
  - Números, comentarios `#`, operadores (`==`, `!=`, `+=`, `**`, `//`, `++`, `--`, `=>`, etc.)
  - Nombres de clase, herencia (`class Dog: Animal`), llamadas a función y nombres de función
    declarados
  - Claves de diccionario resaltadas como en JSON (`{key: v}` o `{"key": v}`), acceso a
    propiedades (`this._name`, `obj.attr`) y argumentos nombrados (`foo(x = 1)`) distinguidos de
    variables sueltas, y continuación de línea con `\`
  - Namespaces del módulo nativo `system` (`System`, `Console`, `DateTime`, `Environment`, `IO`,
    `File`, `Directory`, `Diagnostics`, `Process`) resaltados con su propio color cuando se usan
    como namespace (identificador seguido de `.`), tanto en código normal como dentro de f-strings
- `language-configuration.json`: comentario de línea `#` (con continuación automática al presionar
  Enter), auto-cierre de `()[]{}` y comillas, indentación (`then/else/try/finally/catch` aumentan,
  `end/else/elif/catch/finally` bajan), *folding* por marcadores (`if/while/for/func/class/try/
  extern` ... `end`) en vez de solo por indentación, y tabSize=4 por defecto para `.ava`
- `examples/example.ava`: archivo de prueba que usa clases, herencia, `extern`, f-strings,
  lambdas cortas, `try/catch/finally`, `for/while`, etc.
- `snippets/avalang.json`: snippets de bloques (`if`, `while`, `for`, `func`, `class`, `try`,
  `extern`, lambda, f-string) que usan `then ... end`, para que el autocompletado no ofrezca
  llaves `{ }` de otros lenguajes al escribir estas palabras clave; y snippets de autocompletado
  para toda la API del módulo nativo `system` (`System.Console.*`, `System.DateTime.*`,
  `System.Environment.*`, `System.IO.File.*`, `System.IO.Directory.*`,
  `System.Diagnostics.Process.*`), tomados 1:1 de
  `runtime/avalang/src/builtins/system_module.cpp`
- Icono de extensión y de archivo en `icons/`
- **Ejecutar el archivo actual** (`extension.js`, el único código de activación de la extensión):
  - Botón de play (▶) arriba a la derecha del editor cuando el archivo abierto es `.ava`.
  - `F5` con el foco en un editor `.ava` (comando `avalang.runFile`).
  - También disponible desde la paleta de comandos como **AvaLang: Run File**.
  - Guarda el archivo si tiene cambios sin guardar, y corre `ava_cli <script.ava>` (con
    `--modules <dir>` si se configuró) en una terminal integrada dedicada llamada "AvaLang".
  - Configurable en Settings bajo **AvaLang** (`avalang.*`, con scope de carpeta/workspace, no solo
    global):
    - `avalang.executablePath` — ruta al ejecutable `ava_cli` (nombre en el PATH o ruta absoluta).
    - `avalang.workingDirectory` — directorio de trabajo (default `${fileDirname}`).
    - `avalang.modulesPath` — override opcional de `modules/` (como `--modules`).
    - `avalang.args` — argumentos extra para el script.
    - `avalang.clearTerminalBeforeRun` — limpiar la terminal antes de cada corrida.
    - Todas admiten `${workspaceFolder}`, `${fileDirname}`, `${fileBasename}`,
      `${fileBasenameNoExtension}`, `${file}`.
- `.github/workflows/package.yml`: CI que empaqueta el `.vsix` en cada push/PR (lo sube como
  artifact) y lo publica al Marketplace automáticamente al crear un tag `v*` (usando el secret
  `VSCE_PAT`)

Es un proyecto **casi puramente declarativo** (todo el resaltado/snippets/folding es JSON); el
único código de activación es `extension.js`, dedicado exclusivamente al comando "Run File" de
arriba, sin dependencias externas ni paso de build.

## Probar la extensión (modo desarrollo)

Requiere [VS Code](https://code.visualstudio.com/) y [Node.js](https://nodejs.org/) (para `vsce`,
el empaquetador oficial).

1. Abre esta carpeta (`avalang-vscode/`) en VS Code.
2. Presiona `F5` (o "Run > Start Debugging"). Esto abre una segunda ventana de VS Code
   ("Extension Development Host") con la extensión cargada y `examples/example.ava` abierto.
3. Verifica que el archivo se vea coloreado. Puedes editarlo o abrir tus propios `.ava` en esa
   ventana para seguir probando.
4. Si modificas la gramática (`syntaxes/avalang.tmLanguage.json`), no hace falta reiniciar: en la
   ventana de prueba corre `Developer: Inline Reload Extension` o `Developer: Reload Window`
   desde la paleta de comandos (`Ctrl+Shift+P`).

También puedes forzar manualmente el lenguaje de un archivo sin extensión `.ava` con
`Ctrl+K M` y eligiendo "AvaLang".

Nota: al presionar `F5` en la ventana de **desarrollo de la extensión** (paso 2 de arriba, dentro
de este proyecto `avalang-vscode/`), eso sigue siendo el `F5` normal de VS Code para depurar
extensiones -- no tiene nada que ver con el `F5` que la extensión ya instalada agrega para correr
archivos `.ava` (ese solo aplica dentro de la ventana de prueba, con el foco en un editor `.ava`).

## Configurar el ejecutable de `ava_cli` (Run File)

Por defecto, "AvaLang: Run File" (boton de play / `F5` / paleta de comandos) intenta correr
`ava_cli` asumiendo que esta en el `PATH`. Si no lo esta, o si querés apuntar a un build especifico
del proyecto, configuralo en Settings (`Ctrl+,`, buscar "AvaLang") o directamente en
`.vscode/settings.json` -- como tiene scope de carpeta/workspace, podés tener una ruta distinta por
proyecto:

```json
{
  "avalang.executablePath": "D:/_CODE_/avalang/build_cli/runtime/avalang/Release/ava_cli.exe",
  "avalang.workingDirectory": "${fileDirname}",
  "avalang.modulesPath": "",
  "avalang.args": [],
  "avalang.clearTerminalBeforeRun": true
}
```

`avalang.executablePath` y `avalang.workingDirectory` admiten `${workspaceFolder}`,
`${fileDirname}`, `${fileBasename}`, `${fileBasenameNoExtension}` y `${file}` -- por ejemplo, para
apuntar siempre al build dentro del propio workspace sin hardcodear la unidad de disco:

```json
{
  "avalang.executablePath": "${workspaceFolder}/build_cli/runtime/avalang/Release/ava_cli.exe"
}
```

## Empaquetar (.vsix)

```bash
npm install -g @vscode/vsce
vsce package
```

Esto genera `avalang-0.1.0.vsix` en la raíz del proyecto. Para instalarlo localmente:

```bash
code --install-extension avalang-0.1.0.vsix
```

O desde VS Code: paleta de comandos > "Extensions: Install from VSIX...".

## Publicar en el Marketplace

1. Crea un publisher en https://marketplace.visualstudio.com/manage (necesitas una organización
   de Azure DevOps y un Personal Access Token).
2. Ajusta el campo `"publisher"` en `package.json` si `avalontm` no es tu publisher real.
3. Inicia sesión y publica:

```bash
vsce login <tu-publisher>
vsce publish
```

`vsce publish patch` / `minor` / `major` además incrementa la versión automáticamente antes de
publicar.

## Escalar la gramática

Si `AvaLang.g4` cambia (nuevas palabras clave, nuevos operadores, nueva sintaxis de literal),
solo necesitas tocar `syntaxes/avalang.tmLanguage.json`:

- Nueva palabra clave de control -> agregarla a la alternancia regex de `keywords-control`.
- Nuevo tipo de declaración (como `class`/`func`/`extern`) -> agregar una entrada nueva en
  `repository` y referenciarla en el arreglo `patterns` de la raíz.
- Nuevo operador -> agregarlo dentro de `repository.operators`.

No hace falta recompilar nada: es JSON plano, cambios visibles con `Reload Window`.
