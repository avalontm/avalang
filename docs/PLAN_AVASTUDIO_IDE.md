# Plan — AvaStudio como IDE (diseño, opciones y alcance de build)

## Propósito

Definir, antes de tocar código, cómo debe quedar diseñado AvaStudio como
IDE completo para AvaLang: su layout, sus paneles/opciones, y el alcance
exacto de las cuatro operaciones centrales del flujo de desarrollo —
**generar, correr, compilar y empaquetar**. Sirve como documento de
referencia para trabajar el resto por fases, igual que
`plan_ava_pack.md` o `AVAUI_ARCHITECTURE_FREEZE_PLAN.md`.

Inspiración declarada: VS Code. No como calco pixel a pixel, sino como
modelo mental — layout tipo "workbench" (activity bar + sidebar + área
de edición con pestañas + panel inferior + status bar), paleta de
comandos, y una capa de extensiones. La diferencia de fondo es que
AvaStudio no es un editor de texto genérico con plugins de lenguaje:
es un IDE de un solo lenguaje (AvaLang) con un canvas de diseño visual
integrado (AvaUI) — el equivalente sería VS Code si trajera Figma
incorporado y ambos compartieran el mismo árbol de componentes.

## 1. Estado actual (lo que ya existe en el zip)

| Pieza | Archivo | Qué hace hoy |
|---|---|---|
| Title bar + menús | `titlebar_panel.cpp` | File, panels toggle, Run (`F5` Run Script, `Ctrl+B` Build Executable), Preferences (Settings, Extensions) |
| Editor de código | `editor_panel.cpp` (1455 líneas) | Pestañas, syntax highlighting AvaLang, autocompletado vía `class_index`/`function_index`/`keyword_docs` |
| Canvas de diseño | `designer_canvas.cpp` (1143 líneas) | Edición visual de `.avaui`, bindings de estado (`state_eval.cpp`) |
| Explorador de archivos | `explorer_panel.cpp` | Árbol del proyecto |
| Propiedades | `properties_panel.cpp` | Inspector de componentes seleccionados en el canvas |
| Toolbox | `toolbox_panel.cpp` | Catálogo de componentes arrastrables (`component_catalog.csv`) |
| Terminal | `terminal_panel.cpp` | Ejecuta procesos (Run Script) |
| Output / Logs | `output_panel.cpp`, `logs_panel.cpp` | Salida de build/run separada del terminal |
| Build panel | `build_panel.cpp` (693 líneas) | Front-end de `ava_cli build` → `avapack`; build en hilo aparte, log en vivo, instala vcpkg |
| Settings | `settings_panel.cpp` | Preferencias persistidas (`util/settings.cpp`) |
| Sistema de plugins | `plugin_host.cpp`, `plugin_api.h` (24k) | Ya existe una API de extensión real, con dos plugins de ejemplo: `hello_world` y `ai_agent` (agente IA con herramientas, contexto del proyecto, cliente OpenRouter) |
| Pending edits | `pending_edits_panel.cpp` | Revisión de cambios propuestos (probablemente por el agente IA) antes de aplicarlos |

Conclusión: la base de "workbench" (paneles acoplables, terminal,
output, build en background) **ya está construida**. Lo que falta no es
la arquitectura de paneles — es (a) cerrar huecos de paridad con VSCode
que hoy no existen (paleta de comandos, quick open, panel de
problemas/diagnósticos, debugger), y (b) formalizar el contrato de
las cuatro operaciones (generar/correr/compilar/empaquetar) para que
sean consistentes, descubribles y con las mismas opciones que tendría
`ava_cli` desde línea de comandos.

## 2. Principios de diseño (layout tipo VS Code)

```
+------------------------------------------------------------------+
| Title bar: menú + nombre de proyecto + botones de ventana        |
+---+----------------------------------------------------+---------+
| A |  Editor / Canvas (pestañas, split view)             |  Props  |
| c |                                                      |  panel  |
| t |                                                      | (si hay |
| i |                                                      | selec-  |
| v |                                                      | ción)   |
| i |                                                      |         |
| t +------------------------------------------------------+---------+
| y |  Panel inferior: Terminal | Output | Logs | Problems           |
| B +------------------------------------------------------------------+
| a |  Status bar: rama/branch (futuro), línea:col, errores, build   |
| r |                                                                  |
+---+------------------------------------------------------------------+
```

- **Activity Bar** (nuevo, hoy no existe como tal): columna vertical de
  íconos a la izquierda para cambiar entre Explorer, Búsqueda,
  Toolbox/Componentes, Extensiones, Ajustes — reemplaza el actual menú
  "Panels" disperso en la title bar por algo descubrible y siempre
  visible, igual que VS Code.
- **Sidebar contextual**: cambia de contenido según el ítem activo del
  Activity Bar (Explorer, Toolbox, lista de Extensiones), en vez de
  paneles flotantes independientes.
- **Editor/Canvas central**: pestañas mezclando archivos `.ava` (texto)
  y `.avaui` (canvas visual) en el mismo sistema de tabs — ya es así
  hoy; formalizar que ambos tipos conviven como "editores" del mismo
  árbol, no como modos separados.
- **Panel inferior con pestañas**: Terminal, Output, Logs y un nuevo
  **Problems** (diagnósticos del compilador, ver §3.3) agrupados como
  pestañas de un mismo panel — hoy Output/Logs/Terminal son paneles
  separados; agruparlos baja la carga visual. **Bug de nomenclatura
  encontrado al revisar el código**: `output_panel.cpp` y
  `logs_panel.cpp` usan **el mismo título de ventana ImGui**,
  `"Output"` — en ImGui el título es el identificador de la ventana,
  así que hoy esos dos paneles compiten por la misma identidad (según
  el orden de dibujado, uno puede terminar mostrando el contenido del
  otro, o forzarse a la misma pestaña de dock de forma no intencional).
  Hay que renombrar uno de los dos (`"Output"` para stdout/stderr de
  ejecución vs. `"Logs"` o `"Runtime Log"` para el log interno del
  IDE/engine) **antes** de agruparlos en el panel inferior — si no, el
  bug queda enmascarado en vez de arreglado.
- **Status bar**: línea:columna del cursor, estado del build (idle /
  compilando / éxito / error), y accesos rápidos (versión de AvaLang,
  target de empaquetado activo).
- **Command Palette** (`Ctrl+Shift+P`, nuevo): buscador de acciones —
  "Run Script", "Build Executable", "New Class", "Install vcpkg",
  toggle de paneles — todo lo que hoy vive repartido en menús,
  accesible por texto. Es la pieza de paridad con VSCode más visible
  que falta.
- **Quick Open** (`Ctrl+P`, nuevo): saltar a cualquier archivo del
  proyecto por nombre, sin usar el árbol del Explorer.

## 3. Las cuatro operaciones — alcance funcional

Cada una debe mapear 1:1 a un subcomando de `ava_cli`, para que "lo que
hace el botón" y "lo que puede hacer un desarrollador por consola" no
diverjan nunca — mismo principio ya aplicado en `build_panel.cpp`
("nunca habla con CMake/avapack directamente, solo llama a `ava_cli`").

### 3.1 Generar (`ava_cli new` / scaffolding)

Hoy no existe un flujo de "nuevo proyecto" dentro de AvaStudio — se
asume un proyecto ya abierto vía "Open Folder...". Alcance propuesto:

- **Nuevo proyecto**: wizard mínimo (nombre, carpeta destino, plantilla
  inicial — vacío / con una pantalla AvaUI de ejemplo) que interna-
  mente llama a un futuro `ava_cli new <plantilla> <destino>`.
- **Nuevo archivo dentro del proyecto**: clase `.ava`, pantalla
  `.avaui`, ya con boilerplate correcto (constructor estilo C#, no
  Python — ver convención ya fijada del lenguaje).
- **Generación desde el catálogo**: al arrastrar un componente nuevo al
  canvas, el snippet `.avaui` correspondiente se genera automáticamente
  (esto ya ocurre parcialmente vía `component_catalog.cpp` — formalizar
  como parte de "Generar", no como comportamiento implícito del canvas).
- Explícitamente fuera de alcance por ahora: generación de código por
  IA como reemplazo de este flujo — eso es el rol del plugin
  `ai_agent`, que es un mecanismo aparte y no debe confundirse con el
  scaffolding determinístico de "Generar".

### 3.2 Correr (`ava_cli run`, ya existe como "Run Script" / F5)

Revisado `main.cpp` (`perform_run`): hoy **siempre** corre el tab
activo (`engine.RunScript(active->GetText(), active->file_path)`) — no
existe concepto de "entry point del proyecto"; si no hay ningún tab
abierto, falla con `"no hay ningún archivo abierto para ejecutar"`. No
hay manifest de proyecto (`app_manifest.h` no existe en el repo — fue
una suposición mía en la iteración anterior de este plan, corregida
acá). Lo que sí existe y es reutilizable: `DetectEntryFile()`
(`build_panel.cpp`), la misma heurística que usa Empaquetar — prioriza
`main.ava` en la raíz, si no existe busca el primer `.ava` alfabético
hasta 3 niveles de profundidad.

Diseño formalizado:

- **Mantener** "correr el tab activo" como comportamiento por defecto
  de `F5` cuando hay un archivo abierto y en foco — es simple, no
  requiere manifest, y es el caso de uso más común mientras se itera
  un script suelto.
- **Agregar** "Run Project" (`Shift+F5`, nuevo) que corre el entry
  point resuelto con la misma `DetectEntryFile()` que ya usa el Build
  panel — hoy esa función vive solo en `build_panel.cpp`; hay que
  moverla a un lugar compartido (p. ej. `util/project_utils.*`) para
  que Run y Build usen exactamente la misma heurística y nunca
  diverjan sobre "cuál es el entry point de este proyecto".
- **Detener ejecución en curso** (Stop): hoy no existe — `perform_run`
  es síncrono/bloqueante dentro del engine, no hay hilo cancelable
  como sí tiene Build (`build_panel.cpp` corre en `std::thread`). Si
  se quiere Stop real, correr también en background con el mismo
  patrón que Build.
- Argumentos/variables de entorno al correr: no hay señal de que algo
  los soporte hoy (`RunScript` toma solo fuente + nombre). Queda fuera
  del alcance de esta fase — anotado como roadmap, no se diseña en
  detalle todavía.

### 3.3 Compilar (chequeo, sin correr ni empaquetar) — Problems panel

Hoy no existe un paso de "solo compilar". Pero sí existe ya la pieza
más importante para construirlo: `editor_panel.h/.cpp` expone
`HighlightError(state, file_path, line, column, message)` y
`ClearErrorHighlights(state)`, que es exactamente lo que hoy usa
`perform_run` para marcar el error en el editor cuando `RunScript`
falla (con salto automático al archivo vía `OpenFileInTab` si el error
está en otro archivo del que se ejecutó). Es decir: el mecanismo de
"mostrar un error en el lugar exacto del código" **ya está resuelto**;
lo que falta es (a) poder dispararlo sin ejecutar, y (b) agregarlo en
una lista, no solo como marca puntual en el editor.

Diseño:

- **"Check"** (`Ctrl+Shift+B`, bajo demanda — no automático al
  guardar, porque hoy no hay infraestructura de watcher/debounce y
  agregarla es un costo aparte no cubierto por este plan): corre
  parseo + type-check de todo el proyecto (cada `.ava` alcanzable
  desde el entry point) sin invocar `RunScript`, reutilizando la misma
  ruta de errores que ya produce `perform_run` (mismo tipo de dato:
  archivo, línea, columna, mensaje).
- **Problems panel** (nuevo, agrupado en el panel inferior junto a
  Terminal/Output/Logs, ver §2): lista agregada de todos los errores
  encontrados en la pasada de Check — no reemplaza `HighlightError`,
  lo complementa: al hacer click en un ítem de la lista, se llama al
  mismo `HighlightError` + `OpenFileInTab` que ya usa Run, para saltar
  al lugar exacto.
- Los errores que hoy aparecen solo por correr (`perform_run`) deben
  también alimentar el Problems panel, no quedar aislados en el
  Terminal — un único sink de errores para Run, Check y (a futuro)
  Build, en vez de tres mecanismos de reporte de error distintos.

### 3.4 Empaquetar (`ava_cli build`, ya existe — Build Executable / Ctrl+B)

Ya es la operación más madura (`build_panel.cpp`, con hilo en
background, log en vivo, e instalación de vcpkg integrada — pero hoy
**no** pasa `--target`, así que siempre usa el default `desktop`
implícito de `ava_cli build`, y no expone ni plataforma ni tipo de
build en la UI). Revisado el pipeline real de `runtime/avapack/` para
fijar esto con precisión — dos ejes que hoy se confunden bajo el mismo
nombre "target" y que en la UI deben quedar como dos campos separados:

#### 3.4.1 Plataforma (SO) — detectada, no elegible (por ahora)

`avapack` **no cross-compila**: el `.exe` resultante siempre es para el
mismo SO donde corre el toolchain que lo compila (CMake + MSVC en
Windows, gcc/g++ en Linux/macOS). Soporte multi-SO real es la Fase 8
de `avapack`, hoy marcada como **bloqueada por trabajo externo**. Diseño:

- Un campo de solo lectura en el Build panel: **"Plataforma: Windows
  (detectada)"** (o Linux/macOS), calculado igual que ya hace
  `build_command.cpp` (`_WIN32` / `__linux__` / `__APPLE__`) — sin
  combo, sin opción de cambiarlo, porque hoy no existe otro resultado
  posible.
- El día que Fase 8 de `avapack` habilite cross-compiling, este mismo
  campo pasa de texto fijo a combo real, **con el SO host preseleccionado
  por defecto** — el diseño de UI no cambia, solo se des-deshabilita.
  No modelar esto como una feature futura aparte; dejar el placeholder
  ya listo ahora evita rediseñar el panel más adelante.

#### 3.4.2 Tipo de build: Desktop vs BareKernel — sí elegible hoy

Este es el `--target <desktop|barekernel>` que `ava_cli build` ya
acepta y que hoy el panel ni siquiera envía (usa el default). A
diferencia de la plataforma, esta es una elección real y disponible
ahora mismo:

- Combo **"Tipo de build: Desktop / BareKernel"**, default `Desktop`.
- Si se elige `BareKernel`: mostrar el campo adicional obligatorio
  `--toolchain-dir` (cross-toolchain i686-elf: gcc/g++/ld/objcopy/nm),
  y ocultar/deshabilitar las opciones que no aplican a ese target
  (cifrado/zero-disk/firma de código son conceptos de Fase 3-7,
  pensados para el `.exe` de escritorio, no para el flujo BareKernel).

#### 3.4.3 Requisitos para empaquetar (checklist a mostrar/validar en la UI)

Siempre, para `--target desktop`:

- Toolchain de compilación del SO host (MSVC/Build Tools en Windows;
  `gcc`/`g++` en Linux/macOS) — `avapack` orquesta un build de CMake
  real, no solo copia archivos.
- CMake instalado y localizable.
- `avalang.dll` / `avalang_ui.dll`: prebuildeadas (build incremental,
  rápido, segundos) o compiladas desde cero con `--clean` (build
  limpio, un minuto o más).
- Antlr4 runtime + libglm (dependencias del compilador).
- vcpkg, si las dependencias no están ya instaladas — ya cubierto por
  el flujo "Install vcpkg" existente del panel.
- `--project <dir>` y `--entry <archivo.ava>` válidos.

Solo para `--target barekernel`:

- `--toolchain-dir` con un cross-toolchain i686-elf completo — requisito
  aparte, no compartido con el build de escritorio.

Opcionales, según qué casillas de Fase 3-7 active el usuario en el
panel:

| Opción UI | Flag | Requiere |
|---|---|---|
| Clave de cifrado fija | `--key-file <ruta>` | Un archivo de 32 bytes provisto por el usuario; sin esto, clave random por build |
| Ofuscar a bytecode | `--obfuscate` (+ `--obfuscate-strings`, `--flatten-control-flow`) | Nada extra — ya vendorizado |
| Cero disco en runtime | `--zero-disk` | Nada extra |
| Build de diagnóstico sin cifrar | `--debug` | Nada extra — **el panel debe advertir explícitamente que este `.exe` no es para distribuir** |
| Firma de código | `--sign-pfx <ruta.pfx>` (+ `--sign-password-env`, `--sign-timestamp-url`) | `signtool` (Windows SDK) — **solo Windows**; certificado `.pfx` que el usuario gestiona por su cuenta, `avapack` no lo emite |

Resultado: el path del `.exe` final (`result_path`, ya existe en
`BuildPanelState`) debe quedar accesible con un botón "Revelar en el
explorador de archivos" / "Copiar ruta", no solo en el log de texto.

## 4. Huecos de paridad con VS Code (roadmap, no bloqueante)

| Feature de VSCode | Estado en AvaStudio | Prioridad sugerida |
|---|---|---|
| Command Palette (`Ctrl+Shift+P`) | No existe | Alta — mayor impacto de descubribilidad |
| Quick Open (`Ctrl+P`) | No existe | Alta |
| Problems panel (diagnósticos) | No existe (ver §3.3) | Alta — depende de "Compilar" |
| Activity Bar | No existe como tal (menús sueltos en title bar) | Media |
| Debugger con breakpoints | No existe | Baja — esfuerzo grande, evaluar aparte |
| Marketplace de extensiones (buscar/instalar) | Existe *sistema* de plugins (`plugin_host.cpp`) pero no un catálogo/instalador | Media |
| Multi-root workspace | No existe (un proyecto a la vez) | Baja |
| Integración con control de versiones | No existe | Baja |

## 5. Diseño de UI/UX detallado por panel

Revisado cada panel existente (`src/panels/*.cpp`, `theme.cpp`,
`palette.h`) para diseñar sobre lo que ya hay, no sobre una hoja en
blanco. Base ya sólida y para mantener sin tocar: tema VSCode Dark+
(`theme.cpp`), paleta centralizada con colores de estado
success/warning/error/info (`palette.h`), iconos como cuadrados de
color por extensión (sin font de íconos — `DrawIcon()` en
`explorer_panel.cpp`), drag&drop nativo del Explorer y del Toolbox, y
tracking de cambios sin guardar con confirmación modal al cerrar
(`editor_panel.cpp`). Los criterios usados abajo: **visibilidad del
estado del sistema, prevención de errores, reconocer antes que
recordar, consistencia, y feedback inmediato** (heurísticas de
usabilidad estándar), aplicados panel por panel.

### 5.1 Explorer

Ya tiene: árbol, crear/renombrar/borrar (con confirmación), drag&drop
para mover, color por tipo de archivo. Para completar:

- **Indicador de archivo modificado** en el árbol (punto o color
  distinto en el nombre) espejando el `*` que ya usa el tab del editor
  (`tab.dirty`) — hoy esa señal solo vive en la pestaña; si el archivo
  no está abierto en ningún tab visible, no hay forma de saber que
  cambió.
- **Resaltar el archivo activo** en el árbol cuando se cambia de tab
  (sincronización editor → explorer) — VSCode lo hace por defecto
  ("Reveal in Explorer" automático); hoy no está claro si existe.
- **Buscar dentro del árbol** (filtro de texto arriba del árbol, no
  confundir con Quick Open de §4/Fase 4 — Quick Open es global al
  proyecto, esto es un filtro local del panel, útil en árboles grandes).
- Menú contextual (click derecho) con las mismas acciones que ya
  existen como botones/popups (crear, renombrar, borrar) — reduce
  fricción, patrón esperado en cualquier explorador de archivos.

### 5.2 Editor de código

Ya tiene: pestañas con `dirty` (`*`), autocompletado vía
`class_index`/`function_index`, resaltado de errores con salto
(`HighlightError`). Falta, comparado con el mínimo esperable de un
editor de texto en 2026:

- **Find in file** (`Ctrl+F`) y **Replace** (`Ctrl+H`) — no encontré
  rastro de esto en `editor_panel.cpp`; es la ausencia más notoria
  para un editor de código.
- **Find in project** (`Ctrl+Shift+F`) — buscar un texto en todos los
  `.ava`/`.avaui` del proyecto con resultados agrupados por archivo;
  se apoya en la misma infraestructura de "saltar a archivo:línea" que
  ya existe para errores.
- **Reordenar pestañas por drag** y **pin de pestaña** (fijar una
  pestaña para que no se cierre con "Close Others") — mejora de bajo
  costo, patrón ya esperado.
- El indicador `*` de `dirty` es texto plano en el label; considerar
  además un punto de color (mismo lenguaje visual que se propone para
  el Explorer arriba) para que sea reconocible de un vistazo sin leer.

### 5.3 Designer Canvas (`.avaui`)

Ya tiene: edición visual con bindings de estado, drag&drop desde
Toolbox, selección con Properties. No encontré:

- **Zoom / pan** del canvas — para pantallas con muchos componentes
  anidados esto es prácticamente obligatorio; revisar si existe antes
  de asumir que falta, porque no apareció en la búsqueda de este
  panel.
- **Undo/Redo específico del canvas** — hay `dirty` tracking pero no
  quedó claro si hay una pila de undo separada de la del editor de
  texto (son ediciones estructurales, no de texto, así que
  probablemente necesitan su propio historial).
- **Guías de alineación** (snapping) al arrastrar/redimensionar
  componentes — estándar en cualquier canvas de diseño visual
  (Figma, editores de UI de Unity/Godot), reduce el ajuste manual
  pixel a pixel.
- **Árbol de jerarquía** superpuesto o en un panel lateral (similar al
  demo de `preview_panel.cpp`, que ya dibuja un árbol de componentes
  con `ImGui::TreeNodeEx` clickeable) — hoy ese árbol existe pero está
  marcado como "demo" en el propio código (`PROPERTIES_EDITABLE` /
  nota en `engine_bridge.cpp`); formalizarlo como vista real del
  documento activo (no de un nodo de ejemplo) daría al canvas lo que
  en Figma es el panel de capas.

### 5.4 Toolbox

Ya tiene: catálogo agrupado por categoría, drag&drop, distinción visual
"(container)" para los que aceptan hijos. Bien resuelto para su
tamaño actual; el único hueco si el catálogo crece es un **filtro de
texto** arriba de la lista (mismo patrón que el filtro sugerido para
el Explorer) — no urgente mientras el catálogo (`component_catalog.csv`)
sea chico.

### 5.5 Properties

Ya tiene: tablas editables de propiedades y eventos (key/value, con
agregar/quitar fila) compartiendo el mismo componente `DrawEditableRowTable`
para ambas — buena reutilización, mantenerla como el molde para
cualquier tabla editable nueva que se agregue en otros paneles (p. ej.
variables de entorno si se decide hacer eso para "Correr", §3.2).
Sugerencias:

- **Placeholder distinto por tipo de propiedad**: hoy toda propiedad
  se edita como texto libre (según lo visto en `DrawEditableRowTable`);
  para propiedades con dominio conocido (booleanas, colores, enums del
  catálogo de componentes) un control específico (checkbox, color
  picker, combo) previene errores de tipeo y es más rápido de usar —
  se puede derivar del mismo `component_catalog.csv` que ya define
  qué propiedades acepta cada tipo.
- **Estado vacío explícito**: cuando no hay ningún componente
  seleccionado en el canvas, mostrar un mensaje ("Seleccioná un
  componente en el canvas para ver sus propiedades") en vez de una
  tabla vacía sin contexto — patrón de "empty state" que también
  aplica a Problems (§3.3) y Preview cuando no hay documento activo.

### 5.6 Terminal / Output / Logs / Problems (panel inferior agrupado)

Ver el bug de nomenclatura ya corregido en §2 (Output vs Logs con el
mismo título de ventana) — es lo primero a resolver acá, antes de
agrupar. Una vez resuelto:

- **Nombres claros y distintos** en las pestañas del panel agrupado:
  `Terminal` (proceso interactivo), `Output` (stdout/stderr de la
  última ejecución/build), `Logs` (log interno del IDE — nivel debug,
  no pensado para el usuario final salvo que esté reportando un bug),
  `Problems` (nuevo, §3.3) — cuatro identidades sin solapamiento, cada
  una con su propósito declarado en un tooltip si el nombre no alcanza.
- **Badge de conteo** en la pestaña `Problems` (ej. "Problems (3)")
  igual que hace VSCode — visibilidad del estado del sistema sin tener
  que abrir la pestaña.
- `logs_panel.cpp` ya tiene selección de texto y copiar (`CopySelection`,
  `CopyAll`) — buen nivel de detalle ya resuelto; llevar la misma
  capacidad de copiar a `Output` y `Problems` si no la tienen aún, por
  consistencia entre las cuatro pestañas del mismo panel.

### 5.7 Build panel

Ya cubierto en detalle en §3.4. Complementar con buenas prácticas de
formulario:

- **Deshabilitar el botón "Build" mientras hay un build en curso** (ya
  probablemente cierto dado que corre en `std::thread` con log en
  vivo) y mostrarlo como `"Building..."` con spinner, no solo un log
  que crece — refuerza que el sistema está ocupado sin tener que leer
  el log.
- **Validación inline antes de habilitar Build**: si el entry file
  detectado no existe, o si `--target barekernel` está seleccionado
  sin `--toolchain-dir`, el botón debe quedar deshabilitado con un
  motivo visible al lado (tooltip o texto en rojo), no fallar recién
  al hacer click — coherente con el "checklist" de requisitos definido
  en §3.4.3.

### 5.8 Settings — rediseño: ventana centralizada con tabs por sección, independiente de Plugins

Objetivo general que pidió el usuario y que aplica a esta sección en
particular más que a ninguna otra: **AvaStudio tiene que ser intuitivo
y fácil de configurar y usar** — la Settings window es, literalmente,
el lugar donde eso se pone a prueba primero. Dos hallazgos revisando
`settings_panel.cpp` y `titlebar_panel.cpp` que definen el rediseño:

1. **Hoy Settings y Plugins están mezclados**, justo lo que se pidió
   evitar: la sidebar de `DrawSettingsPanel` tiene un grupo "General"
   (con un único ítem, mal nombrado "Editor" — más abajo) y un grupo
   "Plugins" con un `Selectable` por plugin, todos en la misma lista
   plana, misma jerarquía visual, sin distinción real entre
   "configuración propia de la IDE" y "configuración de una extensión
   de terceros".
2. **Ya existe una superficie separada para plugins** y no se está
   usando: el menú `Extensions` (`titlebar_panel.cpp`) abre su propio
   modal (`"Plugins##PluginsModal"`) con la lista de plugins
   encontrados en `plugins/`, checkbox de enable/disable, aplicado en
   caliente sin reiniciar. Es decir: la separación que se pide **ya
   está construida a medias** — solo falta terminarla, no inventarla.
3.  Bug de nombre encontrado de paso: la sidebar muestra el grupo
   `"General"` pero el único ítem adentro dice `"Editor"` y selecciona
   `g_selected_plugin_index == -1`, que en realidad dibuja
   `DrawGeneralSection` (el campo `modules_path`) — el label no
   corresponde al contenido. Se arregla solo, como consecuencia del
   rediseño de abajo.

#### 5.8.1 La regla de separación

- **Settings** (esta ventana) contiene únicamente configuración propia
  de AvaStudio — nunca nada que dependa de qué plugins estén
  instalados.
- **Extensions** (el modal ya existente, ampliado — ver §5.11) es
  donde vive todo lo relacionado a plugins: instalar/activar/
  desactivar, y también la configuración propia de cada plugin (el
  `RegisteredPanel` que hoy aparece mal ubicado dentro de la sidebar
  de Settings se mueve para acá, ver §5.11).
- Ningún panel de un plugin vuelve a aparecer dentro de la ventana
  Settings bajo ninguna circunstancia — esa es la regla, no una guía.

#### 5.8.2 Layout: tabs, no sidebar

El usuario pidió específicamente tabs por sección (en vez de la
sidebar actual) — con el contenido de Settings ya acotado a "solo lo
propio de AvaStudio" (sin plugins), una fila de tabs horizontal arriba
de la ventana es más liviana que una sidebar de 200px que ya no
necesita alojar una lista potencialmente larga de plugins:

```
+--------------------------------------------------------------+
| [ Buscar configuración...                              ]     |
+--------------------------------------------------------------+
| General | Editor | Build & Package | Idioma | Apariencia |    |
| Atajos de teclado | Terminal                                 |
+--------------------------------------------------------------+
|                                                                |
|   (contenido de la tab activa)                                |
|                                                                |
+--------------------------------------------------------------+
```

- **Barra de búsqueda arriba de las tabs** (nuevo, patrón VSCode
  Settings): filtra por nombre/descripción de cualquier opción en
  cualquier tab y salta directo a la tab que la contiene — evita tener
  que saber de memoria en qué tab vive cada cosa. No bloqueante para
  el resto del rediseño (se puede sumar en una fase propia si conviene
  acotar la Fase 0.5-i18n primero, ver §9).
- **Tabs con `ImGui::BeginTabBar`/`BeginTabItem`** en vez de
  `Selectable` en una child window — es el widget que ImGui ya provee
  para esto exacto, sin reinventar nada.

#### 5.8.3 Contenido de cada tab

Sintetiza lo que ya se diseñó en otras secciones de este plan, para
que cada opción tenga un solo lugar donde vivir (mismo principio de
§7.2):

| Tab | Contiene | De dónde sale |
|---|---|---|
| **General** | `modules_path` (lo que hoy está mal etiquetado "Editor") | Ya existe, solo se renombra bien |
| **Editor** | Tamaño de fuente, tamaño de tabulación/indentación, ajuste de línea (word wrap), autoguardado | Nuevo — huecos del editor no cubiertos hoy |
| **Build & Package** | Solo lo que es "configurar una vez, usar siempre": ruta de vcpkg (`build_vcpkg_root`), ruta por defecto de certificado de firma (`.pfx`), carpeta de salida por defecto. **No** duplica los campos que ya viven en el Build panel (Plataforma detectada, Tipo de build, entry file) — esos son decisiones por-build, no configuración persistente; ver la distinción en §5.8.4 | §3.4 |
| **Idioma** | Combo `language` (en/es) | §6.4 |
| **Apariencia** | Tema (hoy solo "VSCode Dark+" vía `theme.cpp` — el combo queda con una sola opción por ahora, preparado para un tema claro a futuro sin rediseñar la tab); tamaño de fuente del chrome de la IDE (distinto del tamaño de fuente del editor, que vive en la tab Editor) | Nuevo |
| **Atajos de teclado** | Tabla de solo lectura con todos los shortcuts existentes (F5, Shift+F5, Ctrl+B, Ctrl+Shift+B, Ctrl+P, Ctrl+Shift+P, Ctrl+F, Ctrl+Shift+F) — sourceada del mismo registro que va a alimentar el Command Palette (§4/Fase 3), no una lista aparte que se puede desincronizar | §5.9, §4 |
| **Terminal** | Shell por defecto, directorio de trabajo inicial | Nuevo |

#### 5.8.4 Buenas prácticas aplicadas (por qué queda así y no de otra forma)

- **"Configurar una vez" vs. "decidir en el momento"**: cualquier
  opción que cambie build a build (plataforma, tipo de build, entry
  file) se queda en el Build panel donde ya está, porque es contexto
  de esa acción puntual; solo lo que es verdaderamente una preferencia
  persistente (dónde está mi vcpkg, dónde está mi certificado) entra a
  Settings. Mezclar ambas cosas en un solo lugar termina en un
  formulario gigante donde nadie encuentra nada — es el mismo motivo
  por el que VSCode separa `launch.json`/`tasks.json` (por proyecto,
  contextual) de `settings.json` (persistente).
- **Aplicación en caliente, sin reiniciar**: mismo criterio ya fijado
  para Idioma (§6.4) — extendido a toda la ventana: ningún cambio en
  Settings debería requerir reiniciar AvaStudio. Si alguna opción
  futura sí lo requiriera, se marca explícitamente con un tag "requiere
  reiniciar" al lado — la excepción visible, no la regla implícita.
- **Un control por tipo de dato**, no todo `InputText`: booleano →
  checkbox, elección cerrada (idioma, tema) → combo, ruta → campo +
  botón "Browse..." (ya el patrón que usa `modules_path` hoy), número
  con rango (tamaño de fuente) → slider o stepper con min/max, no un
  campo de texto libre donde se puede tipear cualquier cosa — mismo
  principio ya propuesto para Properties en §5.5.
- **Ningún campo vacío sin explicación**: cada opción lleva un
  subtítulo corto atenuado debajo (mismo `ImGui::TextDisabled` usado en
  el resto de la IDE) explicando qué hace, para no depender de que la
  persona ya sepa qué es "vcpkg root" — reduce la necesidad de
  documentación externa.
- **Botón "Restaurar valores por defecto"** por tab (no solo global) —
  permite deshacer una mala configuración de una sección sin resetear
  todo lo demás.
- **La ventana en sí sigue siendo dockeable/no-modal** como ya es hoy
  (`ImGui::Begin("Settings", p_open)`) — no se convierte en un modal
  bloqueante; abrir Settings no debería interrumpir lo que se está
  haciendo en el Editor/Canvas al lado.

### 5.9 Title bar / menús

Ya tiene File, toggle de paneles, Run (con shortcuts anotados: F5,
Ctrl+B), Preferences. Con la llegada de la Activity Bar (Fase 7 del
plan de fases, §6), el menú de "toggle de paneles" pasa a vivir ahí en
vez de en un menú de texto — dejarlo anotado para no duplicar esa
función en dos lugares una vez que la Activity Bar exista.

### 5.10 Consistencia transversal (aplica a todos los paneles)

- **Un solo lenguaje de "estado no guardado"**: el `*` en pestañas
  (editor) y el color propuesto en el árbol (explorer, §5.1) deben ser
  la misma señal visual, no dos convenciones distintas para la misma
  idea.
- **Un solo lenguaje de "vacío"**: todo panel que puede no tener nada
  que mostrar (Properties sin selección, Problems sin errores, Preview
  sin documento) usa el mismo estilo de mensaje centrado y atenuado
  (`ImGui::TextDisabled`, ya usado en varios paneles existentes como
  Toolbox) en vez de dejar el panel en blanco sin explicación.
- **Shortcuts descubribles**: todo atajo de teclado nuevo (Ctrl+F,
  Ctrl+Shift+F, Ctrl+Shift+B, Shift+F5, Ctrl+P, Ctrl+Shift+P) debe
  aparecer como texto al lado del ítem de menú correspondiente — ya es
  el patrón que sigue `titlebar_panel.cpp` hoy (`"Run Script", "F5"`);
  mantenerlo para todo lo nuevo, y una vez que exista el Command
  Palette (Fase 3), listar ahí exactamente los mismos atajos sin que
  ninguno quede huérfano en un solo lugar.

## 6. Multilenguaje (i18n)

Revisado el código: **no existe ninguna infraestructura de i18n hoy**
— todos los strings de UI están hardcodeados como literales en cada
`ImGui::Text(...)`/`ImGui::Begin(...)`, y ya están **mezclados
inconsistentemente entre español e inglés** en el propio código actual
(ejemplos reales encontrados: `toolbox_panel.cpp` tiene
`"Arrastrá un control al lienzo de Design"` en español, mientras
`explorer_panel.cpp` tiene `"Delete \"%s\"?"` en inglés, y
`main.cpp` tiene `"no hay ningun archivo abierto para ejecutar"` en
español). Antes de sumar más idiomas hay que resolver esta
inconsistencia, no construir el sistema de traducción encima de ella.

### 6.1 Qué se traduce y qué no

- **Se traduce**: todo el "chrome" de la IDE — menús, títulos de panel,
  botones, tooltips, mensajes de confirmación/error propios de
  AvaStudio (ej. "Delete file?", "Unsaved Changes"), labels de
  Settings.
- **No se traduce**: la sintaxis de AvaLang en sí (`class`, `func`,
  `this`, `base`, etc. — son palabras del lenguaje, no de la interfaz);
  nombres de archivo, rutas, y contenido que el usuario escribió.
- **Zona gris, a decidir en su propia fase**: los mensajes de error que
  emite el compilador/VM (hoy en el idioma que sea que el compilador
  los genere). Traducirlos requeriría que el compilador soporte
  mensajes localizados, lo cual es una decisión de `core/`, no de
  AvaStudio — queda **fuera de alcance de este plan** (ver §8);
  AvaStudio los muestra tal cual los recibe.

### 6.2 Arquitectura propuesta

- **Tabla de strings por locale en CSV**: `data/langs/en.csv`,
  `data/langs/es.csv` (dos columnas: `key,value`), parseados con el
  mismo `util/csv.cpp` (`ParseCsv`) que ya usan `keyword_docs.csv` y
  `builtin_signatures.csv` — mismo patrón que el proyecto ya usa para
  separar datos de código, sin agregar ninguna librería nueva.
- **Función `Tr(key)`** (`util/i18n.h`/`.cpp`, nuevo): devuelve el
  string traducido para el locale activo; si la key no existe en ese
  locale, cae al inglés; si tampoco existe ahí, devuelve la key cruda
  entre corchetes (ej. `[panel.explorer.title]`) — visible a propósito
  durante desarrollo para detectar traducciones faltantes, en vez de
  fallar silenciosamente o crashear.
- **Reemplazo progresivo, no todo de una vez**: cada panel migra sus
  literales a `Tr("...")` en su propia fase (ver §6.5) — con ~46.7k
  líneas repartidas en ~13 paneles, migrar todo junto es alto riesgo;
  panel por panel es consistente con cómo se viene trabajando el resto
  del proyecto (zip por fase).

### 6.3 El detalle técnico que puede romper todo si se pasa por alto

ImGui usa el string pasado a `Begin(...)` como **identificador de la
ventana**, no solo como texto visible — es lo mismo que ya se detectó
como bug real en §2/§5.6 (colisión `"Output"` entre dos paneles
distintos). Si "Explorer" se traduce ingenuamente a "Explorador" al
cambiar de idioma, ImGui lo trata como **una ventana nueva**: se pierde
el layout de docking guardado (`imgui.ini`) cada vez que alguien
cambia el idioma. Solución estándar de ImGui para este caso: separar
label visible de ID fijo con el sufijo `###`, que fuerza el ID a lo
que sigue de `###` sin importar qué texto venga antes —

```cpp
// Mal: el ID de la ventana cambia con el idioma, se pierde el docking
ImGui::Begin(Tr("panel.explorer.title"), p_open);

// Bien: label traducido, ID estable entre idiomas
std::string title = std::string(Tr("panel.explorer.title")) + "###explorer_panel";
ImGui::Begin(title.c_str(), p_open);
```

Esto aplica a **todos** los `ImGui::Begin(...)` de todos los paneles
(§5 los lista uno por uno) — no es opcional para ninguno, es la
diferencia entre "cambiar de idioma" y "cambiar de idioma y que se
reordenen todos los paneles".

### 6.4 Dónde vive la opción y cómo se aplica

- **Setting nuevo**: `language` (ej. `"en"` / `"es"`) en
  `StudioSettings` (`util/settings.cpp`), mismo mecanismo de
  persistencia que ya usan `build_entry_file`/`modules_path` — no hace
  falta un sistema de persistencia nuevo.
- **Ubicación en UI**: sección "General" de Settings (§5.8, ya tiene
  sidebar de categorías) — un combo simple, no una sección aparte.
- **Aplicación en caliente**: dado que ImGui redibuja cada frame desde
  cero, no hace falta reiniciar la app — cambiar el índice de locale
  activo y el próximo frame ya sale traducido, siempre que se haya
  seguido la regla de `###` de §6.3 en todos los `Begin(...)`.
- **Idiomas de arranque**: **español e inglés**, porque son los dos
  que ya conviven (mal) en el código actual — cualquier otro idioma
  queda para después de que estos dos estén completos y consistentes.
  La fuente (`embedded_font.cpp`) ya cubre esto sin trabajo extra:
  usa `GetGlyphRangesDefault()`, que incluye Basic Latin + Latin-1
  Supplement (tildes, ñ, ¿¡) — agregar francés/portugués/alemán a
  futuro tampoco requeriría tocar la fuente; sí lo requeriría cirílico,
  CJK, etc., que quedan fuera de alcance de este plan.

### 6.5 Plan de fases de i18n (se intercala con §9)

| Fase | Contenido |
|---|---|
| **i18n-0** | `util/i18n.h/.cpp` + `Tr()` + `data/strings_en.csv`/`strings_es.csv` vacíos salvo un par de keys de prueba; setting `language` en Settings → General; aplicar el patrón `###` (§6.3) a **todos** los `Begin(...)` existentes aunque el texto siga hardcodeado por ahora — esto es lo que no se puede migrar "después" sin romper el docking de todo el mundo |
| **i18n-1** | Migrar Title bar + menús (punto de entrada más visible, y ya tiene la convención de mostrar shortcuts al lado que hay que preservar) |
| **i18n-2..N** | Un panel por fase, mismo orden que §5 (Explorer, Editor, Canvas, Toolbox, Properties, panel inferior agrupado, Build, Settings) — cada fase resuelve además cualquier string de ese panel que hoy esté en el idioma "equivocado" respecto de lo que se decida como fuente de verdad (inglés como key canónica, español como primera traducción completa) |

Esta sección se intercala con el plan de fases de §9 (no lo reemplaza)
— dónde exactamente conviene alternar depende de cuánto se quiera
avanzar en paralelo, algo a decidir cuando se arranque a ejecutar.

## 7. Convenciones transversales: centralización y sin comentarios en el código fuente

Dos reglas nuevas, transversales a todo lo diseñado arriba. Corrijo acá
la interpretación de la iteración anterior de este plan: la regla de
"sin comentarios" **no** es sobre los `.ava`/`.avaui` generados para
proyectos de usuario (eso ni se había planteado) — es sobre **el
código fuente de los propios proyectos de AvaLang**: AvaStudio, y por
extensión el resto de los componentes C++ del ecosistema (`ava_cli`,
`avapack`, `core`, `ui`).

Esto es un cambio de convención real, no cosmético: revisando el
código para este plan encontré comentarios de razonamiento en casi
todos los archivos — solo `properties_panel.cpp` tiene 72 líneas de
comentario propias, y `util/csv.h` documenta cada función con varias
líneas de contexto. Esa convención venía siendo, hasta ahora, parte de
cómo se trabaja el proyecto entre sesiones (junto con `bugs.md`,
`AVAUI_PROGRESS.md`, etc.). Adoptar "sin comentarios" de acá en más es
una decisión legítima — código autoexplicativo por nombres claros y
buena estructura en vez de prosa al lado — pero cambia cómo hay que
dejar registro de decisiones de diseño que hoy vivían en el comentario
mismo (esas van a tener que migrar a los `.md` de progreso/plan, no
desaparecer).

**Decidido**: rige para ambos casos — todo el código nuevo se escribe
sin comentarios desde ya, **y además** hay que sacar los comentarios
que ya existen en las ~46.7k líneas actuales. Es el trabajo de más
alcance/riesgo de este plan (toca todo el codebase, no un panel a la
vez), así que se trata como su propia fase — **Fase 0.5 "Limpieza de
comentarios"** en §9 — separada de cualquier otro cambio funcional,
para que un diff de "sacar comentarios" no se mezcle nunca con un diff
que además cambia comportamiento (si algo se rompe, se sabe
exactamente por qué). El punto de `bugs.md`/`AVAUI_PROGRESS.md`
dependiendo de razones hoy documentadas en comentarios sigue en pie:
antes de borrar un comentario que explica un "por qué" no trivial, esa
razón se traslada al `.md` de progreso correspondiente — se pierde el
comentario, no el conocimiento.

### 7.1 Sin comentarios en el código fuente (AvaStudio y proyectos de AvaLang)

- Ningún `.cpp`/`.h`, nuevo o existente, lleva comentarios de ningún
  tipo — ni de cabecera de archivo, ni inline, ni de "por qué se hizo
  así". Las decisiones de diseño que hoy se explicarían en un
  comentario pasan a vivir en los documentos de plan/progreso del
  proyecto (mismo lugar donde ya vive `plan_ava_pack.md`,
  `AVAUI_PROGRESS.md`, `bugs.md`), no en el código.
- Nombres de función/variable/tipo tienen que cargar el peso que antes
  llevaba el comentario — si una función necesitaría un comentario
  para entenderse, el nombre está mal elegido, no falta el comentario.
- Aplica a todo el código nuevo que salga de este plan (paneles
  nuevos, `util/i18n.*` de §6, cualquier archivo de Fase −1 en
  adelante, §9) **y** a la limpieza retroactiva de Fase 0.5.
- **No** aplica al código `.ava`/`.avaui` que AvaStudio genera para
  los proyectos de los usuarios (eso quedó fuera de esta regla — si
  también se quiere sin comentarios ahí, es una decisión aparte a
  confirmar cuando se diseñe el scaffolding de Fase 5/6).

### 7.2 Centralización


Ya es un patrón que este mismo plan viene reforzando sin nombrarlo
todavía como principio explícito — formalizarlo acá para que aplique
parejo a todo lo nuevo, no solo a lo que ya se diseñó así por
casualidad:

- **Colores**: `palette.h` (ya existe, todo el tema pasa por ahí —
  mantener, no volver a hardcodear un hex en un panel nuevo).
- **Strings de UI**: las tablas `langs/en.csv`/`langs/es.csv` de
  §6 — ninguna traducción vive suelta dentro de un `.cpp`.
- **Datos tabulares** (catálogo de componentes, docs de keywords,
  firmas de funciones): ya centralizados en CSV bajo `data/` y leídos
  con `util/csv.cpp` — mismo criterio para cualquier tabla nueva que
  se necesite (por ejemplo, si Fase 5/6 termina necesitando una lista
  de plantillas de proyecto, esa lista va en un CSV/JSON centralizado,
  no hardcodeada dentro del wizard).
- **Configuración persistida**: `StudioSettings` / `util/settings.cpp`
  — todo setting nuevo (`language` de §6, opciones de build de §3.4)
  entra ahí, no en una variable estática suelta dentro de un panel.
- **Heurísticas compartidas entre features**: `DetectEntryFile()` es
  el ejemplo ya señalado en §3.2 — hoy vive solo dentro de
  `build_panel.cpp` pero la usan (o deberían usar) tanto Build como
  Run Project; la Fase 1 del plan de fases ya la mueve a un lugar
  compartido por esta misma razón. Cualquier lógica que dos paneles
  necesiten igual debe vivir en un solo sitio (`util/` o similar), no
  reimplementarse en cada panel por separado.
- **Regla general para todo lo que se diseñe de acá en adelante en
  este plan**: si un valor, string, o snippet se necesita en más de un
  lugar — o incluso en uno solo pero es del tipo "esto podría cambiar
  y no quiero tocar 5 archivos" — no va como literal inline; va en la
  fuente centralizada que le corresponda de la lista de arriba.

## 8. Fuera de alcance de este plan

- El motor del compilador/VM en sí (`core/`) — este plan es sobre la
  IDE, no sobre el lenguaje.
- El plugin `ai_agent` como reemplazo de cualquiera de las cuatro
  operaciones — es una capacidad adicional, no el mecanismo base de
  generar/correr/compilar/empaquetar.
- Definir el formato de `launch.json`-equivalente en detalle (si se
  decide incluirlo) — se deja como decisión de una fase posterior una
  vez cerrado el alcance de "Correr".

## 9. Plan de fases

Orden pensado para que cada fase deje algo usable por sí sola, sin
esperar a que estén todas — mismo criterio que `plan_ava_pack.md`. Dos
decisiones de alcance de las secciones anteriores quedaron abiertas;
acá se resuelven con un default razonado, marcado explícitamente para
que se pueda pisar sin reabrir todo el plan:

- §3.3 (Compilar): bajo demanda, no automático al guardar — ya
  justificado ahí por la falta de infraestructura de watcher.
- §3.1 (Generar): el wizard de "nuevo proyecto" entra recién en Fase 6
  (abajo), no antes — porque no bloquea nada de Correr/Compilar/
  Empaquetar y "nuevo archivo en un proyecto existente" (Fase 5) cubre
  el caso de uso más común primero.

| Fase | Contenido | Por qué en ese orden |
|---|---|---|
| **−1** | Renombrar el título de ventana duplicado entre `output_panel.cpp` y `logs_panel.cpp` (ambos usan `"Output"` hoy, ver §2/§5.6) | Bug de identidad de ventana; arreglarlo antes de que Fase 2 agrupe estos paneles evita heredar el problema a la UI nueva |
| **i18n-0** | `util/i18n.h/.cpp` + `Tr()` + CSVs de locale + setting `language`; aplicar el patrón `###` (§6.3) a todos los `Begin(...)` existentes (§6.5) | Mismo motivo que Fase −1: es un fix de identidad de ventana, conviene resolverlo junto con esa fase y antes de que se agreguen más paneles nuevos (Problems, etc.) que también tendrían que llevar `###` desde el día uno |
| **0** | Separar en el Build panel el campo "Plataforma" (solo lectura, detectada) del combo "Tipo de build: Desktop/BareKernel" (§3.4.1–3.4.2); pasar `--target` real a `ava_cli build` (hoy no se envía) | Ya diseñado en detalle en §3.4; es la corrección más chica y de menor riesgo, no toca ninguna pieza nueva de UI |
| **0.5** | Limpieza de comentarios (§7.1): sacar todos los comentarios de las ~46.7k líneas actuales de AvaStudio (y del resto de proyectos de AvaLang que apliquen), migrando a `.md` de progreso cualquier razón de diseño no trivial que se perdería al borrar el comentario; sin cambios de comportamiento en el mismo diff | Es puramente mecánico (no toca lógica), así que conviene resolverlo temprano, antes de que haya más código nuevo escrito ya sin comentarios que se mezcle con el código viejo que todavía los tiene |
| **1** | Mover `DetectEntryFile()` a un lugar compartido (fuera de `build_panel.cpp`); agregar `Run Project` (`Shift+F5`) usando esa heurística (§3.2) | Depende solo de código ya escrito; sin esto, Check/Problems (Fase 2) no tiene un "proyecto completo" claro sobre el cual correr |
| **2** | Problems panel + modo Check (`Ctrl+Shift+B`), reutilizando `HighlightError`/`ClearErrorHighlights`/`OpenFileInTab` ya existentes (§3.3); unificar que los errores de Run también alimenten el Problems panel; agrupar Terminal/Output/Logs/Problems en el panel inferior con nombres distintos (§5.6) y badge de conteo en Problems | Es el hueco de paridad marcado "Alta" en §4 con más impacto, y la mitad de la plomería (highlight, salto a archivo) ya existe |
| **2.5** | Find in file (`Ctrl+F`) y Find in project (`Ctrl+Shift+F`) en el editor (§5.2) | Ausencia más notoria del editor hoy; se apoya en la misma infraestructura de "saltar a archivo:línea" recién formalizada en Fase 2 |
| **3** | Command Palette (`Ctrl+Shift+P`) — buscador sobre las acciones que ya existen hoy: Run, Run Project, Check, Build, toggle de paneles, Install vcpkg | No depende de las fases anteriores en términos técnicos, pero tiene más sentido una vez que "Run Project" y "Check" ya son comandos reales que listar |
| **4** | Quick Open (`Ctrl+P`) | Independiente, se puede intercalar antes si conviene por carga de trabajo |
| **5** | "Generar": nuevo archivo `.ava`/`.avaui` dentro de un proyecto existente, con boilerplate correcto (constructor estilo C#) | Cubre el caso de uso principal de "Generar" sin necesitar diseño de manifest ni de plantillas de proyecto |
| **6** | Wizard de "nuevo proyecto" (carpeta + `main.ava` inicial, siguiendo la misma convención que ya detecta `DetectEntryFile()`) | Deliberadamente al final de lo "core": no bloquea Correr/Compilar/Empaquetar, y como no hay manifest de proyecto hoy, el wizard puede ser tan simple como crear la carpeta + `main.ava` sin inventar un formato nuevo |
| **7** | Activity Bar (reemplaza el menú "Panels" disperso de la title bar) | Cosmético/estructural; mejor una vez que ya existen más comandos/paneles nuevos (Problems, Command Palette) que vale la pena que la Activity Bar organice |
| **8+** | Roadmap sin comprometer: Marketplace de extensiones sobre `plugin_host.cpp`, Debugger con breakpoints, multi-root workspace, integración VCS | Marcadas "Media/Baja" en §4; esfuerzo grande o dependen de decisiones que este plan no cierra (p. ej. formato de extensión distribuible) |

Cada fase se entrega con zip al final, como viene siendo la dinámica
del resto del proyecto, y no arranca la siguiente hasta que la
anterior esté confirmada.

## 10. Progreso

Checklist vivo — se actualiza al cerrar cada fase, no se reordena ni
se reescribe la tabla de §9/§6.5 (esta sección solo marca estado sobre
esas mismas filas).

| Fase | Estado | Notas |
|---|---|---|
| **−1** | ✅ Hecha | `output_panel.cpp/.h` (huérfano, no compilaba) borrado; `logs_panel.cpp` renombrado de `"Output"` a `"Logs"` en `Begin()`, `builtin_panels.h` y las 3 referencias en `main.cpp` |
| **i18n-0** | ✅ Hecha | `util/i18n.h/.cpp` + `Tr()` + `data/strings_en.csv`/`strings_es.csv` (keys de prueba) + setting `language` (persistido, combo en Settings → General, aplicación en caliente) + patrón `###id` aplicado a los 9 `Begin()` existentes (Explorer, Build, Terminal, Settings, Properties, Preview, Toolbox, Code Editor, Logs) y propagado a `kBuiltinPanelNames`, `DockBuilderDockWindow`, `panel_open`/`persist_if_closed`, atajos Ctrl+,/Ctrl+B |
| **i18n-1** | ✅ Hecha | `titlebar_panel.cpp` migrado a `Tr()`: botones File/View/Run/About, ítems de los 3 menús desplegables, y los modales About/Plugins (título + cuerpo + botón Close). ~30 keys nuevas en `strings_en.csv`/`strings_es.csv` bajo `menu.*`/`about.*`/`plugins.*`/`common.close`. De paso, corregido el mezclado de idioma real que mencionaba §6 (F7 "Ver Código"/"Ver Diseño" y "(no cargado)" del modal de Plugins, hardcodeados en español) con key canónica en inglés. Título de los modales About/Plugins reconstruido cada frame como `Tr(...) + "##Id"` (mismo criterio de ID estable que `###` en paneles, aplicado a popups) para que `OpenPopup`/`BeginPopupModal` sigan coincidiendo en cualquier idioma. Fuera de esta fase (le toca a cada panel en su propia i18n-N): los nombres de panel que lista el menú View, tomados de `kBuiltinPanelNames`/paneles de plugin sin tocar |
| **reorg `data/`** | ✅ Hecha | Reestructurado `data/` en subcarpetas: `langs/` (`en.csv`/`es.csv`, antes `strings_en.csv`/`strings_es.csv`), `docs/` (`keyword_docs.csv`, `builtin_signatures.csv`), `design/` (`component_catalog.csv`). Actualizadas todas las rutas hardcodeadas (`i18n.cpp`, `keyword_docs.cpp`, `builtin_signatures.cpp`, `component_catalog.cpp`) y sus comentarios (`.h` correspondientes, `csv.h`, `data_dir.h`, `toolbox_panel.cpp`). `CMakeLists.txt` no necesitó lógica nueva — ya copiaba `data/` completo con `copy_directory`, recursivo por naturaleza; solo se actualizó el comentario. `data/README.md` reescrito con el árbol de carpetas y una sección nueva para `langs/` |
| **i18n-2** | ✅ Hecha | `explorer_panel.cpp` migrado a `Tr()`: título del panel (`Explorer###explorer` → `Tr("panel.explorer.title") + "###explorer"`, mismo criterio de ID estable que title bar/reorg), popup Nueva Carpeta/Nuevo Archivo + botón Crear, modal de confirmación de borrado (título reconstruido cada frame igual que About/Plugins, vía key propia `explorer.delete_title` en vez de concatenar "?" a `explorer.delete` — en español rompía la apertura de "¿"), popup Rename, menú contextual por entrada y el del root (Nuevo Archivo/Carpeta, Abrir en el Explorador de Archivos, Renombrar F2, Eliminar Del), y el mensaje de carpeta no encontrada. Para los 3 mensajes con `%s` (confirmación de borrado de carpeta/archivo, carpeta no encontrada) se agregó un helper local `TrFormat(key, arg)` que sustituye el `%s` del valor ya traducido *antes* de pasarlo a ImGui — evita depender de que el CSV editable por traductores sea también un format-string válido de printf. `builtin_panels.h` reestructurado: `kBuiltinPanelNames` pasa de `array<string_view>` a `array<BuiltinPanelInfo>` (`id` + `fallback_label` + `tr_key`) para que el menú View pueda mostrar el nombre traducido de los paneles ya migrados (por ahora solo Explorer) y seguir mostrando el literal de siempre en los que aún no lo están; `titlebar_panel.cpp`/`.h` actualizados para leer esa estructura. ~13 keys nuevas en `en.csv`/`es.csv` bajo `explorer.*`/`panel.explorer.title`/`common.cancel` |
| **i18n-3** | ✅ Hecha | `editor_panel.cpp` migrado a `Tr()`: título del panel (`Code Editor###code_editor` → `Tr("panel.editor.title") + "###code_editor"`, mismo criterio de ID estable que Explorer/title bar), mensaje "sin archivo abierto", pestaña Welcome completa (logo + subtítulo — reutiliza `about.tagline` — + botones New File/Open File/Open Folder — reutiliza `explorer.new_file`/`menu.file.open_folder` — + hint final), `DisplayName()` ("Welcome"/"Untitled"), los dos tooltips de hint (parámetros de función y palabra clave: badges BUILT-IN/FUNCION/PALABRA CLAVE, labels QUE HACE/COMO SE ESCRIBE/EJEMPLO, "Opcion N", "Definida en", nota de builtin sobreescribible — con correcciones de acentos faltantes en español, mismo criterio que i18n-1/i18n-2), los dos banners de error de `.avaui` (lectura en Design view, parseo en Code view) y el modal "Unsaved Changes" (título con patrón de ID estable igual que el delete-confirm de Explorer, botones Save/Don't Save/Cancel — reutiliza `menu.file.save` y `common.cancel`). Deliberadamente sin migrar: los separadores de sintaxis `(`/`, `/`)` de la firma de función en el tooltip de parámetros (puntuación de código AvaLang, no chrome de idioma humano) y los hints `Ctrl+N`/`Ctrl+O` (atajos, mismo criterio que el resto de la codebase). El helper `TrFormat` de i18n-2 se generalizó a `TrFormat(key, {arg1, arg2, ...})` (initializer_list) para los mensajes de avaui con dos sustituciones. 25 keys nuevas en `en.csv`/`es.csv` bajo `panel.editor.title`/`editor.*`, con reutilización de `about.tagline`, `explorer.new_file`, `menu.file.open_folder`, `menu.file.save` y `common.cancel` en vez de duplicar |
| **i18n-4** | ✅ Hecha | `designer_canvas.cpp` migrado a `Tr()`: el hint "Arrastrá un control acá" que se dibuja en un contenedor vacío (`canvas.drop_hint`), el label "Dialogs (non-visual):" de la bandeja de diálogos (`canvas.dialogs_label`), el `MenuItem` "Delete" del menú contextual de nodo (reutiliza `explorer.delete`, mismo verbo que Explorer) y el modal de confirmación de borrado de nodo -- título reconstruido cada frame con patrón de ID estable (`Tr("canvas.delete_title") + "##CanvasDeleteConfirm"`, mismo criterio que Explorer/Editor), mensaje con `TrFormat("canvas.delete_confirm", label)`, y los botones Delete/Cancel reutilizando `explorer.delete`/`common.cancel` y el texto "no se puede deshacer" reutilizando `explorer.delete_undone` en vez de duplicar. También los dos banners de estado que se dibujan directamente sobre el canvas (visibles al usuario, no solo logueados): error de render (`canvas.render_error` + `canvas.render_error_hint`) y nodos sin layout (`canvas.missing_layout_nodes`, con el conteo pasado como string vía `TrFormat`). Se agregó a este archivo su propio helper local `TrFormat(key, {args...})`, mismo patrón que explorer_panel.cpp/editor_panel.cpp (helper file-local, no compartido, para no introducir un acoplamiento nuevo entre paneles). Deliberadamente sin migrar: el separador `">"` del breadcrumb (puntuación universal, mismo criterio que los separadores de sintaxis de Editor) y el tooltip que muestra `type_name` al pasar el mouse sobre un contenedor (es un valor dinámico -- el nombre del tipo de componente -- no un literal de chrome de AvaStudio). Zona gris resuelta explícitamente fuera de esta fase: los dos `log_bridge->Log(...)` de este archivo (fallo de `BuildLiveRender`, nodos sin `uid_to_rect`) NO se migraron -- son diagnóstico interno con jerga técnica (`[designer_canvas]`, `uid_to_rect`) que va al panel de Logs, cuyo propio chrome todavía no está migrado (le toca a "panel inferior agrupado", más adelante en el orden de §6.5); decidirlos junto con esa fase evita tener que revisar el criterio dos veces. `designer_canvas.cpp` no tiene su propio `Begin()` (se dibuja embebido en las pestañas del Editor vía `BeginChild("##DesignerCanvas", ...)`, que ya usa un ID fijo con `##` sin label visible), así que no hizo falta ningún `panel.canvas.title` ni el patrón `###`. 7 keys nuevas en `en.csv`/`es.csv` bajo `canvas.*`, con reutilización de `explorer.delete`, `explorer.delete_undone` y `common.cancel` en vez de duplicar -- verificadas con el parser CSV real (77 filas totales, 0 rotas) y balance de llaves/paréntesis del archivo confirmado (190/190, 842/842) |
| **i18n-5** | ✅ Hecha | `toolbox_panel.cpp` migrado a `Tr()`: título del panel (`Toolbox###toolbox` → `Tr("panel.toolbox.title") + "###toolbox"`, mismo criterio de ID estable que Explorer/Editor -- `main.cpp` no necesitó tocarse, su `DockBuilderDockWindow("Toolbox###toolbox", ...)` sigue calzando porque `###` sólo hashea lo que sigue del sufijo, mismo motivo por el que `"Explorer###explorer"`/`"Code Editor###code_editor"` tampoco se tocaron ahí en i18n-2/i18n-3), el hint "Arrastrá un control al lienzo de Design" (`toolbox.hint`) y el tag "(container)" que marca cada fila de tipo contenedor (`toolbox.container_tag`). Deliberadamente sin migrar: `info.display_name`/`info.category` de cada fila del catálogo -- vienen de `data/design/component_catalog.csv`, son datos del catálogo de componentes, no literales de chrome en el código (mismo criterio que `keyword_docs.csv`/`builtin_signatures.csv` en Editor). 3 keys nuevas en `en.csv`/`es.csv` bajo `panel.toolbox.title`/`toolbox.*` -- verificadas con el parser CSV real (80 filas totales, 0 rotas) y balance de llaves/paréntesis del archivo confirmado (8/8, 38/38) |
| **i18n-6** | ✅ Hecha | `properties_panel.cpp` migrado a `Tr()`: título del panel (`Properties###properties` → `Tr("panel.properties.title") + "###properties"`, mismo criterio de ID estable de siempre), mensaje de selección vacía (`properties.empty_selection`), labels Type/Id (`properties.type_label`/`properties.id_label`), su versión de solo-lectura con `TrFormat` (`properties.type_display`/`properties.id_display`), la nota "Read-only..." (`properties.readonly_note`), encabezados de las 3 tablas -- la editable reutiliza `properties.column_key`/`properties.column_value`, la de solo-lectura de Properties usa `properties.column_property` (nombre de columna distinto al de la tabla editable en el código original -- se preservó esa asimetría, solo se tradujo cada uno) y la de Events usa `properties.column_event`/`properties.column_handler` -- y el hint "nueva key" del campo para agregar una property/event nueva (`properties.new_key_hint`). El header de sección "Properties" reutiliza directamente `panel.properties.title` (mismo texto en ambos idiomas que el título del panel, evita duplicar la key); "Events" es `properties.section_events`. Se agregó el mismo helper local `TrFormat(key, arg)` de fases anteriores. Deliberadamente sin migrar: los glifos "x"/"+"/"--" (botón de quitar fila, botón de agregar fila, placeholder de la columna Value en la fila "agregar") -- son símbolos universales, no texto en idioma humano, mismo criterio que los separadores de sintaxis de Editor. `builtin_panels.h` actualizado: la entrada de Properties en `kBuiltinPanelNames` pasa a `tr_key = "panel.properties.title"` (antes `fallback_label = "Properties"`) para que el menú View de la title bar también muestre el nombre traducido -- `titlebar_panel.cpp` ya leía `tr_key`/`fallback_label` desde i18n-2, no necesitó tocarse. 14 keys nuevas en `en.csv`/`es.csv` bajo `panel.properties.title`/`properties.*` -- verificadas con el parser CSV real (94 filas totales, 0 rotas) y balance de llaves/paréntesis del archivo confirmado (38/38, 178/178) |
| **i18n-7** | ✅ Hecha | `terminal_panel.cpp`/`logs_panel.cpp` migrados a `Tr()` (Problems todavía no existe -- es Fase 2, sigue pendiente -- así que esta fase cubrió Terminal + Logs, la mitad del "panel inferior agrupado" que ya existe hoy). Títulos de ambos paneles con el patrón `###id` de siempre (`panel.terminal.title`/`panel.logs.title`); labels de sección ("Execution console"/"General logs"); botones Copy console/Copy all/Clear -- el ancho de cada botón se mide con `ImGui::CalcTextSize` sobre el string YA traducido, guardado en una variable local, para que el pinneado del botón al borde derecho del panel siga funcionando en cualquier idioma; el placeholder de scrollback vacío de cada panel; el menú contextual Copy/Copy All/Select All -- Ctrl+C/Ctrl+A quedan igual que siempre (atajos); y el hint del input de consola de Terminal. Se preservó a propósito la asimetría de texto que ya tenía el código entre el botón "Copy all" (minúscula) de Logs y el ítem de menú "Copy All" del mismo panel y de Terminal -- son keys distintas (`logs.copy_all_button` vs `common.copy_all`), no una inconsistencia a corregir. Nuevas keys `common.copy`/`common.copy_all`/`common.select_all`/`common.clear` reutilizadas entre Terminal y Logs (mismo texto exacto en ambos paneles). `builtin_panels.h` actualizado: Terminal y Logs pasan de `fallback_label` a `tr_key` para que el menú View también los muestre traducidos. Zona gris resuelta explícitamente (venía diferida desde i18n-4): los mensajes que `engine.AppendConsoleLine(...)`/`run.log` escriben en el propio texto de la consola de Terminal (marcador `"Run <path>"`, `"OK"`, `"could not launch..."`, `"script exited with an error..."`, `"the script crashed..."`, en `StartScriptRun`/`PollScriptRun`) quedan **deliberadamente sin migrar por ahora** -- no porque sean mensajes del compilador/VM (no lo son, son de AvaStudio, entrarían en la categoría "chrome" de §6.1), sino porque el marcador `"Run <path>"` tiene una copia documentada y byte-a-byte idéntica en `engine/engine_bridge.cpp::RunScript()` (ver comentario ahí y en `StartScriptRun`) que tiene que mantenerse en sync con esta; migrar solo el lado del panel rompería ese invariante la próxima vez que alguien edite uno sin el otro. Traducir esta familia de mensajes requiere tocar `engine_bridge.cpp` directamente, que no es un panel -- queda como pendiente explícito para una fase futura en vez de mezclarse con la i18n-N de Terminal. Los dos `log_bridge->Log(...)` de `designer_canvas.cpp` que i18n-4 había diferido justamente para esta fase también se resuelven acá: quedan **permanentemente sin migrar** -- son diagnóstico interno con identificadores de código embebidos (`uid_to_rect`, `BuildLiveRender`) que no tienen traducción natural y van a un panel que el propio código describe como para "Plugin and host events" (`logs.empty`), no chrome general de usuario. 14 keys nuevas en `en.csv`/`es.csv` -- verificadas con el parser CSV real (107 filas totales, 0 rotas) y balance de llaves/paréntesis de ambos archivos confirmado (`terminal_panel.cpp` 56/56, 233/233; `logs_panel.cpp` 28/28, 122/122) |
| **i18n-8** | ✅ Hecha | `build_panel.cpp` migrado a `Tr()`/`TrFormat()`: título del panel (`Build###build` → `Tr("panel.build.title") + "###build"`, mismo criterio de ID estable de siempre), texto intro, headers de sección `Project`/`Options`/`Advanced` (`build.section_project`/`build.section_options`/`build.section_advanced`), labels y hints de las 7 filas de path (Project folder, Entry script, Output folder, AES key file, AvaLang repo root, ava_cli path, VCPKG_ROOT) -- con `common.browse` ("Browse...") como key compartida entre todas las filas (antes literal repetido 7 veces) y `build.default_path_hint` ("(default: %s)") reutilizado vía `TrFormat` entre Project folder/Output folder/VCPKG_ROOT en vez de duplicar el mismo formato tres veces, los checkboxes (Obfuscate, Obfuscate strings, Flatten control flow, Zero-disk, Debug build) y sus 2 tooltips multi-línea (`build.obfuscate_tooltip`/`build.zero_disk_tooltip`, usando el escape `\n` que `UnescapeCell` ya soportaba pero que hasta ahora ningún archivo de locale había necesitado), los botones (Detect, Auto-detect×2, Install/Installing vcpkg, Build Executable/Building...), los 4 mensajes de validación pre-vuelo que se muestran antes de lanzar cualquier proceso (ava_cli no encontrado, repo root no encontrado, carpeta de proyecto inexistente, entry file faltante) y los `ImGui::TextColored` de estado dibujados como widgets propios del panel (vcpkg ready/failed, build succeeded/failed, "this can take a while...", "cloning + bootstrapping..."). Se agregó a este archivo el mismo helper local `TrFormat(key, {args...})` de Explorer/Editor/Canvas. Zona gris resuelta explícitamente, mismo criterio que el de Terminal en i18n-7: las dos líneas que arma `log_bridge.Log(...)` justo antes de anunciar lo que ya se reenvió en vivo al Output/Logs vía `FlushLogToOutput` (prefijos `[build]`/`[vcpkg]`) quedan **deliberadamente sin migrar** -- no son un widget de UI independiente, son parte del mismo stream de log crudo que ese "pendiente explícito" ya cubre. `builtin_panels.h` actualizado: Build pasa de `fallback_label` a `tr_key = "panel.build.title"` para que el menú View también lo muestre traducido. ~44 keys nuevas en `en.csv`/`es.csv` bajo `panel.build.title`/`build.*`, con reutilización de `common.browse` y `build.default_path_hint` -- verificadas con el parser CSV real (150 filas totales cada uno, 0 rotas) y balance de llaves/paréntesis del archivo confirmado (101/101, 576/576) |
| **i18n-9** | ✅ Hecha | `settings_panel.cpp` migrado a `Tr()`: título del panel (`Settings###settings` → `Tr("panel.settings.title") + "###settings"`, mismo criterio de ID estable de siempre), headers de sección del sidebar "General"/"Plugins" (`settings.section_general`/reutiliza `plugins.title` para "Plugins" en vez de duplicarlo -- mismo texto que ya usa el modal de Plugins de la title bar), el ítem fijo "Editor" del sidebar (`settings.editor_item`), el placeholder "(none)" cuando no hay plugins con panel de settings (`settings.no_plugins`), el label "Language" (`settings.language_label`), el label/descripción/hint de "Modules folder" (`settings.modules_folder_label`/`settings.modules_folder_description`/`settings.modules_folder_hint`) y sus botones Browse/Save -- reutilizando `common.browse` (de i18n-8) y `menu.file.save` (de i18n-1) en vez de duplicarlos. `DrawSidebarGroupHeader` cambió su parámetro de `const char*` a `const std::string&` para poder recibir directamente el resultado de `Tr()`. Deliberadamente sin migrar: las dos etiquetas del combo de idioma, "English"/"Español" -- son los endónimos de cada idioma (el nombre que cada idioma usa para sí mismo), no chrome traducible: tienen que verse igual sin importar cuál sea el locale activo, misma convención que cualquier selector de idioma de un SO/app (una persona que lee en español igual tiene que ver "English" en inglés en esa lista, no una traducción de la palabra). Ningún `Begin()`/patrón `###` nuevo que resolver aparte del título del panel -- el split sidebar/contenido es un `BeginChild` interno, no una ventana propia. 9 keys nuevas en `en.csv`/`es.csv` bajo `panel.settings.title`/`settings.*`, con reutilización de `common.browse`, `menu.file.save` y `plugins.title` -- verificadas con el parser CSV real (158 filas totales cada uno, 0 rotas) y balance de llaves/paréntesis del archivo confirmado (27/27, 132/132). `builtin_panels.h` actualizado: Settings pasa de `fallback_label` a `tr_key = "panel.settings.title"`. |
| **i18n-10..N** | ✅ Hecha (con una excepción heredada) | Con esto se completa el bloque i18n de §6.5 para los paneles que existen hoy (Explorer, Editor, Canvas, Toolbox, Properties, Terminal, Logs, Build, Settings, más title bar/reorg de i18n-0/1). El único pendiente que quedaba (Problems) se resolvió solo al construirse en la Fase 2: `problems_panel.cpp` se escribió desde el día uno con `util::Tr()`/patrón `###id`/badge de conteo vía `TrFormat`, sin literales hardcodeados que migrar después -- no necesitó su propia fase de i18n-N como los paneles preexistentes. Sigue pendiente, heredado de i18n-7 y sin relación con Problems: la familia de mensajes de consola de Terminal (`StartScriptRun`/`PollScriptRun`) diferida por su invariante de sincronización byte-a-byte con `engine_bridge.cpp::RunScript()`. |
| **0** | ✅ Hecha | `build_panel.cpp`: nueva sección "Target" arriba de "Project" con (a) campo de solo lectura `build.platform_label` calculado con `DetectedPlatformName()` (`_WIN32`/`__APPLE__`/`__linux__`, mismo criterio que `build_command.cpp`, §3.4.1 -- sin combo, placeholder para cuando avapack soporte cross-compiling) y (b) combo `build.target_label` Desktop/BareKernel (§3.4.2) atado a `settings.build_target` (nuevo campo persistido, `""`/`"desktop"` equivalentes). Al elegir BareKernel aparece el campo `build_toolchain_dir` (nuevo, persistido, con Browse) y se oculta toda la sección "Options" (obfuscate/zero-disk/debug/key-file -- ignorados por `ava_cli build --target barekernel` según su propio `--help`) mostrando en su lugar una nota (`build.barekernel_note`). El botón Build ahora valida `build_toolchain_dir` cuando el target es BareKernel (`build.error_toolchain_dir_missing`) y arma los args reales: siempre `--target desktop\|barekernel`, más `--toolchain-dir` solo para BareKernel; el naming del .exe esperado en el mensaje de éxito replica la regla de sufijo de `build_command.cpp` (BareKernel siempre `.exe`, no `AVASTUDIO_EXE_SUFFIX`). `settings.h`/`.cpp`: campos `build_target`/`build_toolchain_dir` agregados a `StudioSettings` y a Load/SaveSettings. `build_panel.h`: `BuildBrowseField::kToolchainDir` nuevo; `main.cpp`: caso agregado al switch de diálogos nativos (folder picker, igual que repo-root/vcpkg-root). 11 keys nuevas en `en.csv`/`es.csv` bajo `build.section_target`/`build.platform_*`/`build.target_*`/`build.toolchain_dir_*`/`build.barekernel_note`/`build.error_toolchain_dir_missing` -- verificadas con el parser CSV real (168 filas totales cada uno, 0 rotas, keys idénticas entre ambos) y balance de llaves/paréntesis de los 4 archivos tocados confirmado |
| **0.5** | ✅ Hecha (AvaStudio) | Sacados todos los comentarios (`//` y `/* */`, cabecera de archivo incluida) de los 85 `.cpp`/`.h` de `runtime/avastudio/src/` -- 47.0k → 39.7k líneas, 3591 comentarios removidos, `{`/`}` balanceados verificados en cada archivo antes y después. Herramienta: tokenizer propio (no regex) que respeta strings/chars/raw-strings `R"delim(...)"` para no confundir un `//`/`/*` dentro de un literal (p. ej. el regex de `syntax_avalang.h`) con un comentario real; corre por archivo, escribe el resultado y no toca nada fuera de comentarios (diffs verificados a mano en varios archivos representativos). De los 3591, los 498 que superaban el umbral heurístico de "no trivial" (≥18 palabras o contienen marcadores como "porque"/"ver §"/"deliberadamente"/"TODO"/"gotcha") se archivaron verbatim, agrupados por archivo con número de línea original, en `runtime/avastudio/COMMENT_ARCHIVE.md` (nuevo) -- pendiente de una pasada humana para podar lo redundante con este mismo plan y fusionar notas repetidas entre archivos en una sola, como pide §7.1. Deliberadamente fuera de esta pasada: `runtime/avalang`, `runtime/avaui`, `runtime/avahost`, `runtime/avapack`, `runtime/avacli` (~85k líneas combinadas, bastante más grandes que AvaStudio y sin tocar todavía en este plan) -- §7.1 dice "AvaStudio y proyectos de AvaLang que apliquen"; se prioriza AvaStudio primero por ser el codebase que este plan sí gobierna línea a línea, y se deja el resto como continuación explícita de esta misma fase en vez de mezclarlo en un solo diff gigante |
| **1** | ✅ Hecha | `DetectEntryFile()` movida de `build_panel.cpp` a `util/project_utils.h/.cpp` (nuevo, agregado a `STUDIO_SOURCES` en `CMakeLists.txt`) -- código sin cambios, solo relocalizado; `build_panel.cpp` quedó sin la función local (y sin los includes `<algorithm>`/`<functional>` que ya no usaba) y sigue llamándola sin calificar por estar ambos en `namespace studio`. `main.cpp`: nuevo `Run Project` (`Shift+F5`) -- resuelve `project_dir`/`entry` con la misma lógica que ya usa el Build panel (`settings.build_project_dir`/`settings.build_entry_file`, con fallback a `DetectEntryFile()`), guarda todos los tabs sucios (`SaveAllTabs`) y reutiliza el mismo mecanismo async que ya tiene `Run Script` (`StartScriptRun`/`PollScriptRun`/click-to-jump del Terminal panel), así que no hizo falta ninguna lógica nueva de highlight de errores. De paso corregido un bug: `want_run` (F5 suelto) disparaba también con Shift+F5 por no excluir `io.KeyShift`, lo que habría hecho correr el tab activo Y el proyecto a la vez. Nuevo campo `run_project_requested` en `EditorState` (mismo patrón que `run_requested`, con su reseteo correspondiente al final del frame -- sin eso el ítem de menú se habría quedado disparando Run Project en cada frame). Entrada `Run Project` (`Shift+F5`) agregada al menú Run de `titlebar_panel.cpp`, debajo de `Run Script`. 2 keys nuevas (`menu.run.run_project`) en `en.csv`/`es.csv` -- verificadas con el parser CSV real (169 filas cada uno, 0 rotas, 0 duplicadas, keys idénticas entre ambos) y balance de llaves/paréntesis de los 6 archivos tocados confirmado |
| **2** | ✅ Hecha | `EngineBridge::CheckScript(source, source_name)` (nuevo, `engine_bridge.h/.cpp`): parsea + type-checkea sin invocar `ava_run` ni tocar `console_` -- reusa exactamente la misma forma de diagnóstico (`RunResult`: `error_line`/`error_column`/`error_source`) que ya devuelve `RunScript` en su rama de fallo, así que Check y Run pueden alimentar el mismo sink sin adaptar nada. `panels/problems_panel.h/.cpp` (nuevo): `ProblemsState` (lista de `ProblemEntry` + selección para copiar) y `UpdateProblemsFromResult(state, source_label, result, fallback_file)` -- función *sink* única: primero borra cualquier entrada previa con ese mismo `source_label` (así "run" y "check" nunca se pisan ni se duplican entre sí) y solo agrega una entrada nueva si `result.success` es falso; `DrawProblemsPanel` reutiliza el mismo patrón de selección/copy/copy-all que ya tenía Terminal (`CopySelection`/`CopyAll`, Ctrl+C/Ctrl+A, menú contextual) y devuelve un `ProblemsFileClickRequest` al hacer click en un ítem, para que quien la use dispare el mismo `HighlightError`+`OpenFileInTab` que ya usa Terminal -- no un mecanismo nuevo. Título con badge de conteo (`panel.problems.title_with_count`, ej. "Problems (3)") vía el mismo patrón `###id` de siempre. Cableado en `main.cpp`: (a) helper compartido `resolve_project_entry()` -- extraído del bloque de `Run Project` de la Fase 1, que antes resolvía `project_dir`/`entry` inline -- ahora lo llaman tanto `Run Project` como `Check`, así ninguno de los dos puede divergir sobre cuál es el entry point del proyecto; (b) nuevo bloque `Check` (`Ctrl+Shift+B`): guarda todos los tabs (`SaveAllTabs`), lee el entry file del disco (no del tab, porque el entry point del proyecto puede no estar abierto en ningún tab), llama a `CheckScript` y pasa el resultado a `UpdateProblemsFromResult` con `source_label = "check"`; si falla, salta al archivo (`OpenFileInTab` si no es ya el tab activo) y llama `HighlightError` -- mismo mecanismo que ya usa Run, formalizado para poder dispararse sin ejecutar (§3.3); (c) `perform_run` ahora también llama `UpdateProblemsFromResult` con `source_label = "run"` justo después de `RunScript`, para que los errores de correr el tab activo dejen de estar aislados en Terminal y aparezcan también en Problems; (d) el panel se dibuja y dockea junto a Terminal/Logs (`dock_bottom`), con el mismo patrón de click-to-jump que ya tenía Terminal. Bug corregido de paso: `want_build` (`Ctrl+B`) no excluía Shift, así que `Ctrl+Shift+B` iba a disparar Build *y* Check a la vez -- mismo tipo de bug que `want_run`/Shift+F5 en Fase 1, mismo fix (`!io.KeyShift`). Nuevo campo `check_requested` en `EditorState` (mismo patrón que `run_project_requested`, con su reseteo al final del frame) y entrada "Check" (`Ctrl+Shift+B`) en el menú Run de `titlebar_panel.cpp`, debajo de "Run Project" -- el popup de ese menú tenía ancho fijo (160px, le alcanzaba a "Shift+F5" pero no a "Ctrl+Shift+B"), así que pasó a `SetNextWindowSizeConstraints`, mismo patrón que ya usaba el menú File para su propio shortcut más largo ("Ctrl+Shift+S"). `Problems###problems` agregado a `kBuiltinPanelNames` (para que el menú View lo liste) y a `CMakeLists.txt` (`problems_panel.cpp`). 4 keys nuevas en `en.csv`/`es.csv` (`panel.problems.title`, `panel.problems.title_with_count`, `problems.section_label`, `problems.empty` -- `menu.run.check` ya existía) -- verificadas con el parser CSV real (174 filas cada uno, 0 rotas, 0 duplicadas, keys idénticas entre ambos) y balance de llaves/paréntesis de los 7 archivos tocados confirmado. Deliberadamente fuera de esta fase: el panel "Output" nuevo y el agrupamiento visual de Terminal/Output/Logs/Problems como pestañas de una sola ventana (§5.6) -- es una pieza de UI más grande y separada del hueco de paridad "Problems + Check" que era el objetivo principal de esta fase; el badge de conteo (parte de §5.6) sí quedó resuelto porque ya viene gratis del propio título del panel |
| **2.5** | ✅ Hecha | Find in file / Replace resultaron gratis: `TextEditor` (`ImGuiColorTextEdit`, tag `Legacy`, ya fetcheada en `CMakeLists.txt`) trae su propia ventana de Find/Replace/Replace All cableada en `handleKeyboardInputs()` (`Ctrl+F` la abre, condicionado a que el editor tenga foco) -- no hizo falta escribir nada para esa mitad de la fase. Find in Project sí es código nuevo: `panels/find_in_project_panel.h/.cpp` (nuevo) -- `FindInProjectState` (query, toggle case-sensitive, resultados) y `RunFindInProject(state, project_root)`: recorre recursivamente `.ava`/`.avaui` bajo `project_root` (orden estable, mismo criterio de "ordenar antes de iterar" que ya usa `DetectEntryFile` en `project_utils.cpp`), busca por substring plano (sin regex, alcance igual al de §5.2) línea por línea, tope de 500 matches (`kMaxMatches`) marcando `truncated` en vez de colgar el panel con una query muy amplia; `DrawFindInProjectPanel` dibuja el campo de búsqueda + checkbox case-sensitive + resultados agrupados por archivo (encabezado de archivo intercalado, mismo patrón visual que usa Explorer para directorios) y devuelve un `FindInProjectClickRequest` al clickear un resultado, para que el caller haga `OpenFileInTab` + selección -- mismo patrón de dos pasos que ya usan Terminal/Problems. Nueva `SelectMatchInEditor()` en `editor_panel.h/.cpp` en vez de reusar `HighlightError`: esa última pinta el marcador rojo de "error" y la borra cualquier Run/Check siguiente, lo cual sería semánticamente incorrecto para un resultado de búsqueda que no es un error -- `SelectMatchInEditor` solo hace `SelectRegion` + `ScrollToLine`. Bug de colisión encontrado y corregido de paso, mismo tipo que `want_run`/Shift+F5 (Fase 1) y `want_build`/Ctrl+Shift+B (Fase 2) pero en esta ocasión no resoluble con `!io.KeyShift` porque el atajo en conflicto vive *dentro* de un widget de terceros: `ImGuiColorTextEdit` ya usa `Ctrl+Shift+F` internamente para "seleccionar todas las ocurrencias en el archivo actual" (`isShiftShortcut && F -> findAll()` en su `handleKeyboardInputs()`), activo solo cuando el editor tiene foco (`ImGui::IsWindowFocused()`); sin gateo, ambos atajos dispararían juntos. Solución: nuevo campo `EditorState::code_editor_has_focus`, seteado en `DrawEditorPanel` justo después de `tab.editor.Render(...)` vía `ImGui::IsItemFocused()` (el child window que la librería acaba de dibujar sigue siendo el "item" anterior en la ventana contenedora, mismo patrón de ImGui que permite `IsItemHovered()` después de `EndChild()`), y el atajo global de Find in Project (`Ctrl+Shift+F` en `main.cpp`) solo se arma cuando ese flag es falso -- con el editor enfocado, `Ctrl+Shift+F` sigue yendo a "seleccionar ocurrencias" de la librería sin interferencia; el menú y el atajo global funcionan en cualquier otro caso. Nuevo campo `find_in_project_requested` en `EditorState` (mismo patrón que `check_requested`, con su reseteo al final del frame) y menú **Edit** nuevo en `titlebar_panel.h/.cpp` (no existía; hoy hay File/View/Run) con el ítem "Find in Project" (`Ctrl+Shift+F`) -- sumó `edit_menu_rect` a `TitleBarResult`, al array de hit-regions de `main.cpp` (5 -> 6, para que el click no arrastre la ventana, mismo mecanismo que ya protege File/View/Run) y a `any_popup_open`. `Find in Project###find_in_project` agregado a `kBuiltinPanelNames` (9 entradas, para que el menú View lo liste) y dockeado en `dock_bottom` junto a Terminal/Logs/Problems; agregado a `CMakeLists.txt` (`find_in_project_panel.cpp`). Al disparar el atajo/ítem de menú se reabre el panel si estaba cerrado, se le da foco (mismo patrón que `want_build` con el panel Build) y se pide foco de teclado en el campo de búsqueda (`focus_query_field`). 10 keys nuevas en `en.csv`/`es.csv` (`menu.edit`, `menu.edit.find_in_project`, `panel.find_in_project.title`, `find_in_project.query_label`/`.case_sensitive`/`.search_button`/`.empty_hint`/`.no_results`/`.results_count`/`.results_truncated`) -- verificadas con el parser CSV real (184 filas cada uno, 0 rotas, 0 duplicadas, keys idénticas entre ambos) y balance de llaves de los 8 archivos tocados confirmado (paréntesis también, salvo un desbalance de 1 preexistente en `editor_panel.cpp` -- confirmado contra el zip original sin tocar, no introducido en esta sesión; el propio conteo naive de parientesis cuenta notación de intervalo semiabierto `[a, b)` en comentarios como si fuera código, que es la explicación de los nuevos "desbalances" de esta pasada). Deliberadamente fuera de esta fase: whole-word matching y búsqueda con regex (no pedidos por §5.2, que solo pide texto plano agrupado por archivo) y "Replace in Project" (natural siguiente paso, pero no mencionado en el alcance de 2.5) |
| **3** | ✅ Hecha | `panels/command_palette.h/.cpp` (nuevo): `Command{id, label, shortcut, action}` -- `label` ya viene armado por el caller como "Categoría: Acción" (mismo criterio que VSCode), el panel solo filtra/dibuja, no sabe de i18n; `CommandPaletteState{query, selected_index, focus_query_field}` sin persistir en disco, mismo criterio que `FindInProjectState`. `OpenCommandPalette()` llama `ImGui::OpenPopup` una sola vez por atajo/click (mismo idiom que el modal de "Unsaved Changes"), no cada frame. `DrawCommandPalette()` se llama sí cada frame (requisito de `BeginPopupModal`), filtra por substring plano case-insensitive (mismo alcance que Find in Project, sin fuzzy scoring), navegación ↑/↓, Enter ejecuta y cierra, Escape cierra sin ejecutar; `CloseCurrentPopup()` se llama *antes* de `EndPopup()` (mientras el popup sigue siendo el tope del stack) pero la action en sí se ejecuta después de `EndPopup()`, por si algún comando futuro necesita abrir otro popup/modal en el mismo frame. Registro de ~24 comandos construido en `main.cpp` cada frame por referencia a todo el estado ya existente (`run_requested`, `run_project_requested`, `check_requested`, `find_in_project_requested`, `save_requested`, `close_tab_requested`, `new_tab_requested`, `open_requested`, `open_folder_requested`, `g_native_close_requested` para Exit) más 3 flags nuevos en `EditorState` (`save_as_requested`, `open_settings_panel_requested`, `open_build_panel_requested`) para los casos que antes solo disparaba la title bar -- ORed en los mismos bloques de manejo que ya miraban `titlebar_result.*`/`want_*`, y reseteados al final del frame junto con el resto de ese grupo. Las categorías del comando ("File:", "Run:", "View:", etc.) reusan directamente las keys de i18n ya existentes de los menús (`menu.file`, `menu.edit`, `menu.run`, `menu.view`, `menu.file.preferences`, `panel.build.title`) en vez de duplicar keys nuevas -- solo 3 keys nuevas hicieron falta (`menu.view.command_palette`, `command_palette.hint`, `command_palette.no_results`). Los 9 comandos de "View: <panel>" iteran `kBuiltinPanelNames` y llaman una lambda nueva `toggle_panel_visibility` en `main.cpp`, extraída del bloque que antes manejaba `titlebar_result.panel_toggle_requested` inline (mismo motivo de extracción que `resolve_project_entry` en Fase 2: que el toggle nunca pueda divergir entre el menú View y la paleta). Mismo patrón para `open_panel_focused` (Settings/Build), que reemplaza los 3 bloques casi idénticos que antes abrían esos paneles desde `titlebar_result.open_settings_requested`/`build_requested`/`want_build`. `build_panel.h/.cpp`: `ResolveVcpkgInstallTarget(settings)` extraída del cómputo inline que hacía el botón "Install vcpkg" (ahora el botón también la llama, en vez de duplicar la cadena de fallbacks) y `StartVcpkgInstall` movida fuera del namespace anónimo del .cpp (antes tenía linkage interno) -- ambas declaradas en `build_panel.h` y usadas directamente por el comando "Build: Install vcpkg" de la paleta sin duplicar lógica. `titlebar_panel.h/.cpp`: nuevo `command_palette_requested` en `TitleBarResult`, ítem "Command Palette..." (`Ctrl+Shift+P`) agregado arriba de la lista de paneles en el menú View (con separador); el popup de ese menú tenía `SetNextWindowSize` fijo (200px) que el shortcut más largo hubiera recortado -- mismo tipo de bug que Ctrl+Shift+B/Ctrl+Shift+S en fases anteriores, mismo fix (`SetNextWindowSizeConstraints`). Atajo global `Ctrl+Shift+P` en `main.cpp` sin necesidad de gateo por foco (a diferencia de Ctrl+Shift+F en Fase 2.5) -- se revisaron los patches vendored de `ImGuiColorTextEdit` bajo `avastudio/patches/` y no hay colisión conocida con la tecla P. Comandos deliberadamente fuera de esta fase, mismo criterio que el diseño original: "Extensions" y "About" (modales `static bool` locales a `titlebar_panel.cpp`, no expuestos como entry point) y cualquier variante de "Replace in Project"/whole-word/regex (fuera del alcance de §5.2, ver notas de Fase 2.5). `CMakeLists.txt`: `src/panels/command_palette.cpp` agregado a `STUDIO_SOURCES`. 3 keys nuevas en `en.csv`/`es.csv` -- verificadas con el parser CSV real (187 filas cada uno, 0 rotas, 0 duplicadas, keys idénticas entre ambos) y balance de llaves de los 8 archivos tocados confirmado (paréntesis también, salvo el mismo desbalance de 1 preexistente en `editor_panel.cpp`/`.h` ya documentado en Fase 2.5 -- notación de intervalo `[a, b)` en un comentario, no código real, confirmado sin cambios contra el zip de la sesión anterior) |
| **4** | ✅ Hecha | `util/project_utils.h/.cpp`: extraída `ListSearchableFiles(project_dir)` -- antes `CollectSearchableFiles` era privada de `find_in_project_panel.cpp`, movida acá (mismo criterio de extracción que `DetectEntryFile` en Fase 1) para que Find in Project y Quick Open compartan el mismo recorrido de archivos en vez de reimplementarlo; `find_in_project_panel.cpp` actualizado para llamarla en vez de su copia local. `panels/quick_open_panel.h/.cpp` (nuevo): `QuickOpenState` (query, selección, lista cacheada de `{display, full_path}`) + `OpenQuickOpen(state, project_root)` -- re-escanea el proyecto y abre el popup, mismo idiom que `OpenCommandPalette` -- + `DrawQuickOpen(state)`, popup modal dibujado cada frame con el mismo layout/estilo que la Command Palette (mismo ancho 560px, mismo posicionamiento centrado arriba), filtro por substring case-insensitive sobre la ruta relativa mostrada (sin fuzzy scoring, mismo alcance que Find in Project/Command Palette), navegación ↑/↓, Enter/click para elegir, Escape para cancelar -- devuelve la ruta absoluta (`full_path`) del archivo elegido, no la relativa: se decidió así a propósito porque `OpenFileInTab` abre el archivo con `std::ifstream(path)` directo, que necesita una ruta que resuelva sin conocer `project_root` (mismo criterio que ya sigue Explorer con su propio `file_to_open`) -- evita heredar la dependencia implícita de CWD==project_root que tiene hoy el click-to-open de Find in Project. Cableado en `main.cpp`: nuevo `QuickOpenState quick_open_state`; atajo global `Ctrl+P` (`want_quick_open`, gateado con `!io.KeyShift` para no disparar junto con `Ctrl+Shift+P` de Command Palette -- mismo tipo de colisión que las de Fases 1-3, mismo tipo de fix); `OpenQuickOpen` se dispara desde `want_quick_open`, el nuevo `titlebar_result.quick_open_requested` o el nuevo `editor_state.quick_open_requested`; `DrawQuickOpen` se llama sin condición cada frame, justo después de `DrawCommandPalette`, y un pick dispara `OpenFileInTab` directo (no hace falta `HighlightError` ni el patrón de dos pasos de Terminal/Problems, porque no hay error que resaltar). `editor_panel.h`: nuevo flag `quick_open_requested` en `EditorState` (mismo patrón que `find_in_project_requested`, reseteado al final del frame junto con el resto de ese grupo). `titlebar_panel.h/.cpp`: nuevo `quick_open_requested` en `TitleBarResult` + ítem "Quick Open..." (`Ctrl+P`) agregado al menú Edit, arriba de "Find in Project" -- no necesitó ajustar el ancho del popup del menú (`SetNextWindowSizeConstraints` ya en 160px), "Ctrl+P" es más corto que "Ctrl+Shift+F". Nueva entrada "Edit: Quick Open" (`Ctrl+P`) en el registro de la Command Palette de Fase 3, junto a "Edit: Find in Project". Deliberadamente sin builtin panel/docking: Quick Open es un overlay/modal, no una ventana dockeable -- mismo criterio ya aplicado a Command Palette, no entra a `kBuiltinPanelNames`. 4 keys nuevas en `en.csv`/`es.csv` (`menu.edit.quick_open`, `quick_open.hint`, `quick_open.no_results`, `quick_open.no_project_files`) -- verificadas con el parser CSV real (191 filas cada uno, 0 rotas, 0 duplicadas, keys idénticas entre ambos) y balance de llaves/paréntesis de los 9 archivos tocados confirmado (paréntesis también, salvo el mismo desbalance de 1 preexistente en `editor_panel.cpp`/`.h` ya documentado en Fases 2.5/3 -- notación de intervalo `[a, b)` en un comentario, no código real, sin cambios en esta sesión). `CMakeLists.txt`: `src/panels/quick_open_panel.cpp` agregado a `STUDIO_SOURCES` |
| **5** | ✅ Hecha | `data/scaffold/file_templates.csv` (nuevo, mismo patrón de CSV centralizado que `component_catalog.cpp` ya usaba para el catálogo del Toolbox -- ver §7.2): dos filas (`class`/`.ava`, `screen`/`.avaui`) con el boilerplate completo como contenido, dos placeholders `{ClassName}`/`{DisplayName}`. `util/scaffold_templates.h/.cpp` (nuevo): `ScaffoldKind{kClass, kScreen}`, `ScaffoldExtension(kind)` y `BuildScaffoldContent(kind, base_name)` -- carga el CSV una sola vez (cacheado en `static`, mismo criterio que `LoadCatalogMetadata`) y sustituye los dos placeholders por separado: `{ClassName}` pasa por `SanitizeIdentifier` (charset `[A-Za-z0-9_]`, prefijo `_` si empieza con dígito, fallback `"NewClass"` si queda vacío) porque termina siendo un identificador de AvaLang real -- el constructor con el mismo nombre que la clase, convención ya fijada en `tools/vscode/examples/example.ava`; `{DisplayName}` pasa por `EscapeAvauiString` (solo escapa `"`/`\`) porque termina siendo un string literal de `.avaui` (`title`/`text`), no código. Verificado con un programa aparte que enlaza contra el `.cpp` real (`csv.cpp`/`data_dir.cpp`) que la sustitución produce exactamente el archivo esperado para varios casos límite (nombre válido, nombre que empieza con dígito, nombre de puros símbolos, nombre con comillas) -- ver notas de esta sesión. `explorer_panel.cpp`: el popup de `DrawCreatePopup` (hasta ahora escribía un archivo vacío) ahora recibe `ExplorerResult&`, agrega dos `RadioButton` (Class/Screen, mismo criterio de "elección binaria siempre visible" que los checkboxes de Build) visibles solo cuando `!is_folder`, y al crear: (a) fuerza la extensión del archivo a la que corresponde al kind elegido vía `target.replace_extension(...)` -- ignora deliberadamente cualquier extensión que haya tecleado el usuario, para que el archivo en disco y su boilerplate nunca puedan desincronizarse (un `.avaui` con una clase `.ava` adentro sería un bug silencioso); (b) escribe `BuildScaffoldContent(kind, target.stem().string())` en vez de un archivo vacío; (c) llama a `OpenFileInTab` a través del `ExplorerResult::file_to_open` ya existente (mismo campo que ya usa el doble-click del árbol) para que el archivo nuevo se abra solo, sin inventar un mecanismo nuevo -- `main.cpp` no necesitó tocarse, ya escuchaba ese campo desde el día uno. `CMakeLists.txt`: `src/util/scaffold_templates.cpp` agregado a `STUDIO_SOURCES`; comentario del `copy_directory` de `data/` actualizado para mencionar `scaffold/` (mismo criterio que la fase "reorg `data/`" siguió al agregar `langs/`) -- sin lógica nueva de CMake, la copia ya es recursiva. 4 keys nuevas en `en.csv`/`es.csv` (`explorer.new_file_kind_class`, `explorer.new_file_kind_screen`) -- verificadas con el parser CSV real (192 filas cada uno, 0 rotas, 0 duplicadas, keys idénticas entre ambos) y balance de llaves/paréntesis de los 3 archivos `.h`/`.cpp` tocados confirmado. Deliberadamente fuera de esta fase: la "Generación desde el catálogo" de §3.1 (arrastrar un componente al canvas ya genera su snippet vía `component_catalog.cpp`, pero formalizarla explícitamente como parte de "Generar" no se tocó -- sigue siendo comportamiento implícito del canvas, tal como estaba) y cualquier entrada de Command Palette/menú File para este flujo -- el wizard de creación necesita un directorio destino que Command Palette/File no tienen contexto para elegir sin ambigüedad (a diferencia de Quick Open/Find in Project, que no escriben nada a disco), así que por ahora sigue siendo exclusivo del menú contextual del Explorer (root y por carpeta), que sí lo tiene |
| **6** | ✅ Hecha | `panels/new_project_panel.h/.cpp` (nuevo): `NewProjectState{name, destination, template_kind, error_key, focus_name_field}` -- sin persistir en disco, mismo criterio que `CommandPaletteState`/`QuickOpenState`; `OpenNewProjectDialog(state, default_destination)` prefillea `destination` con el root actual del Explorer (mismo "prefill con lo que el usuario ya estaba mirando" que `OpenQuickOpen`/`OpenFindInProject` con `project_root`) y arma `focus_name_field`; `DrawNewProjectDialog(state)` -- popup modal dibujado cada frame (mismo requisito que Command Palette/Quick Open), mismo ancho/posicionamiento centrado-arriba que esos dos. Campo Nombre + campo Destino con botón Browse (dispara `browse_destination_requested`, un solo bool en vez del enum `BuildBrowseField` de Build porque acá solo hay un campo que puede pedir diálogo nativo) + elección Empty/With example screen (dos `RadioButton`, mismo criterio de "elección binaria siempre visible" que Explorer en Fase 5) + línea de preview con la carpeta resultante (`new_project.preview_label`, visibilidad de estado del sistema, criterio de §5) + mensaje de error bajo demanda (mismo patrón `TextColored(palette::kError, ...)` que ya usa Build). Validación al crear, en orden: nombre no vacío, destino no vacío, destino existe como directorio, `<destino>/<nombre>` no existe ya (nunca escribe encima de una carpeta que ya está ahí -- mismo criterio de "no fusionar silenciosamente" que ya tiene el modal de confirmación de borrado de Explorer, aplicado antes de crear en vez de antes de destruir). Al crear: `fs::create_directories`, escribe `main.ava` con un boilerplate fijo de script de nivel superior (`import system` + `print(...)`, mismo estilo que `samples/test/main.ava` -- deliberadamente *no* pasa por `data/scaffold/file_templates.csv` de Fase 5, ver nota en el propio `scaffold_templates.h` que ya reservaba ese CSV solo para "Generar" dentro de un proyecto existente) y, si el template es "With example screen", además `screen.avaui` reutilizando literalmente `util::BuildScaffoldContent(ScaffoldKind::kScreen, "Home")` de Fase 5 en vez de inventar una plantilla nueva. Devuelve `{project_dir, entry_file}` en el mismo formato nativo (`.string()`) que ya usa `ExplorerResult::file_to_open`, no el `generic_string()` que Quick Open usa por sus propios motivos. `titlebar_panel.h/.cpp`: nuevo `new_project_requested` en `TitleBarResult`, ítem "New Project..." agregado arriba de "New File" en el menú File (agrupa las dos acciones que involucran una carpeta -- New Project/Open Folder -- separadas de las acciones de archivo suelto). `editor_panel.h`: nuevo `new_project_requested` en `EditorState` (mismo patrón que `quick_open_requested`, para que la Command Palette lo dispare sin pasar por el menú). Cableado en `main.cpp`: nuevo `NewProjectState new_project_state`; trigger edge-triggered igual que Command Palette/Quick Open; `DrawNewProjectDialog` llamado sin condición cada frame justo después de `DrawQuickOpen`, resolviendo `browse_destination_requested` con el mismo `OpenFolderDialog` nativo que ya usa Build, y aplicando una creación exitosa a `explorer_state.root_dir`/`editor_state.project_root` + `OpenFileInTab(entry_file)` -- mismo patrón de dos pasos (el diálogo no toca Explorer/EditorState directamente) que ya usan los picks de Quick Open y los clicks de archivo de Terminal/Problems/Find in Project. Entrada "File: New Project" agregada al registro de la Command Palette (Fase 3), arriba de "File: New File". Bug encontrado y corregido de paso mientras se tocaba el bloque de `open_folder_requested`: "Open Folder..." actualizaba `explorer_state.root_dir` pero nunca `editor_state.project_root`, dejando a Quick Open/Find in Project buscando en el proyecto viejo después de cambiar de carpeta -- mismo tipo de fix de sincronización que `want_run`/`want_build` en Fases 1-2, aplicado acá porque el wizard nuevo depende de que ambos campos se mantengan sincronizados. `CMakeLists.txt`: `src/panels/new_project_panel.cpp` agregado a `STUDIO_SOURCES`. 14 keys nuevas en `en.csv`/`es.csv` bajo `menu.file.new_project`/`new_project.*`, con reutilización de `common.browse`/`common.cancel` ya existentes -- verificadas con el parser CSV real (207 filas cada uno, 0 rotas, 0 duplicadas, keys idénticas entre ambos) y balance de llaves/paréntesis de los 6 archivos `.h`/`.cpp` tocados confirmado (paréntesis también, salvo el mismo desbalance de 1 preexistente en `editor_panel.h` ya documentado desde Fase 2.5 -- notación de intervalo `[a, b)` en un comentario, no código real, sin cambios netos introducidos en esta sesión). Deliberadamente fuera de esta fase: cualquier noción de manifest de proyecto o de `ava_cli new` (no existe todavía, ver §3.1/§9), y elegir la plataforma/tipo de build desde el wizard (eso ya lo resuelve el Build panel en Fase 0, no hace falta duplicarlo acá) |
| **7** | ✅ Hecha | `panels/activity_bar_panel.h/.cpp` (nuevo): franja vertical fija de íconos a la izquierda del dockspace, mismo idiom de ventana top-level sin decoración que ya usa `DrawTitleBar` para su tira de caption buttons (no es parte del dockspace, no es dockeable). `ActivityBarResult{explorer_clicked, search_clicked, toolbox_clicked, extensions_clicked, settings_clicked}` -- cinco íconos dibujados a mano con `ImDrawList` (folder/lupa/grilla 2x2/rombo/engranaje), mismo criterio de "iconos como formas simples" que ya usan los caption buttons de la title bar y `DrawIcon()` de Explorer (sin font de íconos). `ActivityIconButton` (helper local, copia deliberada del idiom `CaptionButton` de `titlebar_panel.cpp` en vez de compartirlo -- mismo motivo que cada panel ya tiene su propio `TrFormat` local) dibuja una barra de acento (`kPrimary`) a la izquierda del ícono activo, más hover -- señal de visibilidad de estado (mismo criterio de §5.10) para saber qué panel está abierto sin tener que abrirlo. Explorer/Toolbox/Settings comparten dock slot como pestañas hoy (`dock_left`/`dock_right`), así que su click es un toggle abre/cierra real (reutiliza `toggle_panel_visibility`, ya existente desde Fase 3); Search reutiliza literalmente `editor_state.find_in_project_requested` (el mismo flag que ya dispara Ctrl+Shift+F y el ítem del menú Edit) en vez de reinventar su propio abrir+foco+`focus_query_field`; Extensions no tiene panel dockeable que togglear -- abre el modal de Plugins ya existente (`titlebar_panel.cpp`) a través de un parámetro nuevo `open_extensions_requested` en `DrawTitleBar`, resuelto con un frame de latencia (`pending_open_extensions`, declarado antes del loop) porque la Activity Bar se dibuja después de la title bar en el mismo frame -- mismo patrón de "un frame de demora entre paneles" que ya usan varios efectos cruzados de esta app (picks de Quick Open, clicks de archivo de Problems/Terminal). `Toolbox###toolbox` agregado a `kBuiltinPanelNames` (9 -> 10 entradas) y `DrawToolboxPanel` ahora acepta `bool* p_open` (antes no tenía botón de cierre ni entrada en `panel_open` -- se dibujaba sin más cuando el tab activo era un `.avaui` en Design view); formalizado como panel real (mismo patrón `panel_open`/`persist_if_closed` que el resto) sin tocar el gating de contexto que ya tenía (sigue sin dibujarse fuera de Design view, sólo ahora es cerrable/toggleable cuando sí aplica). Bug de identidad de menú corregido de paso, mismo tipo que los de nomenclatura de fases anteriores: el loop de `MenuItem` por cada panel que vivía en el menú View de la title bar (`kBuiltinPanelNames` + paneles de plugin) se eliminó por completo de `titlebar_panel.cpp` -- ya era una función duplicada tres veces (menú View, Command Palette desde Fase 3, y ahora Activity Bar), la nota de §5.9 pedía explícitamente que no quedara en dos lugares una vez que la Activity Bar existiera; `TitleBarResult::panel_toggle_requested` (ya sin ningún productor) y los parámetros `panels`/`closed_panels` de `DrawTitleBar` (ya sin ningún consumidor) se sacaron de la firma en vez de dejarlos muertos -- `main.cpp` actualizado en el único call site. El menú View quedó con un solo ítem (Command Palette...), que sigue cubriendo la lista completa de paneles (incluye Toolbox automáticamente al iterar `kBuiltinPanelNames`, sin tocar ese loop) para los paneles que la Activity Bar no representa como ícono (Terminal/Logs/Problems/Build/paneles de plugin). "Extensions" se agregó también como entrada de Command Palette (`menu.file.extensions` vía `pending_open_extensions`) -- ya no aplica la exclusión que tenía en Fase 3 ("modal sin trigger externo"), porque ahora sí lo tiene. Dockspace host desplazado: `dock_pos`/`dock_size` en `main.cpp` restan `kActivityBarWidth` (44px, mismo orden de magnitud que `kButtonWidth` de la title bar) del ancho total y del `x` inicial, con la Activity Bar ocupando esa franja a la izquierda -- mismo patrón de "ventana fija + dockspace host recalculado" que ya existía entre title bar y dockspace (`kTitleBarHeight`/`kTitleBarGap`). Nueva lambda `panel_visible(name)` en `main.cpp` (lee `settings.closed_panels` directo en vez del mapa `panel_open`, que recién gana una entrada para un panel dado la primera vez que ese panel se dibuja más adelante en el mismo frame -- la Activity Bar necesita saber su estado *antes* de eso) para calcular el highlight de cada ícono sin depender del orden de dibujado. No se agregó ninguna key de i18n nueva -- las cinco reutilizadas (`panel.explorer.title`, `panel.find_in_project.title`, `panel.toolbox.title`, `panel.settings.title`, `menu.file.extensions`) ya existían de fases i18n anteriores, mismo criterio de centralización de §7.2. `CMakeLists.txt`: `src/panels/activity_bar_panel.cpp` agregado a `STUDIO_SOURCES`. Balance de llaves/paréntesis de los 8 archivos tocados confirmado. |
| **8+** | ⬜ Roadmap | Marketplace de extensiones, Debugger, multi-root workspace, VCS |

**Siguiente fase a trabajar: 8+ (Roadmap)** -- con la Fase 7 (Activity Bar) cerrada, se completa todo el orden principal de §9: el bloque i18n de §6.5 (i18n-0 a i18n-9), la Fase 0 (Plataforma/target en Build), la Fase 0.5 (limpieza de comentarios en AvaStudio), la Fase 1 (Run Project), la Fase 2 (Problems panel + modo Check), la Fase 2.5 (Find in file/Find in project), la Fase 3 (Command Palette), la Fase 4 (Quick Open), la Fase 5 ("Generar": nuevo archivo dentro de un proyecto existente), la Fase 6 (Wizard de "nuevo proyecto") y la Fase 7 (Activity Bar) ya están cerradas. Lo que sigue es exclusivamente roadmap sin comprometer (§4/§9, Fase 8+: Marketplace de extensiones, Debugger con breakpoints, multi-root workspace, integración VCS) más los pendientes ya diferidos explícitamente a lo largo del plan y sin bloquear ninguna fase: extender la limpieza de comentarios de 0.5 al resto de proyectos de AvaLang (`avalang`/`avaui`/`avahost`/`avapack`/`avacli`) cuando convenga, la revisión humana de `runtime/avastudio/COMMENT_ARCHIVE.md`, el panel "Output" nuevo + agrupamiento visual de pestañas del panel inferior (§5.6, deliberadamente diferido de la Fase 2), la migración i18n diferida de `engine_bridge.cpp` (marcador `"Run <path>"` de `StartScriptRun`/`PollScriptRun`, ver i18n-7), "Replace in Project" (diferido de la Fase 2.5), "About" como comando de paleta deliberadamente fuera de alcance (ver nota de Fase 3, todavía vigente -- a diferencia de Extensions, sigue sin trigger externo), formalizar explícitamente la "Generación desde el catálogo" de §3.1 como parte de "Generar" (diferido de la Fase 5, ver su nota), y cualquier noción futura de manifest de proyecto o `ava_cli new` que reemplace el scaffolding determinístico de la Fase 6 (ver su propia nota de alcance).

