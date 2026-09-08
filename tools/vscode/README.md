# AvaLang para VS Code

Extensión que le da soporte al lenguaje **AvaLang** (archivos `.ava`) en Visual Studio Code:
colores de sintaxis, autocompletado con fragmentos de código listos para usar, y un botón para
correr tu script sin salir del editor.

## Novedades 0.9.0 (Fase 4.2 — Inlay Hints)

- **Tipos inferidos inline**: en `x = Robot()` o en un atributo `perfil = Perfil()` sin `as Tipo`,
  se muestra `: Robot` / `: Perfil` atenuado despues del nombre — solo cuando el tipo detectado es
  una clase real del proyecto, para no confundir una llamada a funcion comun con una
  instanciacion.
- **Retorno inferido inline**: en una funcion o metodo sin `as Tipo` de retorno pero cuyo cuerpo
  permite inferirlo (`return new Clase(...)` o `return variable`), se muestra ` as Clase` despues
  del parentesis de cierre de los parametros.
- **Nombres de parametro en llamadas**: `connect(host, timeout = 5)` muestra `host:` antes del
  primer argumento posicional; no se muestra si el argumento ya usa sintaxis con nombre
  (`timeout = 5`) o si es una variable con el mismo nombre que el parametro (para no repetir
  informacion obvia). Funciona con funciones, metodos (`this.algo()`, `obj.algo()`), constructores
  (`Clase(...)`) y funciones builtin (`abs`, `round`, etc.), pero no sobre parametros variadicos
  (`*values`).
- Todo es configurable: `avalang.inlayHints.enable` (interruptor general), y por separado
  `avalang.inlayHints.types` / `avalang.inlayHints.parameterNames`.

## Novedades 0.8.0 (Fase 4.1 — Code Actions / Quick Fix)

- **Lint estructural en vivo** (sin depender de `ava_cli`): detecta `if`/`elif`/`for`/`case` sin
  `then`, bloques sin su `end` correspondiente, y `end` sobrantes que no cierran nada — se marcan
  con subrayado amarillo apenas escribís.
- **Quick Fix `Agregar 'then'`**: sobre el aviso de `then` faltante, lo inserta al final de la
  linea (respetando un comentario `#` si lo hay).
- **Quick Fix `Agregar 'end' faltante(s)`**: cierra de una todos los bloques que quedaron
  abiertos al final del archivo, en el orden y la indentacion correctos.
- **Quick Fix `Quitar 'end' sobrante`**: borra la linea de un `end` que no correspondia a ningun
  bloque.
- **Quick Fix `Reemplazar por '...'`**: cuando `ava_cli --check` reporta un identificador
  desconocido entre comillas, sugiere hasta 3 nombres parecidos (funciones, clases, builtins,
  miembros de la clase actual) usando distancia de edicion, y arma el reemplazo exacto.
- **Generar Get/Set**: parado en la declaracion de un atributo de clase, ofrece una Quick Action
  (`Ctrl+.`) que genera `GetNombre()`/`SetNombre(value)` ya tipados si el atributo tiene `as Tipo`,
  usando `this.` o `NombreClase.` segun sea de instancia o `static`.

## Novedades 0.6.0

- **Go to Definition** (`F12` / `Ctrl+clic`): sobre una funcion, clase, atributo, metodo, alias
  de import, o `this`/`self`, salta a su declaracion aunque este en otro archivo (por ejemplo un
  modulo importado).
- **Find All References** (`Shift+F12`): busca todas las apariciones de un simbolo en todos los
  `.ava` del workspace, no solo el archivo abierto.
- **Rename Symbol** (`F2`): renombra un simbolo y todas sus apariciones en todos los `.ava` del
  workspace de una sola vez.

## Novedades 0.5.0

- **Autocompletado encadenado** (`getUser().perfil.nombre`): ahora sigue el tipo de retorno de
  una función/método (anotado con `as Tipo`, o inferido mirando sus `return` internos) y el tipo
  de un atributo (anotado o inferido de su valor inicial), para sugerir miembros después de
  cualquier cantidad de puntos, no solo el primero.
- **Fix**: la inferencia de tipo de una variable no reconocía instancias creadas con
  `x as Tipo = new Tipo(...)` (con `new`) — solo `x = Tipo(...)`. Ya soporta ambas formas, con o
  sin `local` y con o sin anotación de tipo.

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
- **Autocompletado inteligente**: además de los snippets, la extensión sugiere palabras clave,
  las funciones nativas (`print`, `len`, `range`, etc.) con su firma, y las funciones/clases que
  ya definiste en tu archivo o importaste con `import`. Escribí `import ` y te sugiere los
  módulos disponibles; escribí `this.` dentro de un método (o `variable.` sobre algo creado con
  `Clase(...)`) y te sugiere sus atributos y métodos.
- **Ayuda al pasar el mouse y al llamar funciones**: pasá el mouse sobre una palabra clave, una
  función nativa, o algo tuyo para ver su sintaxis/firma; al escribir `(` dentro de una llamada,
  VS Code te muestra qué parámetros espera.
- **Errores marcados en rojo**: al guardar (o mientras escribís, si lo activás) la extensión
  compila el script con `ava_cli` en segundo plano y subraya en rojo la línea exacta del error,
  igual que cualquier otro lenguaje — así te enterás de un error de sintaxis o de una función que
  no existe sin tener que correr el script primero.

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

## Errores en rojo mientras trabajás

Por defecto, cada vez que guardás un `.ava` la extensión lo compila con `ava_cli --check` (no lo
ejecuta) y, si hay un error, lo subraya en rojo en la línea exacta con el mensaje al pasar el
mouse. Requiere que `avalang.executablePath` apunte a un `ava_cli` que ya tenga el flag
`--check` (agregado en esta misma versión).

Se puede ajustar en **Settings**:

```json
{
  "avalang.diagnostics.enable": true,
  "avalang.diagnostics.runOn": "onSave",
  "avalang.diagnostics.debounceMs": 600
}
```

- **`avalang.diagnostics.runOn`**: `"onSave"` (default, chequea el archivo tal cual está guardado
  en disco), `"onType"` (chequea mientras escribís, sobre una copia temporal — no toca tu
  archivo real), o `"off"` para desactivarlo del todo.
- **`avalang.diagnostics.debounceMs`**: con `"onType"`, cuánto esperar después de la última tecla
  antes de volver a chequear (para no recompilar en cada letra).

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
