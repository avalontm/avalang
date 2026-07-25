# Plan de Implementación — Vista "Design" estilo VS6 para Ava Studio

Documento de planificación. No se tocó código todavía (salvo el fix ya
aplicado en `output_panel.h/.cpp`). Objetivo: que abrir un archivo de
UI (`.avaui`) muestre un lienzo visual con drag&drop de controles en
vez del editor de texto, con un botón/atajo (F7) para alternar a ver el
código, igual que los `.frm` de VS6.

---

## 0. Nombre de extensión

Propongo **`.avaui`** (no `.avax`) porque:
- Ya existe la palabra "AvaUI" en todo el proyecto (`docs/architecture/AVAUI_FRAMEWORK.md`,
  `bindings/csharp/AvaLang.UI`) — mantiene consistencia de nombre.
- `.avax` no se usa en ningún lado todavía y no dice nada por sí solo.

Si preferís `.avax` es un cambio de una sola constante, no afecta el
resto del plan. Uso `.avaui` en todo el documento; avisame si lo cambio.

---

## 0.1 ACTUALIZACIÓN — hay un prototipo .NET que ya resuelve esto (leer antes de la sección 3)

Reviví esto porque me pasaste `avalang-dotnet.zip`: un binding C# (`AvaLang.UI`,
separado de este repo C++) que **ya construye y corre** un framework de
UI declarativa completo sobre AvaLang — server-side rendering a HTML,
con diffing y patches, pero eso es lo de menos. Lo que importa para
este plan es que ya define, con código funcionando (no solo prosa como
`07_COMPONENT_TREE.md`), la sintaxis real que un archivo `.ava` de UI
usa. Cambia la sección 3 de punta a punta — ver ahí. Resumen de lo que
encontré:

- **`AvaLang.UI/Rendering/AvaComponentParser.cs`** — parser (basado en
  regex + indentación, no en el VM) que convierte texto con esta forma:

  ```
  import "components/navbar"

  state
      count = 0
      message = "Hello!"
  end

  view
      column
          fill = "true"
          padding = 20
          gap = 16

          Navbar()                      -- llamada a componente importado (PascalCase)

          text
              value = message            -- expresión, resuelta contra `state`
              fontSize = 32
              fontWeight = "bold"
          end

          button
              text = "+1"
              click = increment          -- prop de evento -> nombre de método
          end
      end
  end

  methods
      func increment()
          count = count + 1
      end
  end
  ```

  en un árbol `ComponentNode` (`Type`, `Id`, `Props`, `Children` —
  virtualmente el mismo shape que `DesignNode` en `design_document.h`,
  ya escrito para Fase 0 de este plan). Bloques `state`/`view`/`methods`
  (+ `properties`/`import`/`services`/`lifecycle`, ya reservados aunque
  no todos usados todavía), indentación para anidar, `prop = valor`
  para propiedades, y una lista fija de props de evento (`click`,
  `onchange`, `oninput`, etc.) que se guardan aparte como
  handler-por-nombre en vez de como prop de estilo.
- **`AvaLang.UI/Core/ComponentTree.cs`** (mismo repo .NET) llama
  literalmente a `ava_ui_create_tree` / `ava_ui_set_root` /
  `ava_ui_tree_to_json` — **la misma C API de `public/include/avalang.h`**
  que ya usa `EngineBridge::BuildDemoComponentTree()` acá. No es un
  framework paralelo con su propio modelo: es el mismo Component Tree,
  probado desde otro host.
- **`AvaLang.UI/Routing/RouteScanner.cs`** — routing file-based estilo
  Next.js sobre carpetas `Pages/`: `index.ava` → `/`, `about.ava` →
  `/about`, `users/[id].ava` → `/users/{id}` (con constraints tipo
  `[id:int]`, catch-all `[...slug]`), carpeta `components/` reservada
  (no genera rutas, solo se importa con `import "components/navbar"`).
- **`AvaLang.UI/docs/FRAMEWORK.md`** y **`AvaLang.UI/TODO_FLOW.md`**
  documentan el principio de diseño: *"el VM de AvaLang ejecuta el
  script; el framework host hace routing/rendering/diff"* — el parser
  solo extrae estructura (view/methods) una vez, y el VM evalúa
  expresiones de `state` contra ese texto tal cual. Ningún `ui.*`
  builtin especial hace falta para esto: es texto + un parser de
  bloques, exactamente el mismo truco que evita el hueco #3 de la
  sección 2 de abajo.
- Nota aparte, no bloqueante: encontré una página del scaffold
  (`AvaLang.UI.Template/.../users/[id].ava`) escrita en un estilo
  completamente distinto (`return Column(children: [...])`, con
  comentarios `--` y `var`/`return` sueltos) — no matchea el resto del
  proyecto .NET (`index.ava`, `home.ava`, `about.ava`, `layout.ava`,
  `navbar.ava`, `footer.ava` son todos consistentes con el bloque
  `state/view/methods` de arriba, y es el que `AvaComponentParser.cs`
  realmente parsea). Asumo que esa página es un experimento viejo sin
  actualizar y tomo el formato `state/view/methods` como la sintaxis
  real del proyecto — avisame si no es así.

**Conclusión para este plan:** el `.avaui` no debería ser JSON. Debería
ser exactamente esta sintaxis AvaLang de UI — la misma que ya corre en
el prototipo .NET — para que un archivo de Ava Studio y un archivo de
`AvaLang.UI.Web/Pages/*.ava` sean, literalmente, el mismo formato de
archivo. Ver sección 3 reescrita abajo.

---

## 1. Lo que ya existe y en lo que se apoya este plan

Revisé el proyecto y hay más base de la que parecía a primera vista:

- **`docs/architecture/07_COMPONENT_TREE.md` / `AVAUI_FRAMEWORK.md`**: ya
  define el modelo de datos (`Component`, `ComponentTree`, `LayoutType`)
  y el JSON de serialización que el Designer va a necesitar.
- **`public/include/avalang.h`**: la C API del Component Tree ya existe
  y funciona — `ava_ui_create_tree`, `ava_ui_create_component`,
  `ava_ui_set_property/add_child/set_layout/set_id`, y
  `ava_ui_tree_to_json` para serializar. Esto es lo que hoy usa
  `EngineBridge::BuildDemoComponentTree()`.
- **`studio/src/panels/preview_panel.cpp`**: ya dibuja un árbol
  clickeable de un `PreviewNode` (espejo host-side del Component Tree)
  y ya emite un `PropertiesState` al hacer click en un nodo.
- **`studio/src/panels/properties_panel.cpp`**: ya dibuja tabla
  Key/Value de un `PropertiesState`. Hoy es solo lectura (dice
  explícitamente `PROPERTIES_EDITABLE` como pendiente).
- **`studio/src/panels/editor_panel.h`**: el editor de código YA es
  multi-tab (`EditorState::tabs`, cada `EditorTab` con su propio
  `TextEditor`). Esto es clave: la vista Design no necesita ser un
  panel nuevo separado en el dock — puede vivir **dentro de la misma
  pestaña de documento**, exactamente como pedís (VS6 no tenía un panel
  "Design" aparte; el mismo tab del formulario cambiaba de vista con F7).
- **`studio/src/panels/explorer_panel.cpp`**: ya tiene lógica
  por-extensión (hoy solo para `.ava`, línea ~82) para íconos/color, y
  ya delega la apertura de doble-click a `file_to_open` que el caller
  (`main.cpp`) pasa a `OpenFileInTab`.

## 2. Lo que falta (huecos reales, no cosméticos)

Esto es importante decirlo antes del plan, porque cambia el orden de
las fases:

1. **No hay parser texto-AvaUI → Component Tree.** Existe
   `ava_ui_tree_to_json` (serializar a JSON, para depuración/otros
   consumidores) pero **no** existe nada que lea el archivo `.avaui`
   real de vuelta a un árbol editable. Ver sección 0.1: el formato ya
   no es JSON, es la sintaxis `state/view/methods` que
   `AvaComponentParser.cs` prueba en el prototipo .NET — el parser que
   falta acá es el equivalente C++ de ese archivo (indentación +
   bloques + `prop = valor`), no un parser JSON. Sin esto no se puede
   recargar un `.avaui` guardado.
2. **No hay motor de layout con píxeles.** `07_COMPONENT_TREE.md`
   describe el algoritmo de Column/Row en prosa, pero no encontré
   implementación en `core/src/ui/`. El Designer necesita calcular
   rectángulos reales para poder dibujar controles y aceptar clicks/
   drag&drop sobre ellos.
3. **`ui.*` builtins del lenguaje están sin registrar.**
   `core/src/ui/builtins.cpp::RegisterUIBuiltins` es un no-op
   documentado a propósito ("nada en este snapshot lo llama"). Esto
   significa que hoy un script `.ava` **no puede declarar UI** vía el
   VM directamente. Para el Designer esto en realidad **no bloquea
   nada**: como muestra el prototipo .NET (sección 0.1),
   `AvaComponentParser.cs` arma el `ComponentNode`/`DesignNode` con un
   parser de texto aparte del VM, y solo llama al VM para evaluar
   expresiones sueltas dentro de `state`/props (`_vm.Eval(...)`) — el
   Designer puede hacer exactamente lo mismo sin esperar a que
   `ui.*` esté registrado. Sigue bloqueando específicamente un "Run"
   que ejecute la UI *desde dentro del lenguaje* (un script que
   declare su propia UI con sintaxis nativa del VM en vez de este
   parser de texto aparte) — eso queda como Fase 6, no como
   prerequisito.
4. **No hay enumeración genérica de propiedades por tipo de
   componente** (qué props tiene un `button` vs un `stack`) — hoy
   `PreviewNode.properties` se llena a mano en
   `BuildDemoComponentTree()`. El Toolbox/Properties del Designer
   necesita un catálogo de tipos de componente con sus props por
   defecto.

Nada de esto es motivo para no arrancar — son las piezas que agrego
como Fase 0 y Fase 1 abajo.

---

## 3. Formato de archivo `.avaui`

**Cambiado de la propuesta original** (ver sección 0.1). Ya no es JSON
con dos secciones — es texto plano en la sintaxis AvaLang de UI que ya
corre en el prototipo .NET, bloques `state`/`view`/`methods` (y
opcionalmente `properties`/`import`, reservados igual que allá):

```
import "components/navbar"

properties
    title = "Mi App"
end

state
    counter = 0
end

view
    column
        fill = "true"
        padding = 20
        gap = 16

        Navbar()

        text
            value = "Counter: " + counter
            fontSize = 32
        end

        button
            text = "Guardar"
            click = btnGuardar_Click
        end
    end
end

methods
    func btnGuardar_Click()
        -- handler
    end
end
```

- `view`: el árbol de componentes tal cual — esto es lo que dibuja el
  lienzo (`ComputeLayout` + `designer_canvas.cpp`, ya escritos en Fase
  0/2 sobre `DesignNode`, que ya tiene el shape correcto: `type`, `id`,
  `properties`, `events`, `children` — no hace falta tocar ese struct).
- `state`: variables iniciales del documento, mismo rol que
  `UiState`/`state.GetValue(...)` en el prototipo .NET — hoy
  `DesignDocument` no tiene un lugar para esto, ver sección 5.2
  actualizada.
- `methods`: el code-behind real, en sintaxis AvaLang normal
  (`func nombre(params) ... end`), no texto libre. Es lo que se ve al
  presentar F7 — mismo rol que un `.frm` de VS6 con su sección
  `Private Sub ... End Sub`, pero acá es simplemente el `TextEditor`
  ya existente mostrando el archivo `.avaui` completo (o solo el
  bloque `methods`, a decidir en Fase 1) igual que cualquier `.ava`.
- Props de evento (`click`, `onchange`, `oninput`, ... — misma lista
  fija que `AvaComponentParser.cs::EventProps`) se guardan aparte de
  las props de estilo, apuntando a un nombre de función que debe
  existir en `methods` — esto es literalmente `DesignNode::events` tal
  como ya está declarado en `design_document.h`, no hace falta un
  campo nuevo.
- Llamada a un componente importado: `Navbar()` — PascalCase, sin
  bloque `... end` propio en el sitio de la llamada (el árbol real de
  `Navbar` vive en `components/navbar.avaui` y se resuelve al cargar,
  mismo mecanismo que `ComponentResolver.cs::ResolveComponentCall`).
  No es prioridad de Fase 0-2 (no hay Explorer multi-archivo con
  imports todavía en el Designer), pero el formato lo soporta desde
  ya para no tener que romperlo cuando se implemente.

**Por qué este cambio y no la propuesta JSON original:** la única
razón para haber propuesto JSON era "evitar inventar un lenguaje de UI
declarativa ejecutable ahora mismo". Ya está inventado, probado, y
corriendo en otro host (el prototipo .NET) — no hay nada que evitar.
Usar el mismo formato en vez de un JSON paralelo significa que un
archivo `.avaui` de Ava Studio y un archivo `Pages/*.ava` del framework
.NET son intercambiables, y que lo que se ve al presionar F7 es
AvaLang real, no un blob de texto sin relación con el lenguaje.

**Impacto en lo ya implementado (Fase 0 y Fase 2 base, ya
entregadas):** `avaui_json.{h,cpp}` (el parser/writer JSON) queda
obsoleto con este cambio — su reemplazo es un parser de texto con
indentación, más parecido a lo que hace `AvaComponentParser.cs`
(regex por línea + tracking de indentación) que a un parser JSON. El
resto de Fase 0 (`design_document.h/.cpp`, `layout_engine.*`,
`component_catalog.*`) y Fase 2 base (`toolbox_panel.*`,
`designer_canvas.*`) no dependen del formato de archivo en sí — todas
operan sobre `DesignNode`/`DesignDocument` en memoria, así que no hace
falta tocarlas todavía. `design_document.h` sí necesita un campo
nuevo para `state` (hoy solo tiene `code_behind` como string libre;
pasa a ser específicamente el bloque `methods`, y falta un
`std::vector<PropertyRow> initial_state` o similar para el bloque
`state`). Dejo esto para cuando confirmes que seguimos con la
implementación — este mensaje es solo el documento actualizado, según
pediste.

---

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

## 9. Progreso real (actualizado tras la migración texto-AvaLang)

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

