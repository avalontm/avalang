# Ava Studio — Arquitectura de la Aplicación

Este documento cubre Ava Studio como aplicación ImGui en sí misma:
paneles, el modelo de tabs, y -- lo más importante para el resto de la
serie -- **los tres árboles de UI distintos que conviven hoy en el mismo
proceso**, sin confundirse entre sí. Complementa, no reemplaza, a
`AGENTS_STUDIO.md` (que es más bien un changelog de sesión sobre el
Code Editor) y a `docs/history/DESIGNER_VIEW_SESSIONS.md`/
`docs/history/DESIGNER_CANVAS_UX_SESSIONS.md` (el plan histórico, fase
por fase, de cómo se construyó el Designer).
Este doc es la foto del estado actual, no la bitácora de cómo se llegó.

## 1. Layout de paneles

Dockspace de ImGui armado una vez en `main.cpp` (`DockBuilder*`, solo
corre si no hay layout guardado todavía):

```
┌─────────────┬───────────────────────────────┬──────────────┐
│             │                                │              │
│  Explorer   │                                │  Properties  │
│  (o         │        Code Editor             │              │
│  Toolbox,   │   (Code / Design por tab,       │              │
│  ver 1.1)   │    ver sección 2)               │              │
│             │                                │              │
├─────────────┴───────────────────────────────┴──────────────┤
│              Preview          │            Output           │
│         (tab, junto a Output en el mismo dock)               │
└───────────────────────────────────────────────────────────┘
```

| Panel | Archivo | Qué muestra |
|---|---|---|
| Explorer | `panels/explorer_panel.*` | Árbol de archivos de la carpeta abierta; abrir/renombrar/borrar dispara callbacks que `main.cpp` aplica sobre `EditorState` (`OpenFileInTab`, `CloseTabForPath`, `RenameTabPath`) |
| Toolbox | `panels/toolbox_panel.*` | Catálogo de componentes arrastrables (`component_catalog.cpp`) para soltar en el Designer canvas. Ocupa el mismo slot de dock que Explorer -- **solo se dibuja** cuando el tab activo es `.avaui` y está en `TabViewMode::Design` (ver sección 2) |
| Code Editor | `panels/editor_panel.*` | El host de tabs -- por cada `EditorTab` dibuja `TextEditor` (modo Code) o `designer_canvas.cpp` (modo Design, solo si `is_avaui`) |
| Properties | `panels/properties_panel.*` | Tabla editable de props/eventos de la selección actual -- alimentada tanto por Preview (read-only) como por el Designer (editable), vía el mismo struct `PropertiesState` (sección 3) |
| Preview | `panels/preview_panel.*` | Árbol de solo-lectura de un `Component` **runtime real** -- ver sección 3, es el único panel que hoy toca `core::Component` en vez de `DesignNode` |
| Output | `panels/output_panel.*` | Consola de ejecución de `EngineBridge` (stdout de `print()`, errores, resultado de `ava_run`) -- nada que ver con árboles de UI, es la consola del lenguaje |

## 2. El modelo de tabs (`EditorState`/`EditorTab`)

`EditorState::tabs` es un `vector<unique_ptr<EditorTab>>` (heap-allocated
a propósito -- `autocomplete_config` guarda un puntero a su propio tab,
así que no puede moverse si el vector reubica memoria). Cada `EditorTab`
puede estar en uno de dos modos:

- **`TabViewMode::Code`** (default para todo archivo): dibuja
  `TextEditor editor` -- el buffer de texto plano con resaltado/
  autocompletado -- referenciado en el código y en
  `studio/patches/README.md` como `AvaStudio.md`, pero ese documento no
  está presente en `docs/`; el detalle de esa integración queda
  pendiente de documentar acá).
- **`TabViewMode::Design`** (solo si `EditorTab::is_avaui`, detectado
  por extensión `.avaui` al abrir el archivo): dibuja
  `designer_canvas.cpp` sobre `EditorTab::design` (un
  `design::DesignDocument`).

Un tab `.avaui` sigue teniendo su `TextEditor` disponible -- F7/"View >
Toggle Design View" (`ToggleTabViewMode`) alterna entre ver el mismo
archivo como formulario visual o como su fuente `.avaui` cruda, VS6-style
(el mismo par Code/Form-view que inspiró todo este panel, ver
`docs/history/DESIGNER_VIEW_SESSIONS.md` sección 0, y ahora
también en `17_AVAUI_FILE_FORMAT.md`).

Si `.avaui` falla al parsear al abrir, `avaui_load_error` queda seteado
y `design` se deja como un documento en blanco (`NewBlankAvauiDocument()`)
en vez de rechazar abrir el tab -- mismo criterio permisivo que un `.frm`
corrupto en VS6, que igual abre *un* formulario, aunque vacío.

## 3. Los tres árboles de UI que conviven hoy

Esto es lo que hace falta tener clarísimo antes de tocar cualquiera de
los otros 4 documentos de la serie -- Studio **no** tiene un solo modelo
de árbol de componentes en memoria, tiene tres, con propósitos distintos
y sin conexión real entre sí todavía:

1. **El árbol demo de Preview** (`EngineBridge::BuildDemoComponentTree`,
   panel `preview_panel.cpp`). Es un `core::Component` real, construido
   a mano vía la C API (`ava_ui_create_component`, `ava_ui_add_child`,
   etc. -- ver `10_AVAUI.md` sección 1) para validar que ese
   camino funciona de punta a punta. **No viene de correr un `.ava`** --
   el propio comentario del código es explícito: es un standin hasta que
   los builtins `page`/`stack`/`button` (`core/src/ui/builtins.cpp`)
   estén cableados a la VM. El panel lo muestra como árbol de solo
   lectura (`PreviewNode`, un mirror aparte, ni siquiera es `Component`
   directamente).
2. **El árbol `DesignNode` del Designer** (`EditorTab::design`, uno por
   tab `.avaui` abierto). Es el que edita el usuario a mano en el
   canvas -- selección, drag/drop, properties editables. Es el que
   documenta `12_LAYOUT.md` (el render pipeline todavía no tiene doc
   propio, ver `00_SYSTEM_ARCHITECTURE.md`).
3. **El árbol resuelto de imports** (`component_resolver.cpp`, cacheado
   por tab). Toma el `DesignNode` de (2) y expande cada `Componente()`/
   `import` en una copia "sintética" de otro `.avaui` -- sigue siendo
   `DesignNode`, nunca cruza a `Component`. Es lo que
   `designer_canvas.cpp` realmente dibuja (`root_to_draw`), no
   `EditorTab::design.root` directo.

**Ningún script `.ava` real corriendo hoy produce un árbol que Studio
pueda mostrar.** El único camino end-to-end probado es (1), y es
sintético. Esto es relevante para cuando exista un doc de widget
lifecycle (ver `00_SYSTEM_ARCHITECTURE.md`): el lifecycle
que ese doc formaliza es el del Designer (edición), no el de una app
corriendo -- ese segundo lifecycle todavía no se ejercitó ni una vez.

## 4. `EngineBridge`: compilar/correr scripts (aparte de todo lo anterior)

`EngineBridge` es un wrapper delgado sobre la C API (`avalang.h`) --
un `AvaVM` por sesión, `RunScript()` para compilar+correr, y una consola
acumulativa (`ConsoleLine`) que captura `print()` en vivo vía
`ava_vm_set_print_callback`. No tiene nada que ver con árboles de UI
excepto por `BuildDemoComponentTree` (sección 3, punto 1) -- están en el
mismo archivo porque ambos necesitan un `AvaVM*`, no porque sean
conceptualmente la misma cosa.

Nota aparte, sin relación con esta serie de docs pero documentada acá
porque vive en el mismo archivo: `SubmitConsoleInput` es scaffolding sin
terminar -- no hay `input()` en el lenguaje todavía porque `ava_run()`
corre el script sincrónico de punta a punta, y un `input()` bloqueante
congelaría la ventana. La solución (que corrutinas ya existen para esto,
`ava_coroutine_resume`) está anotada en el propio header pero no
implementada.

## 5. Qué de esto es deuda con `core/src/ui` (cross-referencia)

Ya cubierto en detalle en `10_AVAUI.md`, resumen aplicado a
Studio específicamente:

- `component_resolver.cpp` (sección 3, punto 3 de este doc) resuelve
  imports enteramente en `DesignNode` -- si el parser canónico de
  `core/src/ui/avaui_text.*` termina de reemplazar al de Studio (Fase A,
  ya en curso), este resolver es el siguiente candidato obvio a mirar,
  porque hoy reimplementa lógica de resolución que en teoría podría
  compartirse con cualquier host no-Studio que también necesite resolver
  imports.
- El demo tree de Preview (sección 3, punto 1) es, sin proponérselo, la
  prueba de que `core::Component` + la C API funcionan -- pero nadie
  conectó ese camino a un `.avaui` real todavía. Cuando los builtins
  `page`/`stack`/`button` se cableen a la VM, Preview podría pasar a
  mostrar el resultado de *correr* el tab activo en vez de un árbol fijo
  -- eso sería el primer uso real de `Component` fuera de una demo.

## 6. Relación con el resto de la serie

- `10_AVAUI.md` -- el problema de fondo (core vs studio)
  que este doc aplica concretamente a cada panel.
- `12_LAYOUT.md` -- el árbol (2) de la sección 3 (`DesignNode` del tab
  activo) es su input. Un doc de render pipeline todavía no existe.
- Docs de widget lifecycle y de contrato de render backends todavía
  no existen -- cuando se escriban, este doc es la referencia de qué
  NO es genérico en Studio (todo lo de `EditorTab`, dockspace, tabs,
  es 100% de Studio, no del contrato de backend). Ver
  `00_SYSTEM_ARCHITECTURE.md` para el estado de estos huecos.

## 7. Preguntas abiertas propias de Studio

- Multi-`TextEditor` por tab ya está resuelto (uno por `EditorTab`), pero
  El código deja anotado (referencia a un `AvaStudio.md` que no está
  presente en `docs/`) que si Explorer permite abrir el mismo
  archivo en paralelo desde dos lugares hace falta decidir identidad de
  tab por path, no por índice -- no verificado si eso ya se resolvió.
- ¿Cuándo se conecta Preview a un `.avaui`/`.ava` real en vez de al demo
  tree fijo? Depende de que los builtins de `core/src/ui/builtins.cpp`
  se registren en la VM (bloqueante, fuera del alcance de esta serie de
  docs).
- Si `component_resolver.cpp` migra a operar sobre `Component` en vez de
  `DesignNode` (siguiendo la misma dirección que el layout engine, ver
  `12_LAYOUT.md` sección 6), ¿se resuelve antes o después de que
  `DesignNode` mismo se reduzca a metadata liviana (Fase C de
  `10_AVAUI.md`)? Probablemente conviene después, para no
  migrar dos veces la misma pieza.
