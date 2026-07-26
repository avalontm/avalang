# Historia — Vista Design del Designer (sesiones de implementación)

Este documento es la bitacora historica, sesion por sesion, de como se
construyo la vista Design del Designer (parseo de `.avaui`, layout,
canvas). Es un registro de proceso, no arquitectura vigente: para el
estado actual del sistema ver `docs/architecture/`, en particular
`10_AVAUI.md`, `12_LAYOUT.md`, `16_STUDIO.md` y `17_AVAUI_FILE_FORMAT.md`.

Movido desde `docs/architecture/08_DESIGNER_VIEW_PLAN.md` (Parte II y
Anexo A) como parte de la reorganizacion de la documentacion. El
contenido de la Parte I (arquitectura vigente del formato `.avaui`) se
consolido en `docs/architecture/17_AVAUI_FILE_FORMAT.md`.

**Advertencia**: las marcas de estado ("Hecho"/"Pendiente") dentro de
este documento reflejan el momento en que se escribio cada sesion, no
el estado actual del proyecto.

---

## Parte II — Plan original (histórico)

*(Lo que sigue es el plan de fases tal como se escribió al arrancar el
proyecto. Se mantiene como referencia de diseño, pero sus marcas de
"ya implementado"/"pendiente" quedaron desactualizadas apenas se picó
código de verdad — para el estado REAL, usar la tabla de progreso al
inicio del documento o el Anexo A, sección 9.)*

## 4. Modo de interacción (VS6-style, dentro del mismo tab)

Cambios en `EditorTab` (`editor_panel.h`):

```cpp
enum class TabViewMode { Code, Design };

struct EditorTab {
    ...
    TabViewMode view_mode = TabViewMode::Code;
    bool is_avaui = false;          // true si file_path termina en .avaui
    DesignDocument design;          // ver sección 5 -- vacío si !is_avaui
};
```

- Al abrir un `.avaui` (`OpenFileInTab`), se detecta la extensión y se
  fuerza `view_mode = Design` en vez de cargar el `TextEditor` con el
  JSON crudo.
- `DrawEditorPanel`: si `tab.is_avaui`, en vez de `editor.Render(...)`
  llama a `DrawDesignerCanvas(tab.design)` o a `editor.Render(...)`
  sobre un buffer *derivado* (el `code` del JSON) según `view_mode`.
- **F7** y un ítem de menú "Ver código" / "Ver diseño" (mismo lugar
  donde hoy vive Ctrl+S/F5, cerca de la toolbar del editor) alternan
  `view_mode`. Igual que VS6: el toggle es por documento, no global.
- La pestaña (`EditorTab::DisplayName()`) puede mostrar un ícono
  distinto según `view_mode` para que se note en qué vista estás, tal
  como VS6 marcaba el ícono del nodo en el Project Explorer.

---

## 5. Piezas nuevas

### 5.1 `studio/src/design/component_catalog.{h,cpp}` (ya implementado en Fase 0)

Catálogo estático de tipos de componente disponibles para el Toolbox y
para inicializar props por defecto al soltar uno nuevo:

```cpp
struct ComponentTypeInfo {
    std::string type;              // "button", "text", "stack", ...
    std::string display_name;      // "Button" para el Toolbox
    std::vector<PropertyRow> default_properties;
    bool is_container;             // acepta hijos (stack/column/row/grid)
};
const std::vector<ComponentTypeInfo>& GetComponentCatalog();
```

Arranca con lo que ya lista `PROGRESS.md`: `Column/Row/Stack/Grid`,
`Text/Image/Spacer/Divider/Link`, `Button/TextBox/CheckBox/RadioButton`.

### 5.2 `studio/src/design/design_document.{h,cpp}` (ya implementado en Fase 0 — falta 1 campo)

El modelo editable. Host-side, análogo a `PreviewNode` pero mutable y
con identidad estable por nodo (necesario para selección/drag&drop).
Esto ya está escrito y compilando (Fase 0); el struct real terminó
levemente distinto de este boceto original (sin `ImRect last_rect` —
los rects viven aparte, en el `LayoutResult` que devuelve
`ComputeLayout`, ver 5.3 — y `events` es `std::vector<PropertyRow>` en
vez de `pair<string,string>`, reusando el mismo tipo que `properties`):

```cpp
struct DesignNode {
    std::string node_uid;   // generado, no visible al usuario -- para selección/hit-test
    std::string type;
    std::string id;         // el "id" visible/editable, ex. "btnGuardar"
    std::vector<PropertyRow> properties;
    std::vector<PropertyRow> events; // key = event name ("click"), value = handler function name
    std::vector<DesignNode> children;
};

struct DesignDocument {
    DesignNode root;
    std::string code_behind;    // bloque "methods" del .avaui -- código AvaLang real
    std::string selected_uid;   // "" = nada seleccionado
    bool dirty = false;
};
```

**Pendiente por el cambio de formato de la sección 3**: `state` (el
bloque `state` del `.avaui`, ej. `counter = 0`) todavía no tiene dónde
vivir en `DesignDocument` — hoy solo existe `code_behind` para
`methods`. Falta algo como `std::vector<PropertyRow> initial_state;`
antes de que el parser de 5.4 tenga dónde volcarlo. No lo agrego en
este documento porque es un cambio de código, no de plan; queda listo
para la próxima pasada de implementación.

```cpp
DesignDocument LoadAvauiFile(const std::string& path);   // ya implementado -- ver 5.4 para el parser que usa
bool SaveAvauiFile(const DesignDocument& doc, const std::string& path);
DesignDocument NewBlankAvauiDocument();                  // page vacío, para "Nuevo archivo .avaui"
```

### 5.3 `studio/src/design/layout_engine.{h,cpp}` (ya implementado en Fase 0)

Implementa en C++ lo que `07_COMPONENT_TREE.md` describe en prosa
(Column/Row/Stack/Grid) para convertir el árbol en rectángulos ImGui:

```cpp
void ComputeLayout(DesignNode& root, ImRect available_space);
```

Es el hueco más grande del punto 2 de arriba — sin esto no hay canvas.
Arranca simple: Column y Row nomás (los dos que ya tienen ejemplo en el
doc), Grid/Flex quedan para después.

### 5.4 `studio/src/design/avaui_json.{h,cpp}` (implementado como parser JSON en Fase 0 — a reemplazar)

Este archivo hoy es un parser/writer JSON (funciona, tiene tests
manuales, ver el mensaje de entrega de Fase 0). Con el cambio de
formato de la sección 3, su rol pasa a ser un parser de **texto con
indentación**, no JSON — el equivalente C++ de
`AvaComponentParser.cs` del prototipo .NET: reconoce las palabras
reservadas `properties`/`state`/`import`/`methods`/`view`, dentro de
`view` arma el árbol por indentación (`tipo` en una línea, `prop =
valor` en las siguientes, hijos por mayor indentación, `end` para
cerrar), y separa las props de evento (`click`, `onchange`, ... —
misma lista fija) del resto. `methods` se guarda como texto (bloque
`func ... end`), tal cual, para el `TextEditor` en modo Código —
no hace falta parsearlo acá, el lenguaje ya lo parsea al ejecutar.

Probablemente conviene renombrar el archivo (`avaui_text.{h,cpp}` o
similar) cuando se haga este cambio, ya que "json" en el nombre deja
de ser cierto — lo dejo como sugerencia, no lo renombro en este
documento.

### 5.5 `studio/src/panels/toolbox_panel.{h,cpp}` (ya implementado, Fase 2 base)

Lista de `GetComponentCatalog()` como fuente de drag (`ImGui::BeginDragDropSource`,
payload = el `type` del componente, bajo el id `kToolboxDragDropId`).
Se dockea al lado de Explorer (pestaña hermana, mismo dock que hoy
ocupa "Explorer" en `main.cpp` línea ~301) — solo tiene sentido
visible cuando el tab activo está en `view_mode == Design`, igual que
VS6 mostraba la Toolbox solo con un `.frm` abierto. Escrito y
compilando, pero todavía sin ese wiring de visibilidad condicional
(depende de `view_mode`, que es Fase 1 — ver más abajo).

### 5.6 `studio/src/panels/designer_canvas.{h,cpp}` (ya implementado, Fase 2 base)

`std::optional<PropertiesState> DrawDesignerCanvas(DesignDocument& doc, ImVec2 size)`
— el `size` es la única diferencia real con el boceto original (para
que el caller controle el tamaño exacto igual que hace con
`editor.Render(...)`). Llamado desde `DrawEditorPanel` en vez de
`editor.Render(...)` cuando `view_mode == Design` — ese wiring
concreto sigue pendiente de Fase 1, la función ya hace todo lo demás:

1. `ComputeLayout(doc.root, canvas_rect)`.
2. Dibuja cada `DesignNode` como un rectángulo wireframe con su
   `type`/`id` (no un render pixel-perfect del control real — eso es
   una fase posterior).
3. Click sobre un rect → `doc.selected_uid = node_uid`, y devuelve un
   `PropertiesState` (mismo struct que ya usa `properties_panel.h`,
   así no hace falta tocar ese panel en la Fase 1).
4. `ImGui::BeginDragDropTarget()` sobre cada contenedor → soltar un
   `type` del Toolbox agrega un `DesignNode` hijo nuevo con los
   `default_properties` del catálogo.
5. Reordenar/mover (drag de un nodo ya existente hacia otro
   contenedor) queda pendiente — es Fase 4, no Fase 2.

### 5.7 `properties_panel.cpp` — habilitar edición (resuelve el
`PROPERTIES_EDITABLE` que ya está anotado ahí)

Cambiar las celdas de valor de `TextUnformatted` a `InputText`/
`Checkbox`/`Combo` según el tipo de la prop, y devolver los cambios
(nuevo `PropertyEdit{key, new_value}` o similar) para que
`main.cpp`/`designer_canvas.cpp` los escriba de vuelta en el
`DesignNode` seleccionado y marque `doc.dirty = true`.

---

## 6. Fases propuestas

**Fase 0 — Fundaciones sin UI nueva (invisible para vos, pero
bloqueante)**
- `avaui_json.{h,cpp}` (parser + writer del formato de la sección 3).
- `layout_engine.{h,cpp}` con Column/Row nomás.
- `component_catalog.{h,cpp}` con los tipos de `PROGRESS.md`.

**Fase 1 — Abrir y ver (sin edición todavía)**
- `EditorTab::view_mode` / `is_avaui`, detección de extensión en
  `OpenFileInTab`.
- `designer_canvas.cpp` en modo solo-lectura: dibuja el árbol cargado,
  click selecciona y llena Properties (solo lectura, como hoy).
- F7 + ítem de menú para alternar a la vista de código (el `code`
  del JSON, en un `TextEditor` normal, editable como cualquier `.ava`).
- Explorer: ícono/color propio para `.avaui` (mismo lugar que la
  línea 82 de `explorer_panel.cpp`).

**Fase 2 — Drag&drop desde Toolbox**
- `toolbox_panel.cpp` + dock nuevo.
- Soltar un tipo en el canvas crea un `DesignNode`.
- Guardar (`Ctrl+S`) ya escribe un `.avaui` real vía `SaveAvauiFile`.

**Fase 3 — Properties editable con write-back**
- Sección 5.7 completa: editar una prop en el panel actualiza el
  `DesignNode` seleccionado y repinta el canvas en vivo.
- Borrar/duplicar componente seleccionado (Del, Ctrl+D) en el canvas.

**Fase 4 — Mover/reordenar y anidar por drag**
- Arrastrar un nodo ya puesto hacia otro contenedor o para reordenar
  dentro del mismo.

**Fase 5 — Generación de code-behind al doble-click (el flujo que ya
está en `AGENTS_STUDIO.md` como "Workflow Futuro")**
- Doble-click en un `Button` del canvas → si no existe, genera
  `func btnX_Click(sender, e) ... end` dentro de `doc.code_behind` y
  cambia a vista Código con el cursor ahí, igual que VS6.

**Fase 6 — Preview real ejecutando el árbol (opcional, depende de
`ui.*` builtins)**
- Recién acá hace falta resolver el punto 3 de la sección 2
  (`RegisterUIBuiltins`) para que "Run" sobre un `.avaui` realmente
  ejecute la UI en vez de solo mostrar el wireframe del Designer.

---

## 7. Archivos nuevos / a tocar (resumen)

**Nuevos:**
```
studio/src/design/component_catalog.h / .cpp
studio/src/design/design_document.h / .cpp
studio/src/design/layout_engine.h / .cpp
studio/src/design/avaui_json.h / .cpp
studio/src/panels/toolbox_panel.h / .cpp
studio/src/panels/designer_canvas.h / .cpp
```

**A modificar:**
```
studio/src/panels/editor_panel.h/.cpp   -- view_mode, F7, render dispatch
studio/src/panels/explorer_panel.cpp    -- ícono/color .avaui, abrir en Design por defecto
studio/src/panels/properties_panel.h/.cpp -- edición + write-back
studio/src/main.cpp                     -- dock del Toolbox, wiring designer_canvas <-> properties
studio/CMakeLists.txt                   -- agregar los .cpp nuevos de studio/src/design/
```

---

## 8. Preguntas abiertas antes de arrancar a picar código

1. ¿`.avaui` o `.avax`? (uso `.avaui` en este doc, es 1 constante para
   cambiar si preferís lo otro). La extensión sigue siendo una
   constante suelta — lo que cambió en la sección 0.1/3 es el
   *contenido* del archivo, no el nombre.
2. ¿El wireframe de la Fase 1-4 te sirve tal cual (rectángulos con
   type/id, sin estilo real de cada control), o preferís que desde el
   arranque el `button` se vea como un botón ImGui de verdad, `text`
   como texto real, etc.? Cambia el esfuerzo de `designer_canvas.cpp`
   pero no la arquitectura de las demás piezas.
3. ¿El Toolbox va dockeado como pestaña al lado de Explorer (mi
   propuesta, ocupa el mismo lugar solo cuando hay un `.avaui` activo),
   o lo preferís siempre visible en su propio dock?
4. **Nueva, por el cambio de sección 3:** ¿confirmás que seguimos con
   la migración de `avaui_json.{h,cpp}` (JSON, ya escrito en Fase 0) a
   un parser de la sintaxis `state/view/methods` real? Es el único
   cambio de este documento con impacto directo en código ya
   entregado — todo lo demás (`design_document`, `layout_engine`,
   `component_catalog`, `toolbox_panel`, `designer_canvas`) queda
   igual. Si preferís mantener JSON por ahora (por ejemplo para no
   tocar lo que ya compila) y migrar más adelante, también es válido —
   avisame cuál de las dos preferís antes de que toque el parser.

Con eso confirmado arranco por la Fase 0 + Fase 1 (son las que dejan
algo visible en pantalla: abrís un `.avaui`, ves el árbol en el
canvas, F7 muestra el código).

---

## Anexo A — Bitácora de sesiones (detalle completo, cronológico)

*(Fuente de verdad de qué se hizo realmente, sesión por sesión. La
numeración `9.x` se conserva tal cual para no romper las referencias
cruzadas que ya existen dentro de este mismo documento. Para el
resumen rápido, ver la tabla de progreso al inicio del documento.)*

Pregunta 4 de la sección 8 quedó resuelta: se migró. Este documento
había quedado escrito *antes* de tocar código para el cambio de
sección 3; esta sección es el registro de qué de todo lo de arriba
está hecho de verdad hoy en el repo, y qué sigue pendiente.

### 9.1 Hecho

- **`studio/src/design/avaui_text.{h,cpp}`** (nuevo, reemplaza
  `avaui_json.{h,cpp}` — borrado): parser/writer real de la sintaxis
  de sección 3, puerto directo de
  `AvaLang.UI/Rendering/AvaComponentParser.cs` del prototipo .NET:
  - Divide el archivo en bloques de nivel 0 (`properties`/`state`/
    `import(s)`/`services`/`methods`/`lifecycle`/`view`), cada uno
    cerrado por su propio `end` en columna 0 — pero de forma
    tolerante: si falta el `end`, corta en el siguiente keyword de
    sección o en EOF en vez de fallar (más permisivo que el propio
    parser .NET, que solo hacía eso para `properties`/`state`).
  - `view` se arma por indentación (mismo algoritmo que
    `TryParseComponent`/`ParseView`): tipo solo en una línea abre
    bloque, `prop = valor` (o `id = ...`, o un evento de la lista fija
    `click/onclick/onchange/...`) son props/eventos, `Palabra()` con
    paréntesis vacíos y en PascalCase es una llamada a componente
    importado (queda como nodo hoja con ese `type`, sin resolver
    todavía — ver 9.2).
  - Valores: string entre comillas dobles → se guardan sin comillas
    (convención existente de `PropertyRow::value`, "string ya
    listo para mostrar"); todo lo demás (números, `true`/`false`,
    identificadores sueltos, expresiones) se guarda tal cual. Al
    escribir: `true`/`false` y números van sin comillas, todo lo
    demás se cita y escapa. Es un round-trip con pérdida solo para el
    caso de expresiones con comillas embebidas (`"Contador: " +
    counter` volvería como un string citado plano) — aceptable porque
    el Designer todavía no genera ni edita expresiones, solo
    literales por defecto del catálogo (ver `component_catalog.cpp`).
  - `root` (el `DesignNode` sintético tipo `"page"`) recibe sus
    `properties` del bloque `properties` del archivo — no hay
    keyword `page` en el formato, section 3 lo aclara: el bloque
    `properties` a nivel de archivo *es* las props de la página.
  - `import "..."` se preserva verbatim (orden incluido) pero no se
    resuelve — ver 9.2.
- **`design_document.h/.cpp`**: `DesignDocument` ganó
  `initial_state` (bloque `state`) e `imports` (líneas `import`,
  sin resolver). `LoadAvauiFile`/`SaveAvauiFile` actualizados a las
  nuevas firmas de `avaui_text.h`.
- **`editor_panel.cpp`**: los tres call-sites de
  `ParseAvauiText`/`WriteAvauiText` (`SaveTab` en vista Código,
  `ToggleTabViewMode` en ambas direcciones) actualizados a las nuevas
  firmas — el flujo F7/guardar sigue siendo exactamente el descrito
  en sección 4, solo cambió qué hay *adentro* del archivo, no el
  wiring de la UI.
- **`studio/CMakeLists.txt`**: `avaui_json.cpp` → `avaui_text.cpp`.
- Sin tocar (porque ya operaban sobre `DesignNode`/`DesignDocument` en
  memoria, no sobre el formato de archivo, tal como anticipaba la
  sección 3 original): `layout_engine.*`, `component_catalog.*`,
  `designer_canvas.*`, `toolbox_panel.*`.

No se compiló nada de esto todavía (a pedido explícito) — es texto
revisado a mano contra el puerto de `AvaComponentParser.cs`, no
verificado con el compilador real ni con un `.avaui` de prueba
cargado en Ava Studio.

### 9.2 Falta (huecos reales, no cosméticos)

1. **Resolución de `import "components/x"` / llamadas `Componente()`.**
   Hoy el parser los deja como datos sueltos (`DesignDocument::imports`
   como lista de strings; un nodo `Navbar()` como hoja con
   `type = "Navbar"` y nada más) pero nada busca
   `components/x.avaui`, lo parsea, ni sustituye el nodo por su árbol
   real — es exactamente el rol de `ComponentResolver.cs` en el
   prototipo .NET, todavía sin equivalente C++. Bloquea, entre otras
   cosas, que el canvas pueda mostrar un `Navbar()` como algo más que
   una caja vacía. No es prioridad de Fase 0-2 (no hay Explorer
   multi-archivo con imports en el Designer todavía).
2. **`state` no se evalúa ni se bindea a nada.** `initial_state` se
   lee/escribe y ya está — pero como anticipaba la sección 2 punto 3,
   nada ejecuta expresiones como `"Contador: " + counter` contra él
   (no hay VM.Eval equivalente conectado). El canvas dibuja los
   valores tal cual están escritos en el archivo, no valores
   "evaluados". Sigue siendo Fase 6 (`ui.*` builtins), no un bloqueo
   de las fases anteriores.
3. **No hay ningún `.avaui` de prueba real ni test automatizado** para
   este parser — a diferencia de la mención de "tests manuales" de la
   entrega JSON anterior (ya obsoleta). Antes de dar por buena esta
   migración conviene: (a) escribir a mano el ejemplo de la sección 3
   como archivo `.avaui` y abrirlo en Ava Studio compilado, (b)
   round-trip manual F7 ida y vuelta sin que se pierda nada, (c) un
   caso con `Navbar()` para confirmar que al menos no rompe el
   parseo aunque no se resuelva.
4. **Sin compilar.** Todo lo de 9.1 es revisión manual, no build real
   — el primer paso antes de seguir con cualquier fase nueva debería
   ser compilar (`build_studio.bat`) y cazar errores de tipos/includes
   que la revisión manual no vio.
5. Todo lo que ya listaba la sección 2 original y no cambió con esta
   migración (motor de layout más allá de Column/Row, `ui.*`
   builtins registrados, catálogo de props más rico) sigue en pie tal
   cual estaba.

### 9.3 Próximo paso sugerido

Compilar (punto 4) y probar carga/guardado de un `.avaui` de ejemplo
(punto 3) antes de avanzar a Fase 2 (drag&drop) o a resolver imports
(9.2 punto 1) — son las dos formas más baratas de encontrar errores
en el puerto del parser antes de construir más cosas encima.

**Superado por 9.4** — en vez de eso, la sesión siguiente unificó el
parser en `avalang.dll` (recomendado aparte, no era parte de este
documento) antes de compilar el proyecto completo; ver abajo.

### 9.4 Unificación en `avalang.dll` (hecho, en dos entregas)

Cambio de arquitectura, no de formato de archivo: la gramática de 9.1
(que en ese momento vivía duplicada en `studio/src/design/avaui_text.cpp`,
puerto C++ de `AvaComponentParser.cs`) se centralizó en el core del
lenguaje para que cualquier host (Ava Studio, el binding .NET, uno
futuro) la use vía la C API en lugar de mantener su propia copia a
mano — el mismo riesgo de "dos implementaciones de la misma gramática
que pueden divergir" que ya advertía 9.2 punto 3, pero a nivel de
lenguaje en vez de a nivel de host.

**Entrega 1:**
- **`core/src/ui/avaui_text.h` / `.cpp`** (nuevo): el parser/writer de
  9.1 re-portado para construir directamente árboles
  `ava::ui::Component` (el mismo modelo que ya usa
  `ava_ui_create_component`/`ava_ui_create_tree`), en vez de
  `DesignNode` (Studio) o `ComponentNode` (C#). Vive en
  `core/src/ui/`, compilado dentro de `avalang.dll`.
- **`core/src/ui/component.h`**: agregado `Component::GetAllEvents()`
  (insertion-ordered, igual que `GetAllProperties()`) — hacía falta
  para que el writer pueda serializar de vuelta los eventos de un
  nodo, y no existía ningún accessor para enumerarlos.
- **`public/include/avalang.h`** / **`public/src/c_api.cpp`**:
  `ava_ui_parse_avaui_text` / `ava_ui_write_avaui_text` / `ava_ui_text_free`
  — la gramática de 9.1 expuesta como C API. `ava_ui_parse_avaui_text`
  devuelve un `AvaComponentTree*` normal (mismo modelo que
  `ava_ui_create_tree`) más tres out-params para las secciones que no
  encajan en el modelo de Component (`state` como JSON plano,
  `imports` como JSON plano, `methods` como texto verbatim).
- **`CMakeLists.txt`** (raíz): agregado `core/src/ui/avaui_text.cpp` a
  `CORE_SOURCES`.
- Bug encontrado y corregido durante esta entrega (con un test
  standalone real, no solo revisión manual): un componente en forma de
  llamada (`Navbar()`) sin línea en blanco después de él hacía que el
  parser se comiera la línea del sibling siguiente en un segundo
  parse pass — off-by-one heredado tal cual de la lógica original de
  9.1 (y presente también, según lo revisado, en
  `AvaComponentParser.cs`). Corregido en el nuevo parser de
  `core/src/ui/`; **9.1 nunca se corrigió con este fix** porque quedó
  reemplazado por completo en la entrega 2 (ver abajo) — no hace falta
  aplicarlo dos veces.

**Entrega 2:**
- **`public/include/avalang.h`** / **`public/src/c_api.cpp`**: cuatro
  funciones de enumeración —
  `ava_ui_property_count`/`ava_ui_property_key_at` y
  `ava_ui_event_count`/`ava_ui_event_key_at` — para que un caller
  recorra todas las props/eventos de un `AvaComponent` por índice sin
  conocer sus claves de antemano (`ava_ui_get_property`/`get_event`
  existentes piden la clave). Hacía falta específicamente para que el
  adaptador de abajo pueda convertir un `AvaComponent` a `DesignNode`
  sin una lista fija de props posibles por tipo de componente.
- **`studio/src/design/avaui_text.cpp` reescrito por completo**: dejó
  de ser el puerto C++ de 9.1 (duplicado) y pasó a ser un adaptador
  delgado — llama a `ava_ui_parse_avaui_text`/`ava_ui_write_avaui_text`
  de `avalang.dll` y solo convierte `AvaComponent` ↔ `DesignNode` (con
  las cuatro funciones de enumeración de arriba) más un encode/decode
  chico del JSON plano de `state`/`imports` (mismo escapado que el
  core, carácter por carácter: `\" \\ \n \r \t`). Las firmas públicas
  de `studio/src/design/avaui_text.h` no cambiaron — ningún otro
  archivo de `studio/` (`design_document.cpp`, `editor_panel.cpp`)
  necesitó tocarse.
- Detalle de la API resuelto en esta entrega: `ava_ui_set_property`/
  `set_event` piden un `ava_value_t`, y crear uno de tipo string
  normalmente pediría una `AvaVM*` viva (`ava_string_create(vm, ...)`)
  — pero `c_api.cpp` ignora ese parámetro por completo, así que el
  adaptador de Studio pasa `nullptr` sin necesitar levantar una VM solo
  para mover strings de propiedades a través del límite C.
- **`studio/src/design/avaui_json.h` / `.cpp` borrados** — quedaban sin
  ninguna referencia real en el proyecto desde la migración de 9.1 (el
  único rastro era un comentario).
- Verificado con `g++ -fsyntax-only -std=c++20` (limpio, no es un
  build real): `c_api.cpp` completo, el nuevo `avaui_text.cpp` de
  Studio, y `design_document.cpp` (para confirmar que sigue
  compilando contra el header sin cambios de firma).

**Qué NO cambió con esta unificación** (para que quede claro el
alcance): el *formato* del archivo `.avaui` sigue siendo exactamente
el de la sección 3 — esto es un cambio de *dónde vive el código* que
lo lee/escribe, no de qué dice el archivo. Todo lo demás de 9.1
(`design_document.h/.cpp`, `layout_engine.*`, `component_catalog.*`,
`toolbox_panel.*`, `designer_canvas.*`) sigue igual, no dependía del
parser directamente.

**Sigue pendiente** (superset de 9.2, sin cambios por esta
unificación salvo lo tachado):
1. Resolución de `import`/`Componente()` — igual que 9.2 punto 1, sin
   tocar.
2. `state` sin evaluar/bindear — igual que 9.2 punto 2, sin tocar.
3. Sin ningún `.avaui` de prueba real cargado en Ava Studio compilado
   — igual que 9.2 punto 3, sin tocar (el "test standalone" de la
   Entrega 1 fue aparte, sobre el parser de `core/` directamente, no
   sobre el `.avaui` real de la sección 3 vía Ava Studio).
4. **Sin compilar el proyecto completo** — ~~lo único verificado en
   las dos entregas fue syntax-check aislado por archivo
   (`g++ -fsyntax-only`), no `build.bat`/`build_studio.bat` reales.~~
   **Actualizado:** compilado por vos con `build.bat`/`build_studio.bat`
   reales y confirmado que funciona — este punto queda resuelto. Sigue
   sin haber, eso sí, una prueba puntual de un `.avaui` real cargado en
   el Studio ya compilado (ver 9.5 punto 1).
5. El resto de la sección 2 original (motor de layout más allá de
   Column/Row, `ui.*` builtins registrados, catálogo de props más
   rico) sigue en pie tal cual.

### 9.5 Estado al cierre de esta sesión / qué sigue

**Confirmado en esta sesión:** el proyecto compila completo
(`avalang.dll` + Ava Studio) con la unificación de 9.4 adentro y
corre. No se probó todavía, dentro de esta misma sesión, abrir un
`.avaui` real en el Studio ya compilado — eso es lo primero de la
lista de abajo.

Para la próxima sesión, en orden:

1. ~~**Probar un `.avaui` real en el Studio compilado.**~~ **Hecho en
   esta sesión, con una salvedad — ver 9.6.**
2. **Resolución de `import "components/x"` / llamadas `Componente()`**
   (9.2 punto 1, sin tocar todavía). Hoy quedan como datos sueltos —
   nada busca `components/x.avaui`, lo parsea, ni sustituye el nodo
   por su árbol real. Es lo que bloquea que el canvas muestre un
   `Navbar()` como algo más que una caja vacía, y es el gap más
   grande que queda antes de que el Designer soporte multi-archivo.
3. **Fase 2 del plan original (drag&drop desde Toolbox)** — sección 6:
   soltar un tipo del catálogo en el canvas crea un `DesignNode`, y
   `Ctrl+S` ya escribe un `.avaui` real vía `SaveAvauiFile` (que ya
   delega en `avalang.dll` desde 9.4, así que esta fase no debería
   tocar el parser en absoluto, solo `toolbox_panel.cpp`/
   `designer_canvas.cpp`/el wiring de `main.cpp`).
4. **`state` sin evaluar/bindear** (9.2 punto 2) — depende de
   `ui.*` builtins (Fase 6), no bloqueante todavía.
5. **El binding .NET (`AvaComponentParser.cs`) sigue sin migrar** a la
   C API de `avalang.dll` — quedó fuera de alcance en las sesiones de
   unificación (se pidió enfocarse solo en el repo `avalang`, C++). Si
   se retoma esa unificación completa, es la pieza que falta del otro
   lado.

### 9.6 Round-trip del `.avaui` de la sección 3 (hecho, con una salvedad)

**Salvedad primero, para que quede clara:** esto **no** se probó
dentro de Ava Studio compilado con GUI — este entorno no puede correr
un binario de escritorio (ImGui/Windows). Lo que sí se hizo es el
equivalente de más bajo nivel que ya se había usado en la Entrega 1 de
9.4 ("un test standalone real, no solo revisión manual"): un ejecutable
aparte que enlaza directo contra el parser ya unificado en
`core/src/ui/` (el mismo código que `avalang.dll` expone via
`ava_ui_parse_avaui_text`/`ava_ui_write_avaui_text`, y que el adaptador
de Studio en `studio/src/design/avaui_text.cpp` llama sin lógica propia
de parseo) — así que cubre la gramática real, aunque no pasa por
ImGui/`editor_panel.cpp`/F7 en sí. Punto 1 de la lista de arriba sigue
sin una prueba dentro del Studio con GUI; si eso importa
específicamente (vs. probar el parser que el Studio usa), sigue
pendiente.

**Qué se agregó:** `core/tests/avaui_text_roundtrip_test.cpp` —
programa standalone (no enganchado a `CMakeLists.txt`, con el comando
de compilación en un comentario arriba del archivo) que:

1. Parsea el ejemplo exacto de la sección 3 de este documento y
   verifica la forma del árbol resultante (`page` → `column` con
   `fill`/`padding`/`gap` → `Navbar()` + `text` + `button`), el bloque
   `state` (`counter = 0`), `imports` (`components/navbar`) y que
   `methods_text` conserva `btnGuardar_Click`.
2. Llama a `WriteAvauiText` sobre ese árbol y vuelve a parsear el
   resultado — el round-trip F7 (código → diseño → código) — y repite
   las mismas verificaciones.
3. Confirma que un segundo ciclo escritura→parseo produce texto
   idéntico al primero (el writer es estable, no hay drift entre
   pasadas).
4. Agrega el caso pedido: un `Navbar()` sin línea en blanco antes del
   siguiente sibling (`text`) — la forma exacta que gatillaba el bug
   off-by-one corregido en la Entrega 1 de 9.4 — y confirma que ambos
   hijos sobreviven, tanto en el primer parseo como después de un
   round-trip completo de escritura/reparseo.

**Resultado: las 33 verificaciones pasan** (compilado con
`g++ -std=c++20`, sin warnings, enlazando solo
`avaui_text.cpp`/`component.cpp`/`property.cpp`/`event.cpp`/`value.cpp`
— sin dependencias de ImGui ni del resto de Studio).

**Un detalle de normalización encontrado, no un bug:** `fill = "true"`
en el archivo original vuelve como `fill = true` (sin comillas)
después de escribirlo — es el comportamiento documentado en
`avaui_text.h`/`WritePropertyValue` (`"true"`/`"false"` se emiten sin
comillas), no una pérdida de dato; el valor sigue siendo el string
`"true"` en ambos parseos. Se deja anotado por si alguna vez se
depende de la diferencia entre `true` literal (booleano) y `"true"`
string en algún prop — hoy el parser no distingue esos dos casos en
absoluto (todo es string, ver el header), así que no hay forma de que
divergiera.

### 9.7 Resolución de `import`/`Componente()` (hecho)

**Qué resuelve** (punto 2 de la lista pendiente en 9.5, punto 1 de la
sección 2 original): que un `import "components/x"` y una llamada
`X()` en el `view` de un `.avaui` dejen de ser datos sueltos y se
expandan al árbol real del componente importado.

**Archivos nuevos** (agregados a `studio/CMakeLists.txt` junto a los
otros de `design/`, no rompen nada existente):
- `studio/src/design/component_resolver.h` / `.cpp` -- el equivalente
  C++ de `ComponentResolver.cs` del prototipo .NET (ver sección 0.1),
  adaptado a `DesignNode`/`DesignDocument` en vez de
  `ComponentNode`/`ScriptCache` (acá no hay `AvaVM` de por medio -- el
  Designer no evalúa expresiones todavía, ver sección 2 punto 3, esto
  solo mueve estructura de árbol).

**Diseño, igual que 9.2/lo ya resumido antes de esta sesión:**
- `ComponentResolver(base_dir)` -- `base_dir` es la raíz del proyecto,
  fija para toda la recursión (no el directorio del archivo que
  importa en cada momento) -- mismo motivo que
  `ComponentResolver.cs::_basePath` y el mismo bug que hay que evitar
  si algún día se re-deriva por archivo: un import anidado escrito
  *dentro* de un componente ya importado tiene que seguir resolviendo
  contra la raíz, no contra `components/`.
- `LoadComponent`/`ResolveImports(doc)` -- parsean el `.avaui`
  importado vía `LoadAvauiFile` (el mismo parser real, no uno nuevo),
  cachean por `import_path` para no releer/reparsear dos veces en la
  misma pasada, y recurren en los imports del propio componente
  importado.
- El árbol del componente cacheado es `imported_doc.root.children[0]`
  (el único hijo del `page` sintético que envuelve todo archivo
  `.avaui`, ver `design_document.h`) -- coincide con la forma de
  `navbar.ava` y el resto de archivos reales revisados en
  `AvaLang.UI.Web/Pages/`. Si el `view` tuviera más de un nodo raíz
  (no visto en ningún archivo real del proyecto), cae de vuelta al
  `page` completo en vez de romper.
- `ResolveComponentCall(node)` / `ResolveTree(root)` -- una llamada
  `X()` (nodo hoja con `type` PascalCase, igual chequeo que
  `char.IsUpper(type[0])` en el original) se reemplaza por una COPIA
  del árbol cacheado, con `node_uid` regenerado en cada nodo de la
  copia (misma razón que `MakeNode`/`LoadAvauiFile`: dos usos del
  mismo componente no pueden compartir uid o la selección del canvas
  se rompe). `ResolveTree` no muta `doc.root` -- se llama sobre una
  copia del árbol, exactamente la misma decisión de diseño (y el mismo
  motivo: no hornear el árbol expandido dentro del `.avaui` guardado)
  que ya se había tomado para el resolver en la sesión anterior de
  C++.
- Ciclos (`A` importa `B` importa `A`) -- se corta con una pila de
  nombres en expansión (`expansion_stack`); el nodo más interno del
  ciclo queda sin resolver ("caja vacía"), no cuelga.
- Referencias sin import declarado, o import a un archivo que no
  existe en disco -- silenciosas, quedan como estaban (mismo
  comportamiento tolerante que `ParseAvauiText`).

**Pendiente, no bloqueante (sin tocar en esta sesión):**
1. Cablear `ComponentResolver` en `designer_canvas.cpp` para que el
   canvas lo use al dibujar -- es trabajo de UI, no de resolución, y
   sigue sin tocarse (mismo alcance acotado que ya se venía
   siguiendo).
2. `state` sin evaluar/bindear (sección 2 punto 2) -- sin cambios.
3. El binding .NET (`AvaComponentParser.cs`/`ComponentResolver.cs`)
   sigue sin migrar a la C API de `avalang.dll` -- sin cambios (sigue
   fuera de alcance, ver 9.5 punto 5).

### 9.8 Cablear el ComponentResolver en el canvas (hecho)

**Hallazgo primero, importante:** al auditar el punto 3 original de la
lista de 9.5 ("Fase 2 del plan -- drag&drop desde Toolbox") resultó
que **ya estaba completo** en el repo que me pasaron -- no coincidía
con lo que decía 9.5. Verificado uno por uno: `toolbox_panel.h/.cpp`
(drag source), `designer_canvas.cpp::HandleDropTarget` (drop target
real, agrega `DesignNode` al soltar), `editor_panel.cpp::DrawEditorPanel`
(despacha a `DrawDesignerCanvas` en modo Design) y `SaveTab` (ya llama
`SaveAvauiFile`), `main.cpp` (Toolbox dockeado y con visibilidad
condicional a `view_mode == Design`, `designer_selection` ya
alimentando `properties_state`), e ícono `.avaui` en Explorer. Nada
de esto necesitó tocarse. El comentario "Not wired into
DrawEditorPanel/main.cpp yet" que quedaba en `designer_canvas.h`
estaba desactualizado -- reemplazado.

**Lo que sí faltaba y se hizo en esta sesión:** que el canvas dibuje
el árbol REAL de un `Navbar()` en vez de la caja vacía, usando el
`ComponentResolver` de 9.7.

**Diseño:**
- `EditorState::project_root` (nuevo campo, `editor_panel.h`) -- la
  misma raíz fija de proyecto contra la que resuelven los imports
  (ver 9.7), seteada una vez en `main.cpp` justo después de
  `ResolveWorkspaceDir()`, mismo valor que usa Explorer
  (`explorer_state.root_dir`). Vacío por defecto -- ver abajo por qué
  eso importa.
- `DrawDesignerCanvas(doc, size, project_root = "")` -- nuevo tercer
  parámetro, con default vacío para no romper ningún otro call site
  hipotético (hoy solo hay uno, `editor_panel.cpp`, ya actualizado
  para pasar `state.project_root`). Con `project_root` vacío el
  comportamiento es IDÉNTICO a antes de esta sesión (caja vacía sin
  resolver nada) -- importante porque así un test que llame a esta
  función sin querer resolución no se ve afectado.
- Adentro, si `project_root` no está vacío, arma un
  `ComponentResolver` NUEVO cada vez que se llama (o sea, en la
  práctica, cada frame mientras el tab está a la vista) y le hace
  `ResolveImports(doc)`. Anotado como límite de performance conocido,
  no bloqueante: hoy relee/reparsea cada `.avaui` importado del disco
  una vez por frame -- aceptable para la cantidad de componentes que
  tiene el proyecto hoy; si algún día es un problema real, cachear el
  resolver a nivel `EditorTab` (invalidando al guardar) es el arreglo,
  no hecho ahora sin evidencia de que haga falta.
- `DrawNode` (dentro de `designer_canvas.cpp`) ahora recibe un
  `const ComponentResolver*` (`nullptr` = comportamiento viejo) y un
  `bool synthetic`. Al recorrer los hijos de un nodo: si el hijo es
  una llamada a componente (`ComponentResolver::IsComponentCall`,
  ahora público) y el resolver tiene un componente cacheado con ese
  nombre, en vez de dibujar el nodo original se resuelve una COPIA
  descartable (`ResolveComponentCall`, uids frescos) y se dibuja esa
  copia completa con `synthetic = true`, con su propio
  `ComputeLayout` interno acotado al rect que el layout principal ya
  le había asignado al sitio de la llamada.
- **`synthetic = true` implica dos cosas:** (1) el label del nodo
  lleva un sufijo `[import]` para que se note a simple vista que viene
  de un archivo importado, no del árbol editable; (2) NO acepta drops
  del Toolbox (`HandleDropTarget` se saltea) -- soltar ahí no tiene
  dónde persistir de verdad (la copia se descarta al siguiente frame),
  así que en vez de aceptar el drop silenciosamente y perderlo, no se
  ofrece en absoluto. El click SÍ sigue funcionando (selección/
  Properties de solo inspección), igual que cualquier nodo.
- **NO se muta `doc.root`** en ningún momento -- exactamente la misma
  decisión de diseño que ya se había tomado para `ResolveImports` en
  9.7 y para el resolver del `core/` en la sesión anterior a esta;
  ahora se sostiene también en el punto donde de verdad se dibuja.

**Límite conocido, no bloqueante (documentado en el header de
`designer_canvas.h`):** el espacio que el nodo de la llamada
(`Navbar()`) reserva en el layout de su padre sigue calculado sobre
el árbol SIN resolver (un nodo sin hijos = una fila de altura fija,
ver `layout_engine.cpp`) -- la resolución de esta sesión solo llena
ese espacio ya reservado con el layout real del componente, no hace
que el padre se reacomode si el componente resuelto es más alto que
esa fila. Arreglarlo de verdad requiere que `layout_engine.cpp` sepa
de resolución, cambio más grande, fuera de alcance acá.

**Pendiente, no bloqueante:**
1. El límite de layout de arriba (altura del call-site sin ajustar al
   contenido resuelto).
2. Cachear el `ComponentResolver` por `EditorTab` en vez de reconstruirlo
   cada frame, si el I/O a disco por frame llega a notarse.
3. Todo lo demás sigue igual que 9.5/9.7 (Fase 3 properties
   editable, `state` sin evaluar, binding .NET sin migrar).

### 9.9 Fase 3 -- Properties editable con write-back (hecho)

**Qué resuelve:** el `PROPERTIES_EDITABLE` pendiente que
`properties_panel.h`/`preview_panel.cpp` dejaban anotado desde el
Milestone 1 -- editar un valor en el panel Properties ahora escribe de
vuelta al `DesignNode` real (y por lo tanto, al guardar, al `.avaui` en
disco), no solo se ve en la tabla.

**Diseño:**
- `PropertiesState` ganó `editable` (bool), `source_tab_id` (int,
  matchea `EditorTab::id`) y `selected_node_uid` (string, matchea
  `DesignNode::node_uid`). Los tres quedan en su default
  (`false`/`-1`/vacío) para toda selección del Preview panel (árbol
  demo de `EngineBridge`, sin archivo fuente al que escribir) y para
  una selección **synthetic** del Designer canvas (nodo de un
  `Componente()` resuelto vía `ComponentResolver`, ver 9.8 -- no hay
  `DesignNode` real en ningún `doc.root` para esos). Solo una
  selección real de `doc.root` en el canvas los llena.
- `designer_canvas.cpp::ToPropertiesState` recibe ahora `editable`
  (pasado como `!synthetic` desde `DrawNode`) y `tab_id` (threadeado
  desde el nuevo parámetro de `DrawDesignerCanvas`, ver abajo).
- `DrawDesignerCanvas(doc, size, project_root = "", tab_id = -1)` --
  nuevo cuarto parámetro, default `-1` para no romper ningún call site
  hipotético (hoy solo `editor_panel.cpp`, actualizado para pasar
  `tab.id`). `DrawNode` y su recursión (incluida la rama de nodos
  resueltos de 9.8) ahora llevan `tab_id` de punta a punta.
- **`properties_panel.cpp` reescrito**: `DrawPropertiesPanel` pasa a
  tomar `PropertiesState&` (ya no `const&`) y devuelve
  `std::optional<PropertyEdit>`. Cuando `state.editable`, cada fila usa
  `ImGui::InputText` (overload de `imgui_stdlib.h`, ya vinculado en
  `CMakeLists.txt` -- lo usa `output_panel.cpp`, no hacía falta
  agregar nada al build) enlazado directo a `PropertyRow::value`, así
  que la tabla misma refleja cada tecla sin buffer intermedio. Al
  soltar el campo (`ImGui::IsItemDeactivatedAfterEdit` -- unfocus o
  Enter, no cada tecla) arma y devuelve un `PropertyEdit{tab_id,
  node_uid, key, new_value}`. Sin `state.editable`, cae al
  `TextUnformatted` de siempre (selección de Preview, o de un nodo
  synthetic del Designer).
- **`main.cpp`**: `properties_state` (ya existía, sobrevive entre
  frames incluyendo cambios de tab) se pasa ahora por referencia
  mutable. Si `DrawPropertiesPanel` devuelve un `PropertyEdit`, se
  busca el tab por `id` (no por índice -- los índices cambian al
  abrir/cerrar tabs), se llama `design::FindNodeByUid(tab.design.root,
  node_uid)` (ya existía en `design_document.h`, sin tocar) y se
  actualiza el `PropertyRow` con esa `key` dentro de
  `node->properties`. `tab.design.dirty = true; tab.dirty = true;` se
  setean ahí mismo (mismo patrón que `HandleDropTarget`/`SaveTab`) --
  el punto donde `DrawEditorPanel` ya propaga
  `design.dirty -> tab.dirty` corre en su propia llamada anterior en
  el mismo frame, así que sin este mirror el asterisco de "sin guardar"
  en la pestaña tardaría un frame/click de más en aparecer.
- **Por qué por `tab_id` + `node_uid` y no un puntero directo al
  `DesignNode`:** `properties_state` vive en `main.cpp`, fuera de
  `EditorTab`, y sobrevive a cambios de tab activo (igual que ya hacía
  antes de esta fase, ver el `if (editor_state.designer_selection)` de
  arriba) -- guardar un puntero crudo al `DesignNode` seleccionado
  se rompería en cuanto ese tab se cierre, o en cuanto
  `ResolveImports`/una recarga reconstruya el árbol. Buscar por id cada
  vez que se confirma un edit es barato (un árbol de UI real no tiene
  miles de nodos) y tolera "la selección ya no existe" con un no-op
  silencioso (mismo espíritu tolerante que el resto de este documento
  -- import roto, ciclo de componentes, etc.), en vez de un puntero
  colgante.
- **Qué queda fuera de esta pasada, a propósito:** solo el *valor* de
  cada `PropertyRow` es editable -- el `id` del nodo (mostrado arriba
  de la tabla, "Id: ..."), el `type`, agregar/quitar propiedades, y
  renombrar una `key` existente no se tocaron. Tampoco los `events`
  (`node.events`, handler-por-nombre) -- la tabla de Properties nunca
  los mostró, ni antes ni ahora. Ninguno de estos era parte de lo que
  se pidió ("Fase 3: Properties editable con write-back"); si hace
  falta después, es la misma mecánica (otro `PropertyEdit`-like struct,
  u otro campo en el mismo).
- **`preview_panel.cpp`**: comentario `PROPERTIES_EDITABLE` actualizado
  para dejar de decir "no existe todavía" (ya no es cierto para el
  Designer) y aclarar que el árbol demo de Preview se queda de
  solo-lectura a propósito (no tiene archivo fuente al que escribir).
  Nada de su lógica cambió -- sigue sin setear `editable`/
  `source_tab_id`/`selected_node_uid`, que es justamente lo que lo deja
  de solo lectura por default.

**Pendiente, no bloqueante:**
1. El límite de layout de 9.8 (altura del call-site sin ajustar al
   contenido resuelto) -- sin tocar, es lo próximo acordado.
2. Cachear el `ComponentResolver` por `EditorTab` (9.8, punto 3) -- sin
   cambios.
3. `id`/`type`/agregar-quitar-props/eventos editables -- ver el punto
   de alcance de arriba, no pedido en esta pasada.
4. `state` sin evaluar/bindear, binding .NET sin migrar -- sin cambios,
   igual que 9.5/9.7/9.8.

### 9.10 Fix del límite de layout -- resolver el árbol completo antes de layoutear (hecho)

**El bug (recordatorio, ver 9.8):** `layout_engine.cpp::EstimateHeight`
decide el alto que reserva cada nodo mirando únicamente
`node.children`. Un `Navbar()` en `doc.root` es un nodo hoja (`type =
"Navbar"`, sin `children` -- la resolución de 9.7/9.8 nunca tocaba
`doc.root`), así que `EstimateHeight` le daba los `kDefaultLeafHeight`
(32px) fijos de cualquier hoja. `designer_canvas.cpp` resolvía
`Navbar()` a su árbol real *después*, solo para dibujar, pero acotado a
ese mismo rect de 32px ya calculado -- un Navbar más alto se recortaba
en vez de empujar a sus siblings hacia abajo.

**La opción elegida (B del análisis previo): resolver TODO el árbol una
sola vez, antes de layoutear** -- en vez de resolver nodo por nodo
mientras se dibuja. Nada de `layout_engine.cpp` cambió: sigue sin saber
nada de imports/resolución, sigue siendo el mismo módulo agnóstico de
antes. Todo el cambio vive en `designer_canvas.cpp`.

**Diseño:**
- `DrawDesignerCanvas`: si hay `resolver` (project_root no vacío) Y
  `doc.imports` no está vacío, arma una copia completa de `doc.root`
  (`resolved_root`) y le corre `resolver->ResolveTree(resolved_root)`
  -- el método ya existente en `component_resolver.h` (sin cambios ahí
  tampoco) que reemplaza recursivamente cada `Componente()` por su
  subárbol real, en el lugar, con manejo de ciclos y nesting ya
  resuelto de 9.7. `ComputeLayout` corre UNA vez sobre ese árbol
  completo ya expandido -- cada sibling después de un `Navbar()` ya
  tiene su `y` calculado contra el alto REAL del componente, no contra
  un placeholder de 32px.
- **Caso común sin costo extra:** si no hay resolver o `doc.imports`
  está vacío (la inmensa mayoría de páginas, sin `Componente()` en
  absoluto), no se copia nada -- `ComputeLayout`/`DrawNode` corren
  directo sobre `doc.root`, exactamente como antes de 9.8. La copia
  completa del árbol solo se paga cuando de verdad hay algo que
  resolver.
- **`real_uids` (nuevo, `std::unordered_set<std::string>`):**
  reemplaza al parámetro `synthetic` que se pasaba a mano por toda la
  recursión en 9.8. Se llena UNA vez (`CollectUids(doc.root, ...)`)
  con los `node_uid` reales de `doc.root`, antes de copiar/resolver.
  `ComponentResolver::ResolveTree` preserva el `node_uid` de cualquier
  nodo que NO reemplaza (solo los call-sites resueltos reciben uids
  frescos, ver `RegenerateUidsRecursive` -- sin cambios ahí), así que
  "¿este uid está en `real_uids`?" es exactamente "¿este nodo es real
  (editable/droppable) o vino de un componente importado?". Con
  `real_uids` vacío (nada se resolvió este frame) se trata todo como
  real -- mismo comportamiento de siempre.
- **`DrawNode` se simplifica:** ya no recibe `resolver` ni hace el
  chequeo `IsComponentCall`/`GetComponentNode` nodo-por-nodo dentro de
  la recursión (esa lógica quedó centralizada una sola vez, dentro de
  `ComponentResolver::ResolveTree`, en vez de duplicada entre
  `layout_engine`/`designer_canvas` -- mismo principio de "no
  divergir gramática/lógica en dos lugares" que ya se venía siguiendo
  desde la unificación del parser en 9.4). `synthetic` ahora se deriva
  de `real_uids` en vez de threadearse como parámetro booleano por
  cada llamada recursiva.
- **`HandleDropTarget` reescrito:** ya no muta el nodo que recibe
  directamente -- ese nodo puede vivir dentro de `resolved_root` (la
  copia descartable de este frame, reconstruida desde cero en el
  próximo). Ahora busca el nodo real vía
  `design::FindNodeByUid(doc.root, node.node_uid)` (mismo uid
  preservado que arriba) y muta ESE. Esto también unifica el camino:
  cuando no hubo resolución (`root_to_draw == &doc.root`), el nodo
  pasado a `HandleDropTarget` YA es el nodo real -- el lookup por uid
  simplemente se encuentra a sí mismo, mismo código para ambos casos.
- **Properties (Fase 3, 9.9) no necesitó ningún cambio** -- el
  write-back ya buscaba por `tab_id` + `node_uid` vía `FindNodeByUid`
  en `main.cpp`, exactamente el mismo mecanismo que ahora también usa
  `HandleDropTarget`. La arquitectura queda consistente: `resolved_root`
  (o `doc.root` directo cuando no hace falta resolver) es solo el
  "view model" para dibujar/layoutear; toda mutación (drop, edit de
  Properties) pasa siempre por `FindNodeByUid(doc.root, uid)` como
  única fuente de verdad.
- `designer_canvas.h` actualizado: la sección "Known limitation" de 9.8
  se reemplazó por la descripción de este fix.

**Qué NO cambió:** `layout_engine.h/.cpp` (el fix no necesitó tocar el
motor de layout en absoluto -- la opción A del análisis, hacerlo
resolver-aware, hubiera roto la separación de módulos documentada en su
header; se descartó a propósito), `component_resolver.h/.cpp` (todo lo
que necesitaba, `ResolveTree`/`RegenerateUidsRecursive`, ya existía
desde 9.7), `properties_panel.*`/`main.cpp`'s write-back de 9.9,
`editor_panel.cpp` (mismo call site, misma firma de
`DrawDesignerCanvas`).

**Pendiente, no bloqueante:**
1. Cachear el `ComponentResolver` (y ahora también `resolved_root`) por
   `EditorTab` en vez de reconstruirlos cada frame -- 9.8 punto 3, sin
   cambios, sigue sin evidencia de que haga falta.
2. `id`/`type`/agregar-quitar-props/eventos editables en Properties --
   9.9, sin cambios.
3. `state` sin evaluar/bindear, binding .NET sin migrar -- sin cambios.

### 9.11 Fase 4 -- Mover/reordenar nodos ya colocados por drag (implementado)

**Lo que hace:** arrastrar un nodo YA puesto en el canvas (no desde el
Toolbox) para moverlo a otro contenedor, o para reordenarlo entre sus
hermanos actuales -- el hueco que 5.6 punto 5 y 6 (Fases propuestas)
dejaban explícitamente pendiente desde Fase 2.

**Diseño (mismo patrón "un solo `BeginDragDropTarget`, varios
`AcceptDragDropPayload`" que ya usaba `HandleDropTarget` para el
Toolbox, ahora extendido en vez de duplicado):**

- **`design_document.h/.cpp` (nuevo):**
  - `FindParentOfUid(root, uid)` -- análogo a `FindNodeByUid` ya
    existente, pero devuelve el PADRE del nodo con ese uid (o
    `nullptr` si el uid es el del root, o si no existe).
  - `NodeContainsUid(node, uid)` -- ¿`uid` es `node` mismo o algo en su
    subárbol? Usado para rechazar el caso "soltar un contenedor sobre
    uno de sus propios hijos" (haría el árbol inconsistente).
  - `enum class DropZone { kBefore, kInto, kAfter }`.
  - `MoveNode(doc, moved_uid, target_uid, zone)` -- la cirugía real.
    Valida (self-drop, mover el root, uid no encontrado, ciclo) y
    devuelve `false` sin tocar `doc` si algo de eso falla. Si es
    válido: copia el subárbol movido a una variable local ANTES de
    borrar nada (evita punteros colgantes -- `std::vector::erase`
    mueve/reasigna los elementos siguientes al borrado, así que
    cualquier puntero crudo a un hermano posterior quedaría apuntando
    a datos incorrectos), borra el original de los `children` de su
    padre viejo, y busca TODO lo demás (target, padre del target)
    de nuevo desde cero (`FindNodeByUid`/`FindParentOfUid` contra
    `doc.root`) después de ese borrado -- nunca reutiliza un puntero
    tomado antes de mutar un vector. Setea `doc.dirty = true` en éxito.
- **`designer_canvas.h` (nuevo):** `kNodeMoveDragDropId` --
  `"AVAUI_NODE_MOVE"`, mismo esquema que `kToolboxDragDropId` de
  `toolbox_panel.h` (payload = `node_uid` del nodo arrastrado,
  NUL-terminated) pero declarado acá porque tanto el drag source como
  todos los drop targets de este payload viven enteros dentro de
  `designer_canvas.cpp` -- no hace falta compartirlo con otro panel
  como sí pasa con el del Toolbox.
- **`designer_canvas.cpp`:**
  - `DrawNode`: el mismo `InvisibleButton` que ya daba click+selección
    ahora también es un `BeginDragDropSource` (patrón estándar de
    ImGui, "este ítem es además origen de drag") cuando el nodo es
    real (no `synthetic`) y no es el root (`doc.root.node_uid` -- el
    root no tiene padre del que sacarlo). Un click simple sin arrastrar
    sigue seleccionando exactamente igual que antes.
  - `HandleDropTarget` reescrita: ahora recibe también `is_container`,
    `p0`, `p1` (el rect ya dibujado/inset del nodo). Dentro del MISMO
    `BeginDragDropTarget` acepta:
    - `kToolboxDragDropId` (sin cambios de comportamiento, sigue
      gateado a `is_container`).
    - `kNodeMoveDragDropId` (nuevo): en qué franja del rect cayó el
      mouse decide la `DropZone` -- franja superior/inferior (25% cada
      una, `kEdgeBandFrac`) siempre es `kBefore`/`kAfter` (hermano,
      antes/después); la franja central (50%) es `kInto` (adentro,
      como hijo nuevo) PERO solo si el target es contenedor -- en un
      leaf la franja central cae a `kAfter` en vez de no hacer nada
      (un leaf no tiene dónde meter un hijo). Esto también significa
      que ahora TODO nodo real (no solo los contenedores) es un drop
      target válido, para poder reordenar alrededor de un leaf.
- **Qué NO cambió:** `layout_engine.*` (el layout en sí no sabe nada de
  drag/reorder, sigue calculando rects igual), `component_resolver.*`
  (nada de esto toca nodos sintéticos -- ver el guard `!synthetic` que
  ya existía, ahora también gatea el drag source), `properties_panel.*`
  ni el write-back de 9.9 (mover un nodo no cambia sus props/id/tipo).

**Qué queda fuera a propósito:** el hit-testing de overlap
padre/hijo (ver el comentario ya existente sobre "gana el último
agregado") no se tocó -- el drag source/drop target nuevos heredan
ese mismo comportamiento (arrastrar sobre el área de un hijo apunta al
hijo, no al padre), consistente con cómo ya funcionaba el click, pero
sin stress-test específico para layouts "stack" muy anidados/solapados.

**Pendiente, no bloqueante:**
1. Cachear el `ComponentResolver`/`resolved_root` -- 9.8 punto 3, sin
   cambios.
2. `id`/`type`/agregar-quitar-props/eventos editables -- 9.9, sin
   cambios.
3. `state` sin evaluar/bindear, binding .NET sin migrar -- sin cambios.
4. Feedback visual mientras se arrastra (línea/resaltado indicando en
   qué franja/zona va a caer el drop) -- hoy la mecánica funciona pero
   es "a ciegas" hasta soltar; no pedido en esta pasada.

### 9.12 Properties: id/type/agregar-quitar-props/eventos editables + feedback visual del drag (implementado)

Dos piezas independientes de la lista de pendientes, hechas juntas en
esta pasada:

**A. Lo que 9.9 había dejado explícitamente afuera del alcance
("Properties: id, type, agregar/quitar props, events"):**

- **`properties_panel.h`:** `PropertyEdit` ahora tiene un campo
  `kind` (`enum class PropertyEditKind`: `kValue` -- el de siempre --,
  `kId`, `kType`, `kAddProperty`, `kRemoveProperty`, `kEvent`,
  `kRemoveEvent`). `PropertiesState` ahora también carga
  `events` (mirror de `DesignNode::events`, poblado únicamente por
  `designer_canvas.cpp::ToPropertiesState`; Preview lo deja vacío).
- **`properties_panel.cpp`:** reescrito.
  - `Type` es ahora un Combo (editable) poblado desde
    `design::GetComponentCatalog()` -- elegir un tipo distinto SOLO
    cambia `DesignNode::type`; a propósito NO re-siembra
    `default_properties` (perdería props editadas a mano), así que
    props/events que ya no aplican al nuevo tipo simplemente quedan
    ahí sin usarse.
  - `Id` es ahora un `InputText` editable (mismo patrón
    deactivated-after-edit que ya usaban los valores de properties).
  - Extraída `DrawEditableRowTable` (nueva, en el anónimo namespace) --
    una tabla key/value genérica con botón "x" por fila y una fila
    "agregar" al final (key nueva + botón "+", deshabilitado si la key
    está vacía o ya existe). Se llama una vez para `properties` y otra
    para `events` -- misma función, distintos `PropertyEditKind` para
    poder distinguirlos en el write-back.
  - Cada tabla mirrorea localmente el add/remove sobre su propio
    `std::vector<PropertyRow>` (además de emitir el `PropertyEdit`)
    para que no tarde un frame extra en reflejarse -- mismo espíritu
    que ya justificaba mirrorear `tab.design.dirty`/`tab.dirty` en
    `main.cpp`.
- **`main.cpp`:** el `switch (edit->kind)` en el write-back de Fase 3
  ahora cubre los 7 casos -- `kValue` es exactamente el código que ya
  existía, los demás son nuevos: `kId`/`kType` asignan directo,
  `kAddProperty`/`kRemoveProperty` agregan o borran de
  `node->properties` (con guard anti-duplicado en el add, redundante
  con el del panel pero barato), `kEvent` es find-or-add sobre
  `node->events` (cubre tanto "cambié el handler" como "agregué un
  evento nuevo" con el mismo código), `kRemoveEvent` borra de
  `node->events`.

**B. Feedback visual del drag (el punto 5 pendiente de 9.11):**

- `designer_canvas.cpp`: la franja (`ComputeDropZone`, extraída como
  función compartida) que antes solo se calculaba al soltar ahora
  también se calcula en un `AcceptDragDropPayload(kNodeMoveDragDropId,
  ImGuiDragDropFlags_AcceptPeekOnly)` -- ese peek NO consume el
  payload, así que corre en TODOS los frames que el mouse pasa arrastrando
  sobre un target válido, solo para dibujar (con el mismo
  `ImGui::GetWindowDrawList()` de siempre) una línea arriba/abajo del
  rect (`kBefore`/`kAfter`) o un recuadro resaltado completo (`kInto`).
  El accept real (el que sí consume el payload y llama a
  `design::MoveNode`) sigue siendo el de siempre, más abajo, así que la
  única garantía que hace falta es que ambos usen la MISMA
  `ComputeDropZone` -- lo que se ve mientras se arrastra es exactamente
  lo que va a pasar si se suelta ahí.

**Qué NO se tocó:** `layout_engine.*`, `component_resolver.*`,
`design_document.h/.cpp` (el `MoveNode`/`DropZone` de 9.11 se reutiliza
tal cual, sin cambios), `toolbox_panel.*`.

**Pendiente, no bloqueante (lo que sigue quedando de la lista original):**
1. Compilar 9.11 y 9.12 de verdad (ninguna de las dos pasó por un build
   todavía).
2. Cachear el `ComponentResolver`/`resolved_root` por `EditorTab` --
   9.8 punto 3, sigue sin cambios, sigue sin evidencia de que haga
   falta.
3. `state` sin evaluar/bindear contra la VM (depende de builtins
   `ui.*`, Fase 6) -- el más grande de los que quedan, sin tocar.
4. Binding .NET (`ComponentResolver.cs`) sin migrar a la C API -- fuera
   de alcance de este repo C++, sin cambios.

### 9.13 Fase 5: generación de code-behind al doble-click (implementado)

Lo que quedaba de la lista de pendientes como "el flujo que ya está en
`AGENTS_STUDIO.md` como Workflow Futuro" (sección 6, Fase 5): doble
click en un `Button` del canvas genera su handler `Click` y salta a
verlo en código, igual que VS6.

- **`design_document.h/.cpp` (nuevo):** `EnsureClickHandler(doc, uid)`
  -- toda la mutación del árbol/code-behind vive acá, no en
  `designer_canvas.cpp`, mismo patrón que `MoveNode`. Busca el nodo
  real por `uid` (`nullptr`/`""` si es sintético o el uid no existe --
  no hace nada). Si el nodo no tiene `id`, le asigna uno nuevo
  (`NextAutoId`: `<type><N>` libre en todo el árbol, ej. `button1`,
  estilo VS6 auto-nombrando `Command1`). Si ya tiene un evento `click`
  con handler, lo reutiliza tal cual (nunca lo renombra -- para eso ya
  está `PropertyEditKind::kEvent` en Properties) y sólo le vuelve a
  agregar el stub a `code_behind` si no está (`CodeBehindHasFunc`, un
  `find` de `"func <name>("` -- no vale la pena parsear AvaLang acá
  sólo para esto). Si no tiene `click` todavía, genera
  `<id_sanitizado>_Click`, lo guarda como evento nuevo, y agrega el
  stub:
  ```
  func btnGuardar_Click(sender, e)
      -- TODO: btnGuardar_Click
  end
  ```
  al final de `code_behind` (con línea en blanco antes si ya había
  algo). Marca `doc.dirty = true` en cualquier caso que haya cambiado
  algo.
- **`designer_canvas.h/.cpp`:** `DrawDesignerCanvas` suma un parámetro
  nuevo `std::string* out_generated_handler = nullptr` (se limpia al
  entrar si no es null). `DrawNode` lo recibe también y lo propaga a
  la recursión. Justo después del `IsItemClicked()` de siempre (que
  sigue seleccionando igual, un doble click dispara igual el primer
  click): si el nodo es real (`!synthetic`) y `node.type == "button"`,
  `ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)`
  sobre el mismo `InvisibleButton` de siempre llama a
  `design::EnsureClickHandler(doc, node.node_uid)` y, si devolvió algo,
  lo escribe en `*out_generated_handler`. Acotado a `"button"` nomás
  (no a cualquier control) -- es el único tipo con un evento default
  obvio; el resto queda para una pasada futura en vez de inventarle
  uno.
- **`editor_panel.cpp`:** `JumpToCodeBehindHandler(tab, handler_name)`
  (nueva, namespace anónimo, después de `ToggleTabViewMode` porque la
  reusa) -- si `tab.view_mode == Design` llama a `ToggleTabViewMode`
  (ya hace exactamente el trabajo de serializar `tab.design` a texto y
  reconstruir el índice/autocomplete que hace falta acá, no se
  duplica), después busca la línea de `"func <handler_name>("` en el
  texto ya serializado (`FindLineOf`, un `find` + contar `\n` antes)
  y mueve el cursor a la línea del cuerpo del stub (`SetCursor`) más
  `ScrollToLine` para que quede visible, mismo patrón que ya usaba
  `HighlightError`. El call site de `DrawDesignerCanvas` en
  `DrawEditorPanel` declara un `std::string generated_handler` local,
  lo pasa por dirección, y si volvió no vacío llama a
  `JumpToCodeBehindHandler` -- todo dentro del mismo bloque `if
  (selected)` que ya dibujaba el canvas, no hizo falta tocar
  `EditorState`.

**Qué NO se tocó:** `properties_panel.*` (el evento `click` generado
se ve ahí como cualquier otro, editable/removible con lo que ya hacía
9.12), `layout_engine.*`, `component_resolver.*`, `avaui_text.*` (el
nuevo `code_behind`/evento se serializan con el `WriteAvauiText` que
ya existía, sin cambios).

**Pendiente, no bloqueante:**
1. Sólo cubre `type == "button"` -- extender a otros controles
   interactivos (checkbox/textbox/...) cuando haya un evento default
   obvio para cada uno.
2. Cachear el `ComponentResolver`/`resolved_root` -- 9.8 punto 3, sigue
   sin cambios.
3. ~~`state` sin evaluar/bindear contra la VM (Fase 6) -- sigue siendo
   el pendiente más grande, sin tocar.~~ **Primera pasada hecha en
   9.14, ver abajo.**
4. Binding .NET sin migrar -- sin cambios, fuera de alcance.

### 9.14 Fase 6, primera pasada: `state` evaluado contra una VM real (implementado)

Se pidió explícitamente Fase 6 ("state/VM binding, `ui.*` builtins") --
el pendiente marcado repetidas veces como "el más grande que queda"
(9.2 punto 2, 9.4, 9.5 punto 4, 9.13 pendiente 4). Antes de picar
código se revisó qué bloquea realmente qué, porque el doc los venía
acoplando: **no hace falta `RegisterUIBuiltins`** para esto -- la
sección 2 punto 3 ya lo decía ("el Designer puede evaluar expresiones
sueltas... sin esperar a que `ui.*` esté registrado") y la C API
(`avalang.h`) ya expone `ava_compile`/`ava_run`/`ava_set_global`/
`ava_get_global` de sobra para evaluar una expresión contra un puñado
de globals. `core/src/ui/builtins.cpp::RegisterUIBuiltins` **sigue
siendo un no-op** -- eso es la Fase 6 completa del plan ("un script
declara su propia UI con sintaxis nativa del VM"), una pieza bastante
más grande e inventada sin evidencia de cuál debería ser su superficie
de API; lo que sí tiene un alcance concreto y ya escrito en la sección
2 es evaluar `state` y props contra la VM, así que se hizo eso.

- **`studio/src/design/state_eval.h` / `.cpp` (nuevo):**
  - `BuildStateVM(doc)` -- crea una `AvaVM` y bindea cada entrada de
    `doc.initial_state` como global, en orden de archivo. El texto de
    cada valor (ya sin comillas, ver el comment de `avaui_text.h`) se
    infiere como bool/number/string con el mismo criterio
    (`LooksNumeric`) que `core/src/ui/avaui_text.cpp` usa para el
    camino inverso (escribir) -- espejado a mano en vez de exportado
    del core porque este lado solo tiene la C API (`ava_value_t`), no
    el `Value` interno que esa función usa.
  - `EvalPropertyExpr(vm, raw_value)` -- evalúa `raw_value` como
    expresión AvaLang (`__avaui_eval__ = (<raw_value>)`, compilar +
    correr + leer el global de vuelta) y devuelve el resultado
    formateado a texto. Fallback a `raw_value` tal cual si falla
    compilar/correr, o si el resultado es `Nil`.
  - La pieza que resuelve la ambigüedad que `avaui_text.h` ya
    documentaba como limitación (un string literal simple como
    `"Guardar"` y un identificador desnudo como `message` llegan
    indistinguibles a `PropertyRow::value`, ambos sin comillas): se
    aprovecha que `VM::GetGlobal` (`core/src/vm/vm.cpp`) devuelve
    `Nil` para cualquier nombre no definido en vez de tirar error --
    así que evaluar `"Guardar"` como identificador compila bien, corre
    bien, y da `Nil` (porque no hay ningún global `Guardar`), lo que
    cae al fallback y muestra `"Guardar"` tal cual -- exactamente lo
    que ya se mostraba antes de esta pasada. Evaluar `message` (una
    var real de `state`) en cambio sí resuelve a su valor. No hace
    falta tocar el parser para esto.
  - `GetDisplayPropertyKey(type)` -- qué prop de cada tipo es "lo que
    se ve": `value` (text/textbox), `text` (button/link), `label`
    (checkbox/radiobutton), `""` para el resto (containers, image,
    spacer, divider -- ninguno tiene un texto obvio para evaluar y
    mostrar así). Vive acá y no en `component_catalog.h` porque es
    específico de esta pasada, no parte del catálogo estático.
- **`designer_canvas.cpp`:** `DrawDesignerCanvas` arma una
  `state_vm` con `BuildStateVM(doc)` una vez por llamada (una VM por
  frame para todo el árbol, no una por nodo), la destruye al final con
  `ava_vm_destroy`. `DrawNode` suma el parámetro `AvaVM* state_vm` (se
  lo pasa a sí mismo en la recursión, igual que `out_generated_handler`
  en 9.13) y, justo debajo del label de siempre (`type (id)`), dibuja
  un segundo renglón con `EvalPropertyExpr(state_vm, valor_de_la_
  display_prop)` cuando `GetDisplayPropertyKey(node.type)` no es vacío
  -- esto es, en los hechos, lo primero que el canvas dibuja de
  contenido real de un control (antes de esta pasada no había ningún
  texto de prop en pantalla, solo el rectángulo wireframe + type/id).
  Corre igual para nodos sintéticos (resueltos de un import) -- a
  diferencia de edición/drag&drop, mostrar el valor evaluado no
  necesita un `DesignNode` real en `doc.root`.
- **`studio/CMakeLists.txt`:** agregado `src/design/state_eval.cpp`.

**Qué NO se tocó:** `core/src/ui/builtins.cpp` (`RegisterUIBuiltins`
sigue siendo el no-op documentado -- eso es la Fase 6 completa, no
esta pasada), `avaui_text.*` (ni core ni Studio -- el parser sigue sin
distinguir literal de identificador, esta pasada lo esquiva en vez de
arreglarlo), `properties_panel.*` (sigue mostrando/editando el texto
FUENTE de la prop, no el evaluado -- a propósito, es lo que se guarda),
`design_document.*`, `layout_engine.*`, `component_resolver.*`,
`component_catalog.*`.

**Limitaciones conocidas de esta primera pasada (a propósito, no
bugs):**
1. **Performance sin optimizar.** Una VM nueva por frame por
   `DrawDesignerCanvas`, y una compilación (`ava_compile`/`ava_run`)
   por cada display-prop visible, cada frame -- nada cacheado. Mismo
   "pendiente, no bloqueante" que ya viene arrastrando el
   `ComponentResolver` (9.8 punto 3); aceptable para un canvas de
   edición (no un loop de 60fps con miles de nodos), pero una pasada
   futura debería reconstruir la VM solo cuando `doc.dirty` cambie y/o
   cachear el resultado evaluado por nodo+prop en vez de re-evaluar
   siempre.
2. **`state` de un import resuelto no se bindea.** `BuildStateVM` usa
   `doc.initial_state` (el documento que se está editando) -- un
   `Navbar()` resuelto por `ComponentResolver` dibuja con la MISMA
   `state_vm` del documento padre, no con el `state` propio de
   `navbar.avaui` (que ni siquiera se carga hoy -- ver 9.2 punto 1,
   `ComponentResolver` no lee `state`/`methods` del archivo
   importado). Fuera de alcance de esta pasada.
3. **Ambigüedad literal/identificador resuelta con un truco, no de
   raíz.** Ver arriba (`Nil` fallback) -- funciona para el caso común
   pero significa que una expresión que legítimamente evalúa a `Nil` a
   propósito se mostraría con su texto fuente en vez de "nil". Un
   arreglo de raíz necesitaría que el parser preserve si el valor
   original estaba entre comillas o no (campo nuevo en
   `PropertyRow`/`Component::properties`, cambio de API más grande) --
   no se tocó en esta pasada.
4. **`RegisterUIBuiltins` sigue sin registrar nada.** La Fase 6
   completa del plan ("Preview real ejecutando el árbol" desde un
   script con sintaxis nativa) sigue entera, sin tocar.

**Pendiente, no bloqueante:**
1. Cachear `state_vm`/evaluación -- limitación 1 de arriba.
2. `state`/`methods` de un import resuelto -- limitación 2, depende de
   9.2 punto 1 (`ComponentResolver` no lee esas secciones todavía).
3. `RegisterUIBuiltins` real (Fase 6 completa) -- limitación 4, el
   pendiente más grande que queda ahora.
4. Extender Fase 5 a otros controles -- 9.13 pendiente 2, sin cambios.
5. Binding .NET sin migrar -- sin cambios, fuera de alcance.

### 9.15 Fase 6, cacheo de state_vm/evaluación (implementado)

Pendiente 1 de 9.14: reconstruir el `state_vm` solo cuando haga falta en
vez de una VM nueva + una recompilación por display-prop en CADA frame.

- **`designer_canvas.cpp`:** nuevo `DesignerVmCacheEntry` (struct
  file-local) y `g_designer_vm_cache` (`unordered_map<int,
  DesignerVmCacheEntry>`, keyeado por `EditorTab::id`, nunca reusado
  dentro de un proceso). Cada entry tiene `AvaVM* vm`, `bool
  last_dirty`, y `eval_cache` (`unordered_map<string,string>`, key =
  `node_uid + '\x1f' + raw_value`).
- `DrawDesignerCanvas`: para `tab_id >= 0`, busca/crea la entry;
  reconstruye el VM (y limpia `eval_cache`) solo si `entry.vm ==
  nullptr` o `entry.last_dirty != doc.dirty` (transición, no "está en
  true"). Ya no destruye el VM al final de la llamada -- queda vivo en
  el cache. `tab_id < 0` (sin caller real hoy) mantiene el
  comportamiento viejo build+destroy por llamada, ya que `-1` no es key
  segura (colisionaría entre callers).
- `DrawNode` suma el parámetro `eval_cache` (nullable, se threadea en
  la recursión igual que `state_vm`/`out_generated_handler`). Antes de
  llamar `EvalPropertyExpr` busca `node_uid + '\x1f' + prop.value` en
  el cache; hit devuelve el string ya evaluado, miss evalúa y guarda.
  Editar el texto de un display-prop cambia la key automáticamente
  (nueva `raw_value`), así que se re-evalúa solo ese nodo+prop sin
  necesitar que `doc.dirty` transicione -- el cache de evaluación no
  depende de `last_dirty` para eso, solo el rebuild del VM en sí.
- **`InvalidateDesignerVmCache(int tab_id)` (nueva, declarada en
  `designer_canvas.h`):** libera `entry.vm` (`ava_vm_destroy`) y borra
  la entry del map. Se llama una vez desde
  `editor_panel.cpp::CloseTabNow`, justo antes de
  `state.tabs.erase(...)` -- único lugar donde un tab sale de verdad de
  `EditorState::tabs`. Sin esto el VM cacheado de cada `.avaui` cerrado
  quedaba leaked (antes de esta pasada no pasaba porque se destruía
  cada frame, ahora que vive más que un frame sí hace falta liberarlo
  explícitamente en algún lado).

**Qué NO se tocó:** `state_eval.h`/`.cpp` (`BuildStateVM`/
`EvalPropertyExpr`/`GetDisplayPropertyKey` siguen igual, solo cambió
CUÁNDO se llaman, no cómo), `component_resolver.*` (su propio cacheo,
9.8 punto 3, sigue sin tocar -- pendiente separado), `design_document.*`,
`properties_panel.*`.

**Limitación conocida (a propósito, documentada en el código):**
`last_dirty` es una aproximación barata, no un dependency-check real
sobre `doc.initial_state`: una segunda edición hecha ANTES de guardar
la primera (`doc.dirty` ya estaba en `true`) no dispara otra
reconstrucción del VM, porque no hay transición false->true. Esto es
inofensivo hoy porque `initial_state` no tiene ninguna UI de edición en
el Designer (solo se carga/serializa, ver `design_document.h`) -- es lo
único bindeado DENTRO del VM. Si en el futuro se agrega edición de
`state` desde Properties, esta aproximación dejaría de alcanzar y
haría falta un dependency-check real (ej. comparar
`doc.initial_state` contra una copia guardada en la entry) en vez de
mirar solo `doc.dirty`.

**Pendiente, no bloqueante:**
1. Cachear `ComponentResolver`/`resolved_root` -- 9.8 punto 3, sigue
   sin cambios, mismo patrón de cache-por-tab_id que este ahora podría
   reusar.
2. `state`/`methods` de un import resuelto -- 9.14 limitación 2, sigue
   sin tocar.
3. `RegisterUIBuiltins` real (Fase 6 completa) -- 9.14 limitación 4, el
   pendiente más grande que queda.
4. Extender Fase 5 a otros controles -- 9.13 pendiente 2, sin cambios.
5. Binding .NET sin migrar -- sin cambios, fuera de alcance.

### 9.16 Fase 6 + 9.8 punto 3, cacheo del ComponentResolver/resolved_root (implementado)

Pendiente 1 de 9.15 (y arrastrado desde 9.8 punto 3): el resolver se
reconstruía, y el árbol se re-resolvía (deep copy + `ResolveTree`)
desde cero, en CADA frame -- lo mismo que 9.15 ya había resuelto para
el `state_vm`, sin tocar todavía.

- **`designer_canvas.cpp`:** `DesignerVmCacheEntry` (la misma struct de
  9.15, sin renombrar) suma `cached_project_root`, `resolver`
  (`std::optional<design::ComponentResolver>`), `resolved_root`
  (`std::optional<design::DesignNode>`) y `real_uids`. `resolver`/
  `resolved_root` se reconstruyen JUNTO con el `state_vm`, bajo la
  misma condición de invalidación que ya usaba 9.15 (`entry.vm ==
  nullptr || entry.last_dirty != doc.dirty`), sumando además
  `entry.cached_project_root != project_root` (por si `project_root`
  cambia para el mismo `tab_id`, caso raro pero barato de cubrir).
- `DrawDesignerCanvas` ya no arma un `ComponentResolver` local ni
  resuelve el árbol por llamada para `tab_id >= 0` -- usa
  `entry.resolved_root` (o `doc.root` si no hay imports) como
  `root_to_draw` directamente. El path `tab_id < 0` (sin caller real
  hoy) queda exactamente igual que antes de esta pasada: resolver y
  árbol resuelto locales, reconstruidos en cada llamada.
- `InvalidateDesignerVmCache` (sin cambios de firma) ya cubre esta
  mitad del cache gratis -- al hacer `g_designer_vm_cache.erase(it)`
  se destruyen `resolver`/`resolved_root` como cualquier miembro
  `std::optional`, no necesitan liberación manual como el `AvaVM*`.

**Qué NO se tocó:** `component_resolver.h`/`.cpp` (la clase en sí sigue
igual, solo cambió cuándo se construye/usa una instancia),
`state_eval.*`, `design_document.*`, `properties_panel.*`.

**Limitación conocida (heredada de `component_resolver.h`, no nueva):**
la clase ya documentaba que "nada acá vigila el filesystem", así que un
resolver no debería vivir más allá de un solo pase. Cachearlo entre
frames invierte esa premisa a propósito: si un componente importado
(ej. `navbar.avaui`) se edita desde OTRO tab, este tab ya no lo
refleja hasta que su propio `doc.dirty` transicione (una edición local,
o reabrir el archivo). Antes de esta pasada se releía del disco cada
frame, así que ese cambio cruzado aparecía al instante -- ahora queda
stale hasta el próximo rebuild de este tab. Aceptado como trade-off,
mismo criterio "no bloqueante" que el resto de la lista.

**Pendiente, no bloqueante:**
1. `state`/`methods` de un import resuelto -- 9.14 limitación 2, sigue
   sin tocar.
2. `RegisterUIBuiltins` real (Fase 6 completa) -- 9.14 limitación 4, el
   pendiente más grande que queda.
3. Extender Fase 5 a otros controles -- 9.13 pendiente 2, sin cambios.
4. Binding .NET sin migrar -- sin cambios, fuera de alcance.

### 9.17 Fase 6, `RegisterUIBuiltins` — primera pasada (`ui.log`)

`RegisterUIBuiltins` dejó de ser no-op. Antes de escribir nada se
revisó este repo y `avalang-dotnet` completo buscando algún uso real
de `ui.algo(...)` — no hay ninguno; `AvaLang.UI/TODO_FLOW.md` (el
modelo de referencia que sí corre) solo usa `state` como globals y
`print()`. Como la superficie de API no está definida en ningún lado,
en vez de inventar `ui.navigate`/`ui.alert`/setters imperativos sin
evidencia de firma, se registró solo lo que no requiere inventar un
mecanismo de callback host↔VM nuevo:

- `core/src/ui/builtins.cpp`: `ui` se registra como global (un Dict,
  no un native suelto, para que `ui.log(...)` compile vía
  `GETGLOBAL "ui"` + `GETATTR "log"` — mismo mecanismo que ya resuelve
  `str.upper()`/`dict.keys()`). Única entrada: `ui.log(...)`, igual
  que `print()` pero con prefijo `"[ui] "`.
- `public/src/c_api.cpp`: `ava_vm_create()` llama
  `ava::ui::RegisterUIBuiltins(vm)` — `ui.*` queda disponible en toda
  VM, incluida la del Designer.

**Pendiente, deliberadamente no resuelto (falta definir el mecanismo,
no solo la firma):**
1. `ui.navigate(route)` — el routing file-based vive en el host
   .NET/Studio, no en el core.
2. `ui.alert(msg)` / cualquier UI nativa — necesita un callback
   host→VM nuevo que hoy no existe (similar a `SetPrintSink` pero para
   otro evento).
3. Setter imperativo sobre el árbol (`ui.setProp(...)`) — no hace
   falta hoy: mutar `state` por asignación directa ya cubre el caso de
   uso (mismo patrón que `AvaLang.UI`: `count = count + 1`).
4. Fase 6 completa ("Preview real ejecutando el árbol" desde un click)
   — resuelto parcialmente en 9.18.

### 9.18 Fase 6, click ejecuta el `methods` handler real (Ctrl+Click)

Pendiente 4 de 9.17: nada corría un handler de `methods` contra
estado real — `state_vm` solo tenía `state` bindeado, ninguna función
de `doc.code_behind`.

- `studio/src/design/state_eval.h`/`.cpp`: dos funciones nuevas.
  - `BindCodeBehind(vm, doc)` — compila y corre `doc.code_behind` (el
    bloque `methods` completo) contra `vm` una vez, así cada
    `func nombre(...) ... end` queda registrado como global invocable.
    No-op silencioso si no compila (código a medio escribir).
  - `InvokeHandler(vm, handler_name)` — llama `handler_name()` (cero
    argumentos) contra `vm`. Devuelve `true` si corrió sin error.
- `studio/src/panels/designer_canvas.cpp`:
  - `BindCodeBehind` se llama junto a `BuildStateVM`, bajo la misma
    condición de rebuild de la VM cacheada — no se re-bindea en cada
    click.
  - `DrawNode`: si el click es con Ctrl apretado y el nodo (real, no
    sintético) tiene un evento `"click"` con handler, lo corre contra
    `state_vm` y limpia `eval_cache` para que el canvas muestre el
    `state` nuevo desde el próximo frame.
  - Ctrl+Click en vez de un modo `Run`/F5 dedicado para no tocar
    `editor_panel.cpp`/`main.cpp` en esta pasada — click normal sigue
    siendo solo selección.

**Limitaciones conocidas, no bloqueantes:**
1. Nodo sintético (import resuelto) no puede correr su handler — su
   `state`/`methods` propios ni se cargan hoy (9.14 limitación 2).
2. Un error de runtime en el handler no tiene dónde mostrarse (no hay
   consola de Preview).
3. Ctrl+Click es un atajo para probar un handler mientras se edita, NO
   un modo Preview real — sin indicación visual de "estás corriendo",
   sin forma de resetear `state` sin cerrar/reabrir el tab.

**Pendiente, no bloqueante:**
1. Modo Preview real dedicado (indicación visual, reset de `state`,
   consola de errores) — limitaciones 2-3 de arriba.
2. `ui.navigate`/`ui.alert`/setters imperativos — 9.17, sigue
   bloqueado en definir el mecanismo de callback host↔VM.
3. `state`/`methods` de un import resuelto — 9.14 limitación 2.
4. Extender Fase 5 a otros controles — 9.13 pendiente 2.
5. Binding .NET sin migrar — fuera de alcance.

### 9.19 Fase 6 completa — mecanismo `ui.alert`/`ui.navigate` + modo Preview dedicado

Cierra los dos pendientes que quedaban "Parcial" en la tabla: 9.17
pendiente 4 (`ui.navigate`/`ui.alert` bloqueados por falta de
mecanismo) y 9.18 pendiente 1 (modo Preview real: indicación visual,
reset de `state`, consola de errores).

**Parte A — mecanismo host↔VM (lo que bloqueaba `ui.alert`/`ui.navigate`):**

- `core/src/vm/vm.h`/`.cpp`: `VM::AlertSink`/`VM::NavigateSink`, copia
  exacta del patrón de `PrintSink` (`SetAlertSink`/`Alert()`,
  `SetNavigateSink`/`Navigate()`). Sin sink instalado, ambos degradan
  a `Print()` con un prefijo (`"[ui:alert] "` / `"[ui:navigate] "`) en
  vez de perder el mensaje silenciosamente.
- `public/include/avalang.h`/`public/src/c_api.cpp`:
  `ava_vm_set_alert_callback` / `ava_vm_set_navigate_callback`, mismo
  mecanismo que `ava_vm_set_print_callback`.
- `core/src/ui/builtins.cpp`: `ui.alert(msg)` y `ui.navigate(route)`
  agregados al dict `ui` junto a `ui.log`. `ui.navigate` pasa `route`
  como string opaco -- el core sigue sin saber nada de rutas (el
  routing file-based sigue viviendo en `AvaLang.UI/Routing` del lado
  .NET, o en un futuro router de Studio); es el host quien decide qué
  hacer con el evento.

**Parte B — modo Preview dedicado en el Designer (studio):**

- `studio/src/design/state_eval.h`/`.cpp`: `InvokeHandler` ahora
  acepta `std::string* out_error = nullptr` -- captura el mensaje de
  compile/run error en vez de descartarlo, sin romper el caller
  existente (Ctrl+Click en modo edición, que no lo pasa).
- `studio/src/panels/designer_canvas.h`/`.cpp`: nuevo `PreviewLogLine`
  (`Kind::Log/Alert/Navigate/Error`) + `SetDesignerPreviewActive`,
  `IsDesignerPreviewActive`, `ResetDesignerPreviewState`,
  `GetDesignerPreviewLog`, `ClearDesignerPreviewLog`. Todo cacheado
  por `tab_id` en la misma `DesignerVmCacheEntry` que ya tenía
  `state_vm` (9.15/9.16) -- `preview_active` sobrevive un rebuild de
  la VM (prender/apagar Preview no gana ni pierde estado), `preview_log`
  se vacía en cada rebuild (una VM nueva no puede seguir produciendo
  un log válido para la vieja).
  - Al (re)construir `entry.vm` se instalan los tres sinks (print/
    alert/navigate) apuntando a `&entry` -- captura cualquier
    `ui.log`/`ui.alert`/`ui.navigate` que corra un handler contra esa
    VM, sea por Ctrl+Click en edición o por un click en Preview.
  - `DrawNode`: el click ahora corre el handler `click` si
    `preview_active || Ctrl` (antes: solo `Ctrl`). Un error de
    compile/run del handler se agrega a `preview_log` como
    `Kind::Error` -- resuelve "un error de runtime en el handler no
    tiene dónde mostrarse" (9.18 limitación 2).
  - `DrawDesignerCanvas`: dibuja un borde + banner naranja "PREVIEW"
    sobre el canvas mientras `preview_active` -- resuelve "sin
    indicación visual de que estás corriendo" (9.18 limitación 3).
- `studio/src/panels/editor_panel.cpp`: barra sobre el canvas en
  Design view con botón `Preview: ON/OFF` + `Reset` (deshabilitado sin
  Preview activo) + texto de ayuda según el modo. Debajo del canvas,
  una consola scrolleable (solo ocupa espacio si hay algo que
  mostrar) con las líneas de `preview_log`, coloreadas por tipo
  (alert=warning, navigate=info, error=error, log=texto secundario),
  con auto-scroll al fondo (mismo criterio que
  `output_panel.cpp`) y botón "Limpiar consola".

**Decisión de diseño -- por qué Ctrl+Click se mantiene además de Preview:**
Preview (click normal) y Ctrl+Click (en cualquier modo) hacen lo
mismo por debajo (`InvokeHandler` contra el mismo `state_vm`) -- Ctrl+
Click sigue siendo útil para probar un handler puntual sin salir de
edición ni prender el banner. No se unificaron en un solo mecanismo
porque son dos flujos de UX distintos (edición rápida vs. modo
dedicado), no dos implementaciones distintas.

**Limitaciones conocidas, no bloqueantes:**
1. Nodo sintético (import resuelto) sigue sin poder correr su
   handler -- su `state`/`methods` propios ni se cargan hoy (9.14
   limitación 2, sin cambios).
2. Ningún host de este repo instala todavía un alert/navigate sink
   fuera del Designer -- el "Run" genérico de `EngineBridge`
   (`studio/src/engine/engine_bridge.cpp`, usado por F5 sobre código
   plano) sigue sin bindearlos; solo hacía falta para Fase 6 del
   Designer, no para el runner genérico.
3. `ui.setProp(...)` (setter imperativo sobre el árbol) sigue sin
   hacer falta -- el modelo de mutar `state` por asignación directa
   (`count = count + 1`) ya cubre el caso de uso, mismo criterio que
   9.17 documentaba.
4. Extender Fase 5 a otros controles (checkbox/textbox) -- 9.13
   pendiente 2, sin cambios.
5. `state`/`methods` de un import resuelto -- 9.14 limitación 2, sin
   cambios.
6. Binding .NET sin migrar -- sin cambios, fuera de alcance.
