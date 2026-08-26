# AvaLang para VS Code

Extensión que le da soporte al lenguaje **AvaLang** (archivos `.ava`) en Visual Studio Code:
colores de sintaxis, autocompletado con fragmentos de código listos para usar, y un botón para
correr tu script sin salir del editor.

## Qué te da

- **Colores de sintaxis** para todo lo que se ve en un archivo `.ava`: palabras clave (`if`,
  `func`, `class`, `try`, `async`, `await`, etc.), strings normales y con interpolación
  (`$"hola {nombre}"`), números, comentarios, diccionarios, y las funciones del módulo `system`
  (`System.Console`, `System.IO`, etc.) resaltadas aparte para que se distingan de tus propias
  variables.
- **Autocompletado con snippets**: escribí `if`, `while`, `func`, `class`, `try`, `extern`,
  `select`, `asyncfunc`, `lambda`, `fstring`, o cualquier método de `System.*`, y aceptá la
  sugerencia para insertar el bloque completo ya armado (con `then ... end` y los cursores en el
  lugar justo para completar).
- **Indentación y plegado automáticos**: al escribir `then`, `else`, `try`, etc. el editor
  indenta solo, y podés colapsar bloques completos (`if/while/for/func/class/try/extern/select
  ... end`) igual que en cualquier otro lenguaje.
- **Botón "Run File"**: corré el archivo `.ava` que tenés abierto con un clic, sin usar la
  terminal a mano (ver más abajo).

## Instalación

1. Descargá el archivo `.vsix` de esta extensión.
2. En VS Code, abrí la paleta de comandos (`Ctrl+Shift+P` / `Cmd+Shift+P`) y elegí
   **Extensions: Install from VSIX...**
3. Seleccioná el `.vsix` descargado. Listo — al abrir cualquier archivo `.ava` ya vas a ver el
   resaltado de sintaxis.

Si tu archivo no tiene la extensión `.ava`, podés forzar el lenguaje manualmente con `Ctrl+K M`
y eligiendo "AvaLang" de la lista.

## Correr tu script (botón ▶ / F5)

Con un archivo `.ava` abierto vas a ver un botón de play (▶) arriba a la derecha del editor.
También podés usar `F5`, o buscar **AvaLang: Run File** en la paleta de comandos. Cualquiera de
las tres opciones guarda el archivo (si tiene cambios sin guardar) y lo corre en una terminal
integrada llamada "AvaLang".

Por defecto la extensión asume que el ejecutable `ava_cli` está disponible en el `PATH` del
sistema. Si no es así, o si querés apuntar a un build específico de tu proyecto, configuralo en
**Settings** (`Ctrl+,`, buscar "AvaLang") o en `.vscode/settings.json` de tu carpeta de trabajo:

```json
{
  "avalang.executablePath": "/ruta/a/tu/build/ava_cli",
  "avalang.workingDirectory": "${fileDirname}",
  "avalang.modulesPath": "",
  "avalang.args": [],
  "avalang.clearTerminalBeforeRun": true
}
```

- **`avalang.executablePath`**: dónde está `ava_cli`. Podés poner solo el nombre si está en el
  `PATH` (`"ava_cli"`), o una ruta completa al ejecutable.
- **`avalang.workingDirectory`**: desde qué carpeta se ejecuta el script (por defecto, la carpeta
  del propio archivo `.ava`).
- **`avalang.modulesPath`**: carpeta `modules/` alternativa, si tu proyecto no usa la que está
  junto al ejecutable.
- **`avalang.args`**: argumentos extra que se le pasan al script.
- **`avalang.clearTerminalBeforeRun`**: si querés que limpie la terminal antes de cada corrida.

`executablePath` y `workingDirectory` aceptan variables como `${workspaceFolder}` y
`${fileDirname}`, útiles para no tener que hardcodear una ruta fija — por ejemplo, para que
siempre apunte al build dentro de tu propio proyecto sin importar en qué máquina estés:

```json
{
  "avalang.executablePath": "${workspaceFolder}/build_cli/runtime/avalang/Release/ava_cli"
}
```

Como esta configuración tiene alcance de carpeta/workspace (no solo global), podés tener una ruta
distinta guardada por cada proyecto.

---

## Para quienes quieran modificar la extensión

<details>
<summary>Probar cambios en modo desarrollo, empaquetar y publicar (clic para expandir)</summary>

### Probar la extensión en modo desarrollo

Requiere [VS Code](https://code.visualstudio.com/) y [Node.js](https://nodejs.org/) (para `vsce`,
el empaquetador oficial).

1. Abrí esta carpeta (`tools/vscode/`) en VS Code.
2. Presioná `F5` ("Run > Start Debugging"). Se abre una segunda ventana de VS Code ("Extension
   Development Host") con la extensión cargada y `examples/example.ava` abierto.
3. Si modificás la gramática (`syntaxes/avalang.tmLanguage.json`), no hace falta reiniciar: en la
   ventana de prueba corré `Developer: Inline Reload Extension` o `Developer: Reload Window` desde
   la paleta de comandos.

Nota: ese `F5` es el de VS Code para depurar extensiones, distinto del `F5` que la extensión ya
instalada agrega para correr archivos `.ava` (ese solo funciona dentro de la ventana de prueba,
con el foco en un editor `.ava`).

### Empaquetar (.vsix)

```bash
npm install -g @vscode/vsce
vsce package
```

Genera `avalang-<version>.vsix` en esta carpeta. Para instalarlo localmente:

```bash
code --install-extension avalang-<version>.vsix
```

### Publicar en el Marketplace

1. Creá un publisher en https://marketplace.visualstudio.com/manage (necesitás una organización
   de Azure DevOps y un Personal Access Token).
2. Ajustá el campo `"publisher"` en `package.json` si `avalontm` no es tu publisher real.
3. Iniciá sesión y publicá:

```bash
vsce login <tu-publisher>
vsce publish
```

`vsce publish patch` / `minor` / `major` incrementa la versión automáticamente antes de publicar.

### Escalar la gramática

Si `AvaLang.g4` cambia (nuevas palabras clave, nuevos operadores, nueva sintaxis de literal), solo
hace falta tocar `syntaxes/avalang.tmLanguage.json`:

- Nueva palabra clave de control -> agregarla a la alternancia regex de `keywords-control`.
- Nuevo tipo de declaración (como `class`/`func`/`extern`) -> agregar una entrada nueva en
  `repository` y referenciarla en el arreglo `patterns` de la raíz.
- Nuevo operador -> agregarlo dentro de `repository.operators`.
- Nuevo tipo primitivo -> agregarlo a la alternancia `(int|float|bool|string)` dentro de
  `repository.type-annotation`.

Es JSON plano: no hace falta recompilar nada, los cambios se ven con `Reload Window`.

</details>
