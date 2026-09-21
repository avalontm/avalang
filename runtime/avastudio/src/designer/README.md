# designer/ — Fase 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 y 11

Este directorio es el núcleo formal del Designer descrito en
`AVASTUDIO_DESIGNER_IMPLEMENTATION_ARCHITECTURE.md`, sección 5 en adelante.
Fase 0 solo define contratos y tipos; ninguna pantalla de AvaStudio
depende todavía de estos archivos (eso empieza en Fase 3 en adelante).

## UiNode / ComponentTree / RenderTree / Layout protocol

`avalang.ui.dll` ya implementa estas capas (`components/IComponent.h`,
`components/ComponentTree.h`, `render_tree/IRenderNode.h`,
`render_tree/IRenderTree.h`, `layout/LayoutTypes.h`). `types.h` no las
reimplementa: las expone como alias bajo `studio::designer`, para que
el resto del Designer nunca referencie `avalang::ui::*` directo y
tenga una sola fuente de verdad (decisión 7 del documento: "Designer
utiliza el mismo runtime de UI").

## Property metadata / Component metadata

`PropertyRow` (en `panels/properties_panel.h`) ya existe pero es solo
`key = value`, sin tipo, categoría ni editor. `property_metadata.h` y
`component_metadata.h` agregan el contrato que falta (usado por
ComponentRegistry en Fase 2 y por el Properties Panel en Fase 5).
No reemplazan `PropertyRow` todavía.

## Command protocol

No existía ningún mecanismo de undo/redo: `DesignDocument` (en
`design/design_document.h`) se muta directo (`AddComponentNode`,
`RemoveNode`, `MoveNode`, `EditComponentNode`). `command.h`/`command.cpp`
agregan `ICommand` y `CommandManager` (con transacciones vía
`BeginTransaction`/`EndTransaction`, agrupadas en `CompositeCommand`).
Los comandos concretos (`CreateComponentCommand`, `SetPropertyCommand`,
etc.) se implementan en Fase 6, cuando el Designer Surface exista.

## Layout Core (Fase 1)

`Measure`/`Arrange`/`Rect`/`Constraints` y el soporte de `Row`/`Column`
ya existen completos en `avalang.ui.dll` (`layout/LayoutEngine.h`,
`layout/LayoutEngineImpl.cpp`). `Container`/`Stack` no tienen un caso
propio ahí: cualquier tipo sin manejador específico cae en el `else`
final, que ya calcula tamaño como el máximo de los hijos y los
organiza superpuestos (`ArrangeStack`) — exactamente la semántica que
`Container`/`Stack` necesitan, así que tampoco se reimplementan.

`live_render_bridge.cpp` y `panels/designer_canvas.cpp` ya invocan
`avalang::ui::LayoutEngine` directo, cada uno por su cuenta. `layout_core.h`/
`layout_core.cpp` agregan `LayoutCore`: un wrapper único sobre ese mismo
`LayoutEngine` que expone `Compute`/`MeasureNode`/`ArrangeNode` y cachea
el resultado por `NodeId` (`RectOf`/`HasRect`), para que el resto del
Designer consulte geometría por `NodeId` en vez de tocar `ComponentId`/
`ILayoutNode` de avaui directo. Los dos sitios existentes no se
migraron todavía a `LayoutCore` — eso es parte de Fase 3 (Designer
Surface), cuando el canvas se reconstruya sobre `DesignerContext`.

## Component Registry (Fase 2)

`avalang::ui::registry::ComponentTypeRegistry` (real, en `avaui/src/registry/`)
ya es la única fuente de verdad para `type`/`display_name`/`is_container`/
`default_properties` — cada control (`Button.cpp`, `TextBox.cpp`, etc.) se
autorregistra ahí. `component_registry.h`/`component_registry.cpp` no
reimplementan ese registro: `ComponentRegistry` itera
`GetComponentTypeRegistry()` una sola vez y construye un `ComponentMetadata`
(Fase 0) por tipo, sin duplicar esa lista en otro lado.

Lo que sí faltaba, porque ningún lugar del código lo modela hoy:

- **Tipado de propiedades.** El registro real solo da un `PropertyValue`
  (bool/number/string) por default. `ComponentRegistry` deriva `type`,
  `category` (heurística por nombre: `text`/`label` → Typography,
  `width`/`gap`/`padding` → Layout, `isEnabled`/`isChecked` → Behavior, etc.)
  y `designerEditor` (`PropertyEditorKind`) para cada propiedad, y usa esa
  misma lista como `defaultProperties` y `supportedProperties` — hoy son el
  mismo conjunto porque el runtime no distingue "propiedad soportada sin
  default" de "propiedad con default".
- **`category`/`icon` del Toolbox.** Se reutiliza `design::FindComponentType`
  (el catálogo legado ya cacheado desde `data/design/component_catalog.csv`)
  en vez de releer el CSV o duplicar esos valores.
- **`supportedEvents`.** No existe ningún concepto de evento distinto de una
  propiedad en `avaui` hoy (`click = Save` es, en los hechos, una propiedad
  con un handler asignado — ver sección 41 del documento de arquitectura).
  Como el Properties Panel (Fase 5) y el Command system (Fase 6) sí necesitan
  distinguir "esto dispara comportamiento" de "esto es un valor", se agregó
  una tabla mínima, por tipo, de eventos conocidos (`button`→`click`,
  `checkbox`→`change`, `dialog`→`open`/`close`, etc.), documentada aquí para
  que quede explícito que es metadato nuevo del Designer, no algo leído de
  otra fuente existente.
- **`allowedParents`.** Se deja vacío (sin restricción) en todos los tipos:
  no hay ninguna fuente real de reglas de anidamiento todavía: introducir
  restricciones inventadas aquí sería exactamente el tipo de segunda fuente
  de verdad que la Fase 0 evita.
- **`designerCapabilities`.** `isContainer`/`canReorderChildren` se derivan
  de `is_container` (real); `canResize` se deja en `true` para todos porque
  todo nodo tiene geometría vía `LayoutCore` (Fase 1); `canReparent`,
  `canDelete`, `canDuplicate` quedan en los defaults de `DesignerCapabilities`
  (Fase 0).

`ComponentRegistry::Instance()` construye la lista una sola vez (estático),
igual que `design::GetComponentCatalog()`. Nada del Toolbox ni del
Properties Panel se migró a `ComponentRegistry` todavía — Toolbox es Fase 7
y Properties es Fase 5; ambas fases consumirán este registro en vez de
`design::ComponentTypeInfo`/`PropertyRow`.

## Designer Surface (Fase 3)

Analizado antes de escribir: `panels/designer_canvas.cpp` (1161 líneas) ya
dibuja el canvas completo en modo inmediato de ImGui — selección, hover,
drag/drop, presets de dispositivo — pero todo mezclado en una sola función,
usando `design::DesignDocument::selected_node_id` (un solo string, sin
hovered/focused/multi-selección) y hit-testing implícito vía
`ImGui::IsItemHovered()` por cada ítem dibujado (no una función `HitTest`
reutilizable). `design/live_render_bridge.cpp` ya arma
`LayoutEngine`+`RenderTree`+`SceneGraph` reales (`BuildLiveRender`), y
`design/imgui_renderer.cpp` ya es el `Renderer` real (implementa
`avalang::ui::IRenderer`/`BaseRenderer`). Ninguno de los dos se reimplementó.

Nuevo, formalizando exactamente lo que la sección 12/21/30/37 del documento
pide y que no existía como contrato reutilizable:

* `selection_manager.h`/`.cpp` — `SelectionManager`: selección múltiple,
  `Primary()` (la más reciente), `Hovered()`, `Focused()`, independiente de
  `DesignDocument::selected_node_id`. `DesignerContext` (Fase 4) decidirá
  cómo sincronizar ambos; esta fase no toca `design_document.h`.
* `viewport.h`/`.cpp` — `DesignerViewport`: zoom/pan con `ToCanvas`/`ToScreen`,
  `FitToScreen`, `Center`. No existía ningún concepto de zoom/pan en el
  canvas actual (los "device presets" de `designer_canvas.cpp` cambian el
  tamaño disponible para layout, no la vista); esto es genuinamente nuevo.
* `hit_test.h`/`.cpp` — `HitTest(tree, LayoutCore, point)`: recorre el árbol
  real (`IComponent::Children()`) probando cada `LayoutRect` cacheado por
  `LayoutCore` (Fase 1) de atrás hacia adelante, devolviendo el nodo más
  profundo/superior en ese punto. Reutiliza el caché de Fase 1 en vez de
  reimplementar el recorrido de `ILayoutNode` o duplicar el mapa
  `nodeIdToRect` que ya arma `live_render_bridge.cpp`; se agregó
  `LayoutCore::Rects()` (accessor de solo lectura) para poder iterarlo.
* `overlay.h`/`.cpp` — `BuildOverlay(SelectionManager, LayoutCore)`: produce
  una lista de `OverlayItem` (rect + tipo: selección/selección primaria/hover)
  como datos puros, separados de cómo se dibujan (`designer_canvas.cpp` sigue
  dibujando sus propios anillos de selección por ahora — la migración de ese
  dibujo a consumir `OverlayItem` es parte de Fase 4).
* `surface.h`/`.cpp` — `DesignerSurface`: agrupa `LayoutCore`+`DesignerViewport`+
  `SelectionManager` y expone `Pick()` (screen→canvas vía viewport, luego
  `HitTest`) y `Overlay()`. También define `enum class DesignerLayer`
  (Background/Grid/Component/Selection/Guide/DragDrop/Interaction, sección 12)
  como el contrato de orden de capas que el canvas reconstruido usará.
* `types.h` — se agregó `Renderer = avalang::ui::IRenderer`, la abstracción
  real (`renderer/IRenderer.h`) que `ImGuiRenderer` (en `design/`) ya
  implementa; no se creó una interfaz nueva.

Nada de esto sustituye todavía a `designer_canvas.cpp`: es la base formal
sobre la que Fase 4 (Tree) y la reconstrucción del canvas se apoyan.
`DesignerSurface` no se instancia desde ninguna pantalla real todavía.

## Tree (Fase 4)

Analizado antes de escribir: no existe ningún panel de árbol del documento
(`.avaui` component tree) en AvaStudio hoy. `panels/explorer_panel.h` es el
árbol de archivos/carpetas del proyecto en disco — un concepto totalmente
distinto — y no hay ningún otro panel que liste la jerarquía de componentes
de un `.avaui` abierto. Esta fase formaliza ese modelo, todavía sin panel
ImGui real (igual que Fase 3, la UI queda para cuando se reconstruya el
canvas).

* `document_tree.h`/`.cpp` — `BuildDocumentTree(tree)` recorre el árbol real
  (`IComponent::Children()`, ya usado por `hit_test.cpp`) en pre-order y
  produce una lista plana de `DocumentTreeNode` (id, tipo, profundidad,
  si tiene hijos). `DisplayNameFor(node)` usa la propiedad `id` del nodo
  cuando existe (la identidad semántica de la sección 8 del documento,
  ej. `button saveButton`) y cae a `TypeOf(node)` si no está seteada —
  nunca se muestra el `NodeId` interno al usuario, tal como pide la
  sección 8 ("`NodeId` puede permanecer interno").
* `tree_state.h`/`.cpp` — `TreeState`: estado de expandir/colapsar por
  `NodeId` (expandido por default; solo se recuerdan los colapsados) y
  `VisibleNodes(tree)`, que filtra `BuildDocumentTree` ocultando los
  descendientes de cualquier nodo colapsado. `ExpandAncestorsOf(id, tree)`
  es la mitad que faltaba de "Canvas ↔ Tree synchronization": cuando una
  selección se origina en el Canvas (`DesignerSurface::Pick`, Fase 3), el
  Tree debe poder expandirse para revelarla. Reutiliza
  `design::FindNodeById` (ya real, en `design/design_document.h`) para
  ubicar el nodo por `NodeId` y sube por `IComponent::Parent()` — no se
  reimplementó la búsqueda por id ni el recorrido de ancestros.

La sincronización de selección en sí (la otra mitad de "Canvas ↔ Tree")
no necesitaba código nuevo: `SelectionManager` (Fase 3) ya es la única
fuente de selección que la sección 21 pide compartir entre Canvas, Tree y
Properties — un futuro panel de árbol solo necesita leer/escribir ese mismo
`SelectionManager`, no un mecanismo de sincronización aparte.

## Properties (Fase 5)

Analizado antes de escribir: `panels/properties_panel.h`/`.cpp` (Fase 0 ya
lo señaló) solo conoce `PropertyRow` (`key = value` en texto plano, sin
tipo, categoría ni fuente); no distingue "valor puesto por el usuario" de
"valor por default del tipo", y su edición no pasa por ningún `ICommand`
(los llamadores de `PropertyEdit` mutan `DesignDocument` directo vía
`EditComponentNode`). Esta fase no toca ese panel — formaliza, bajo
`studio::designer`, lo que le falta para dejar de ser la única fuente:

* `property_editor.h`/`.cpp` — funciones puras de conversión entre
  `avalang::ui::PropertyValue` y texto de UI: `FormatPropertyValue` (antes
  vivía duplicada como `DisplayValue`, local a `component_registry.cpp`;
  se movió aquí y `ComponentRegistry` ahora la reutiliza, igual que ya
  reutilizaba `PropertyTypeName`, también movida aquí) y
  `TryParsePropertyValue`, que hace el camino inverso (texto → `PropertyValue`
  tipado) validando que el texto se consuma por completo (`Bool`: `true`/
  `false`/`1`/`0`; `Number`: `std::stod` estricto; `String`: siempre válido;
  `List`/`Nil` no son editables en línea, se rechazan). `ValidatePropertyEdit`
  combina ese parseo con `PropertyMetadata::validation` (un campo que ya
  existía en Fase 0 pero que nada leía todavía) como patrón `std::regex`
  opcional — es la primera vez que ese campo tiene un consumidor real.
* `property_grid.h`/`.cpp` — `BuildPropertyGrid(node)` arma, para el nodo
  real (`UiNode*`), una lista de `PropertyGridSection` agrupadas por
  `PropertyCategory` (mismo enum de Fase 0), en el orden fijo que la
  sección 21 implica (Identity primero, Advanced al final). Cada fila
  (`PropertyGridRow`) reutiliza el `PropertyMetadata` completo de
  `ComponentRegistry::Find(TypeOf(node))` (Fase 2) y le agrega el valor
  *actual* del nodo (`IComponent::GetProperty`), marcando `PropertySource::
  Local` si el nodo lo tiene puesto o `PropertySource::Default` si se está
  mostrando el default del tipo — algo que `PropertyRow` no podía expresar.
  `supportedEvents` (Fase 2) se proyecta como filas más, en la categoría
  `Events` (también existía en el enum de Fase 0 sin ningún consumidor).
  La fila `id` no viene de `ComponentRegistry` (ningún control la declara
  como default property — es una propiedad transversal que ya usa
  `DisplayNameFor`, Fase 4): se sintetiza aquí mismo como
  `PropertyCategory::Identity`, sin inventar una segunda fuente para el
  nombre del nodo.
* `property_commands.h`/`.cpp` — `SetPropertyCommand`, el primer `ICommand`
  concreto (Fase 0 dejó los concretos para Fase 6, pero el roadmap pide
  este explícitamente aquí porque sin él la Property Grid no tiene forma
  de mutar con undo/redo). Guarda el valor anterior (`GetProperty` antes de
  `SetProperty`, o "no tenía valor" para poder hacer `RemoveProperty` en el
  undo) y localiza el nodo por `NodeId` vía `design::FindNodeById` en cada
  `Execute`/`Undo`/`Redo` — mismo patrón que `TreeState::ExpandAncestorsOf`
  (Fase 4): no cachea el puntero `UiNode*`, porque el árbol puede mutar
  entre comandos.

Nada de esto está conectado todavía a `properties_panel.cpp` ni a
`CommandManager`: como en fases previas, es el contrato formal que un
Properties Panel reconstruido (y el resto de comandos de Fase 6) usarán.

## Commands (Fase 6)

Analizado antes de escribir: `CommandManager`/`ICommand`/`CompositeCommand`
(Fase 0) ya implementan Undo/Redo y transacciones — nada que hacer ahí, el
roadmap los da por incluidos. El resto de mutaciones estructurales hoy
viven en `design_document.cpp` como funciones libres que mutan
`DesignDocument&` directo y sin deshacer (`AddComponentNode`, `RemoveNode`,
`MoveNode`), y `ComponentTree::DestroyComponent` (real, en `avaui.dll`)
libera el nodo por completo del pool — no hay ninguna forma de "soft
delete" en la API real. Esa es una limitación real, no algo que se pueda
resolver sin tocar `avalang.ui.dll`, así que los comandos de esta fase la
asumen explícitamente: en vez de `DestroyComponent`, deshacer
crear/borrar/pegar hace *detach* (`RemoveChild`, que ya deja
`Parent() == nullptr`) y solo llama `DestroyComponent` (recursivo, sobre
todo el subárbol) en el destructor del comando, y solo si el nodo sigue
desconectado en ese momento — si el comando sigue en el undo stack (el
nodo está vivo en el documento), el destructor no toca nada.

* `property_commands.h`/`.cpp` (extendido) — `SetStyleCommand` y
  `SetEventCommand` son subclases de `SetPropertyCommand` (Fase 5) que
  solo fijan qué propiedad tocan (`"style"`, o el nombre del evento como
  `"click"`) y qué `Description()` devuelven; no hay una segunda
  implementación de Execute/Undo/Redo — la sección 41 del documento ya
  aclara que un evento *es*, en los hechos, una propiedad con un handler
  asignado, y `Button`/etc. ya declaran `style` como propiedad plana real,
  así que no había ninguna mecánica nueva que formalizar, solo el nombre
  del comando que el roadmap pide distinguir.
* `move_commands.h`/`.cpp` — `MoveComponentCommand`/`ReparentComponentCommand`.
  El algoritmo de reordenar/reparentar ya existía, completo y correcto,
  como `design::MoveNode(DesignDocument&, ...)` — pero atado a
  `DesignDocument`, no a un `UiComponentTree*` crudo. Se extrajo (cambio
  aditivo, sin tocar la firma ni el comportamiento de los 2 llamadores
  reales en `designer_canvas.cpp`) un overload
  `MoveNode(IComponent* root, movedId, targetId, DropZone)` en
  `design_document.h`/`.cpp`, y la sobrecarga original ahora es un wrapper
  de una línea sobre ese overload. `MoveComponentCommand` llama a ese
  mismo overload para Execute/Redo, y para Undo captura, antes del primer
  Execute, el padre y el siguiente hermano originales del nodo movido
  (`NextSiblingIdOf`, nuevo) para poder revertir con el mismo primitivo
  (`kBefore` sobre ese hermano, o `kInto` sobre el padre si era el último
  hijo). `ReparentComponentCommand` es un `MoveComponentCommand` con
  `zone` fijo en `kInto` — la sección 27 no pide ninguna mecánica de
  preservación adicional (identidad/propiedades/estilos/eventos ya se
  preservan solos porque el nodo nunca se recrea, solo se relinkea; el
  recálculo de layout ya lo dispara `LayoutCore`, Fase 1, al recomputar
  desde la estructura del árbol).
* `lifecycle_commands.h`/`.cpp` — `CreateComponentCommand`,
  `DeleteComponentCommand`, `PasteComponentCommand` y
  `DuplicateComponentCommand`. Ninguno de los 4 existía en ninguna forma
  deshacible (`AddComponentNode`/`RemoveNode` legados son permanentes:
  `RemoveNode` llama `DestroyComponent` directo, `AddComponentNode` no
  guarda nada para un undo). `CreateComponentCommand`/`DeleteComponentCommand`
  son estructuralmente inversos (crear = attach de un nodo nuevo, borrar =
  detach de uno existente) y comparten el mismo patrón detach/destroy-on-
  discard. Para Paste/Duplicate (sección 43: clonar subtree + nuevas
  identidades + un solo comando) no existe ningún método `Clone` en
  `IComponent`/`ComponentTree` — se implementó `CloneSubtree` (recorre
  `PropertyNames()`/`GetProperty` reales, copia todo excepto `id`, que se
  regenera con `UniqueId` comparando contra los `id` ya presentes en el
  árbol) como la única función de clonado, sin inventar un formato de
  clipboard serializado aparte (la sección 46 ya reserva la escritura
  canónica a `AvauiWriter`, que esta fase no toca). `PasteComponentCommand`
  es el primitivo (clona `sourceId` y lo cuelga de `targetParentId`
  explícito) y `DuplicateComponentCommand` es un `PasteComponentCommand`
  que resuelve `targetParentId_` como el padre *actual* del origen justo
  antes de ejecutar — no hay una segunda implementación de clonado.
  `DestroySubtree` (nuevo, recursivo) también corrige un caso real que
  `DestroyComponent` no cubre: solo libera un nodo y desconecta a sus
  hijos (`SetParent(nullptr)`) sin destruirlos, así que un `Delete`/`Create`
  descartado sobre un nodo contenedor fugaría a sus hijos si se llamara
  `DestroyComponent` directo en vez de recorrer el subárbol primero.

Nada de esto está conectado todavía a `designer_canvas.cpp` ni a un
`CommandManager` real instanciado desde AvaStudio — es, otra vez, el
contrato formal; conectarlo a una UI concreta (Toolbox arrastrando para
crear, Canvas arrastrando para mover, atajos de teclado para
Undo/Redo/Duplicate) queda para cuando esas pantallas se reconstruyan
sobre `DesignerSurface`/`DesignerContext`.

## Toolbox (Fase 7)

Analizado antes de escribir: `panels/toolbox_panel.h`/`.cpp` (legado) ya
dibuja categorías (`design::GetComponentCatalog()`, agrupado por
`info.category` en el orden del CSV) y ya arrastra hacia
`kToolboxDragDropId` — pero no filtra por texto (sección 19 pide
`search`) y no expone ningún contrato de a qué se suelta ni con qué
operación. `panels/designer_canvas.cpp` (`HandleDropTarget`,
`ComputeDropZone`) ya resuelve, por su cuenta y acoplado a ImGui
(`ImVec2`, `ImGui::GetMousePos()`), una zona de tres estados
(`kBefore`/`kInto`/`kAfter`) y ejecuta el drop directo contra
`DesignDocument` (`AddComponentNode` seguido de `MoveNode` cuando el
target no es contenedor) — dos mutaciones sueltas, ninguna pasa por
`ICommand`, así que un drop del Toolbox no tiene undo hoy. Ninguno de
los dos archivos se tocó.

* `toolbox_model.h`/`.cpp` — `ToolboxModel::Sections(query)` agrupa
  `ComponentRegistry::All()` (Fase 2) por `category` y filtra cada
  entrada con `Matches` (substring case-insensitive sobre `displayName`
  y `type`), formalizando `search`/`categories` de la sección 19 sin
  releer el CSV ni duplicar `design::GetComponentCatalog()`. Para que el
  orden de categorías y de entradas dentro de cada una coincida con el
  Toolbox legado (el CSV trae una columna `order` que `ComponentRegistry`
  ignoraba desde Fase 2), se agregó `int order` a `ComponentMetadata` —
  poblado desde el mismo `design::FindComponentType` que ya alimenta
  `category`/`icon`, con `INT_MAX` para los tipos sin fila en el CSV,
  igual que hace `design::GetComponentCatalog()` con su `next_order` —
  y `ComponentRegistry` ahora ordena `entries_` por ese campo en el
  constructor (`std::stable_sort`, una sola vez). Nada consumía todavía
  el orden de iteración de `ComponentRegistry::All()`/`ByCategory()`
  (Fase 5/6 solo usan `Find(type)`, indexado por tipo), así que ordenar
  ahí no cambia comportamiento existente.
* `drop_target.h`/`.cpp` — formaliza exactamente el pipeline de la
  sección 20 (`DesignerSurface → HitTest → DropTargetResolver →
  DropOperation → Command`), independiente de ImGui:
  - `ComputeDropZone(rect, point, isContainer, allowSibling)` es la misma
    regla que `designer_canvas.cpp::ComputeDropZone`: un no-contenedor
    resuelve mitad superior/inferior → `kBefore`/`kAfter`; un contenedor
    con `allowSibling` resuelve banda superior → `kBefore`, banda
    inferior → `kAfter` y el centro → `kInto` (banda = 20% de la altura,
    entre 6 y 16 px y nunca más del 30%); un contenedor sin
    `allowSibling` (la raíz) siempre resuelve `kInto`. Trabaja sobre
    `LayoutRect`/`LayoutPoint` (Fase 1) en vez de `ImVec2`, para que
    cualquier superficie (no solo el canvas ImGui) pueda resolver una
    zona de drop.
  - `FlowLayoutOf`, `ComputeInsertIndex`, `ResolveInsertPosition` y
    `ComputeInsertMarker` resuelven, para un `kInto` sobre un contenedor
    con hijos, en qué posición cae el punto de suelta. El eje sale del
    tipo (Row/Column/For/If, ScrollView/ListView/Flex según `direction`,
    Grid por `columns`; Page/Container/Stack se apilan y siempre agregan
    al final). La posición se traduce a "antes de este hijo"
    (`kBefore`) para reutilizar `MoveNode`/`InsertRelative` sin nuevos
    comandos.
  - `ResolveDropTarget(targetId, rect, point, isContainer, sourceKind)`
    añade la capa que no existía: `DropOperation` (`InsertChild`,
    `InsertBefore`, `InsertAfter`, `Reparent`, `Replace` — los 5 que
    pide la sección 20). `DragSourceKind` distingue si lo que se suelta
    es un componente nuevo del Toolbox o un nodo existente del Canvas,
    porque es la única diferencia real entre `InsertChild` y `Reparent`
    (ambos son `zone == kInto`; la operación aplicable depende de si el
    nodo ya existe en el árbol o hay que crearlo). `Replace` queda
    declarado en el enum porque el roadmap lo pide explícitamente, pero
    `ResolveDropTarget` nunca lo produce: no hay ninguna interacción real
    (ni en `designer_canvas.cpp` ni en ningún otro lugar) que pida
    reemplazar un nodo por otro, así que inventar cuándo dispararlo sería
    la misma segunda fuente de verdad que Fase 2 evitó con
    `allowedParents`.
  - `BuildDropOverlay(target, layout)` activa `OverlayItemKind::
    DropIndicator`, declarado en `overlay.h` desde Fase 3 pero sin
    ningún productor hasta ahora (mismo patrón que
    `PropertyMetadata::validation` en Fase 5): para `kInto` devuelve el
    rect completo del target; para `kBefore`/`kAfter`, una franja fija
    (`kDropIndicatorThickness = 4.0`, valor propio de este archivo, sin
    equivalente previo) pegada al borde superior o inferior del target.
    Es la mitad de "drop preview" que pide la sección 19/20 — la otra
    mitad (dibujar `OverlayItem` en la capa `DesignerLayer::DragDrop`)
    sigue sin UI real, igual que el resto de `OverlayItem` desde Fase 3.
  - `BuildCreateDropCommand`/`BuildMoveDropCommand` cierran el pipeline
    en el último paso (`DropOperation → Command`): el primero arma un
    `InsertComponentCommand` (nuevo, abajo) y el segundo un
    `MoveComponentCommand` (Fase 6) ya existente — ninguno reimplementa
    lógica de mutación, solo traducen un `DropTarget` resuelto al
    comando concreto.
* `lifecycle_commands.h`/`.cpp` (extendido) — `InsertComponentCommand`:
  el equivalente deshacible de lo que `HandleDropTarget` hace hoy en dos
  pasos sueltos (`AddComponentNode` + `MoveNode`). `Attach()` decide el
  padre real según la zona (`target` mismo si `kInto`, o `target->
  Parent()` si `kBefore`/`kAfter`), hace `AddChild` una vez y, solo para
  `kBefore`/`kAfter`, reutiliza `design::MoveNode` (el mismo primitivo
  que ya usa `MoveComponentCommand`, Fase 6) para reposicionar contra el
  hermano correcto — no hay una segunda implementación de reordenamiento.
  Comparte el patrón detach-on-undo/destroy-on-discard exacto de
  `CreateComponentCommand` (misma fase, mismo archivo), incluyendo
  `DestroySubtree` ya existente: no se dupicó ese destructor.

Nada de esto está conectado todavía a `toolbox_panel.cpp` ni a
`designer_canvas.cpp` — como en cada fase anterior, es el contrato
formal (`ToolboxModel` para qué mostrar y buscar, `drop_target.h` para
qué comando ejecutar y qué previsualizar) que un Toolbox e Interaction
Layer reconstruidos sobre `DesignerSurface`/`DesignerContext` usarán en
vez de `design::GetComponentCatalog()` + `HandleDropTarget` directo.

## Code Sync (Fase 8)

Analizado antes de escribir: el round-trip real (sección 45, `.avaui ↕
AST ↕ Designer`) ya existe y funciona — `avalang::ui::parser::
AvauiParser`/`AvauiWriter` (reales, en `avaui/src/parser/`) son la única
fuente de verdad de parseo/escritura, y `design::ParseAvauiText`/
`design::SaveAvauiFile` (`design/design_document.cpp`) ya los envuelven.
`panels/editor_panel.cpp::ToggleTabViewMode` ya hace la sincronización
bidireccional completa: Design→Code llama `WriteAvaui` con un
`AvauiWriteOptions` armado a mano y Code→Design llama `ParseAvauiText`,
mostrando el error de parseo inline (`avaui_load_error`, marcador en el
editor) si el texto no es válido. Ninguno de los dos archivos se tocó.

Dos gaps reales, no cosméticos, quedaron a la vista de esa lectura:

- **Ningún lugar verifica el round-trip** (sección 47: `parse → AST →
  write → parse → comparar AST`). `ToggleTabViewMode` confía en que
  `WriteAvaui` y `ParseAvauiText` son inversos exactos, pero no existe
  ninguna función que lo compruebe — ni en tests, ni en runtime. Tampoco
  existe ningún comparador de árboles: `IComponent`/`PropertyValue` no
  tienen `operator==`, y comparar por `NodeId()` sería incorrecto (cada
  parseo crea nodos con identidad interna nueva; comparar por ahí
  compararía instancias, no equivalencia semántica).
- **`AvauiWriteOptions::extends` existe pero nadie lo usa.** `design::
  SaveAvauiFile` (`design_document.cpp`, no tocado) arma su propio
  `AvauiWriteOptions` y nunca copia `doc.extends` — un documento con
  `extends` pierde esa directiva al guardar desde Design. Es un bug real
  en código ya existente fuera del alcance de `designer/`; `code_sync.h`
  no lo corrige ahí, pero tampoco lo hereda: arma su propio
  `AvauiWriteOptions` completo (`WriteOptionsFor`, incluye `extends`)
  para que `VerifyRoundTrip` no falle por esa causa distinta a lo que
  realmente está midiendo.

Nuevo, en `runtime/avastudio/src/designer/`:

* `code_sync.h`/`.cpp` — `NodesEquivalent(a, b, differences?)`: el
  comparador de árboles que faltaba. Compara `TypeName` y el conjunto de
  propiedades (nombre → valor formateado vía `FormatPropertyValue`,
  Fase 5 — se reutiliza esa conversión en vez de comparar
  `PropertyValue` campo a campo, porque ya es la representación
  canónica que el resto del Designer usa para mostrar un valor) de forma
  no ordenada (`std::unordered_map`, para que un `PropertyNames()` en
  distinto orden entre dos parseos no cuente como diferencia), y baja
  recursivamente por `Children()` en orden posicional — el orden de los
  hijos sí es semántico (es la estructura del `view`), a diferencia del
  orden de las propiedades. Deliberadamente no compara `NodeId()` por lo
  ya explicado. Sin `differences` (`nullptr`, el caso común de "¿son
  equivalentes, sí o no?") corta en el primer mismatch; con
  `differences` recorre todo el árbol y acumula cada divergencia
  encontrada, para uso como reporte de test.
  `VerifyRoundTrip(sourceText)` es, literalmente, el pipeline de la
  sección 47: parsea el original (`design::ParseAvauiText`, real),
  regenera texto (`WriteAvaui`, real, con `WriteOptionsFor` — el
  `AvauiWriteOptions` completo que corrige el gap de `extends`
  mencionado arriba), reparsea ese texto generado, y compara ambos
  árboles con `NodesEquivalent`. No reimplementa parseo ni escritura en
  ningún paso — solo encadena las funciones reales y agrega la
  comparación que no existía.
* `code_sync.h`/`.cpp` (continuación) — `DesignerSyncMode` (`Design`,
  `Code`, `Split`) formaliza los 3 modos que pide la sección 45. `panels/
  editor_panel.h::TabViewMode` (legado) solo tiene `Code`/`Design` — no
  hay ningún `Split` real en AvaStudio hoy, así que no hay ninguna
  fuente que migrar; el tercer valor queda declarado como el contrato
  que un futuro modo dividido implementará, sin inventar cómo se vería
  esa UI. `DocumentState` (`Clean`/`Dirty`/`Saving`/`Saved`/`Error`,
  sección 55) formaliza lo que hoy es un único `bool dirty` repetido en
  paralelo en `design::DesignDocument::dirty` y `EditorTab::dirty`
  (ninguno de los dos distingue "guardando" de "sucio", ni tiene un
  estado de error persistente — un fallo de guardado hoy no deja
  rastro en ese `bool`). `Transition(current, event)` es una función
  pura sin estado (`DocumentEvent`: `Edit`/`SaveStarted`/
  `SaveSucceeded`/`SaveFailed`/`Reload`) — no reemplaza el `bool dirty`
  existente todavía, es el contrato de qué estado debería resultar de
  cada evento del ciclo guardar/parsear, para cuando `EditorTab` (o su
  reemplazo) se migre a un estado explícito de 5 valores en vez de un
  booleano.

Nada de esto está conectado todavía a `editor_panel.cpp`: `ToggleTabViewMode`
sigue siendo el único camino real de sincronización, y sigue sin ejecutar
`VerifyRoundTrip` en ningún punto (guardar, cambiar de modo, o como test).
Conectar `VerifyRoundTrip` a un test real, y migrar `EditorTab`/
`DesignDocument` al `DocumentState` de 5 valores, queda para cuando ese
panel se reconstruya sobre esta capa — igual que el resto de fases.

## Preview (Fase 9)

Analizado antes de escribir: `panels/designer_canvas.cpp` ya invoca el
runtime real de `avalang.ui.dll` para pintar el canvas —
`studio::design::BuildLiveRender` (`design/live_render_bridge.cpp`, no
tocado) arma `avalang::ui::LayoutEngine` + `IRenderTree` + `ISceneGraph`
reales a partir del `ComponentTree` del documento, y
`avalang::ui::ImGuiRenderer` + `SceneCommandWalker` (`avaui/src/commands/`,
reales) los pintan — sin pasar por ningún renderer paralelo de AvaStudio.
Eso es, literalmente, lo que pide la sección 34/Fase 9 ("usar el runtime
real"), y ya existe: no hay nada que reimplementar en la capa de
layout/render/scene.

Lo único con el nombre "Preview" que encontré, `panels/preview_panel.h`/
`.cpp` + `EngineBridge::PreviewNode`/`DemoTree` (`engine/engine_bridge.h`),
es código de demostración: el propio panel imprime
`"Component Tree (demo -- see note in engine_bridge.cpp)"` y no toca
`DesignDocument`, `ComponentTree` ni ningún tipo de `avaui/` — no
representa ningún documento real. No se tocó ni se reutilizó: adaptarlo
habría significado ponerle el nombre de una función real a ese código
demo.

El gap real, con eso descartado: la pintura real de `BuildLiveRender`
solo es alcanzable hoy a través de la caché por `tab_id` de
`designer_canvas.cpp` (`DesignerVmCacheEntry`) y siempre se pinta junto
con la interacción de edición de `DrawNode` en la misma pasada — anillo
de selección, hover, `ImGui::InvisibleButton` por nodo para
click/drag, breadcrumb, tray de diálogos. No existe ninguna función que
devuelva un frame renderizado sin esa capa de edición, y la sección 33
("Design Mode vs Preview Mode") nombra la distinción pero no hay ningún
tipo en el código que la represente.

Nuevo, en `runtime/avastudio/src/designer/`:

* `preview.h`/`.cpp` — `CanvasMode` (`Design`/`Preview`) formaliza
  exactamente la distinción de la sección 33. `BuildPreview(tree,
  viewport, extends, projectRoot)` reenvía directo a
  `studio::design::BuildLiveRender` — no reimplementa layout, render
  tree ni scene graph, solo expone ese mismo pipeline real como contrato
  de `designer/`, en espacio `NodeId`/`LayoutSize` (Fase 1) en vez de
  `int`/`int` sueltos, para que algo distinto de
  `DesignerVmCacheEntry::live_render` (acoplado a `tab_id` e ImGui)
  pueda pedir un frame. `PreviewFrame` es un alias directo de
  `LiveRenderResult` (mismo patrón de alias que `types.h` usa para
  `UiNode`/`RenderTree`/etc. desde Fase 0): una sola fuente de verdad,
  sin struct paralelo. `HasRect`/`RectOf` leen `nodeIdToRect` con la
  misma forma que `LayoutCore::HasRect`/`RectOf` (Fase 1), para que
  cualquier UI que ya sepa consultar geometría por `NodeId` en Design
  Mode sepa consultarla igual en Preview.
* `surface.h`/`.cpp` (extendido) — `DesignerSurface` ahora lleva
  `CanvasMode` (`Mode()`/`SetMode()`, por defecto `Design`, para no
  cambiar comportamiento existente) y `Overlay()` devuelve una lista
  vacía cuando el modo es `Preview`. Es el significado operacional real
  de "sin overlays de edición" (sección 33): en vez de dejarlo como una
  frase del documento, un `DesignerSurface` en `CanvasMode::Preview` deja
  de producir `OverlayItem` de selección/hover/drop, sin necesidad de
  que quien dibuje sepa filtrar por modo. `Pick()`/`Selection()` no se
  tocaron — elegir un nodo no es en sí mismo una acción de edición, y
  nada en el roadmap pide deshabilitar hit test en Preview, solo la capa
  de edición visual.
* `CMakeLists.txt` con la fuente nueva.

Verificado con `g++ -fsyntax-only` sobre todo `designer/*.cpp`: compila
limpio (requiere `glm/glm.hpp`, dependencia de terceros de
`avaui/src/scene/ISceneNode.h` ya usada transitivamente por
`live_render_bridge.h`, no añadida por esta fase).

Nada de esto está conectado todavía a `designer_canvas.cpp`:
`DrawNode` sigue haciendo su propio hit test manual con
`ImGui::InvisibleButton` en vez de `designer::HitTest`/`DesignerSurface::
Pick`, y sigue pintando overlays de selección incondicionalmente — no
hay ningún lugar hoy que llame `SetMode(CanvasMode::Preview)`. Migrar
`designer_canvas.cpp` (o una pantalla de Preview nueva) para construir
un `DesignerSurface` real, pedirle `BuildPreview` en vez de
`BuildLiveRender` directo, y pintar `Overlay()` en vez de la lógica
propia de `DrawNode`, queda para cuando ese panel se reconstruya sobre
`DesignerSurface`/`DesignerContext` — igual que el resto de fases.

## Responsive (Fase 10)

Analizado antes de escribir: `panels/designer_canvas.cpp` ya tiene
`kDevicePresets` (7 entradas: `Responsive` en 0×0 más seis tamaños fijos
Desktop/Web/Android/iOS) y un combo (`DrawDevicePresetBar`) que fuerza
`canvas_size` al tamaño elegido — eso cubre la mitad de "device
profiles" del roadmap, pero es un `constexpr` en un namespace anónimo de
ese `.cpp`, no exportado, así que nada fuera de ese archivo puede
consultarlo. `avalang::ui::theme::ProjectStyleSheet`/`BreakpointOverride`
(`avaui/src/theme/ProjectStyleOverrides.h`, reales) ya son "breakpoints"
funcionando de verdad — pero solo los consume
`HTMLRenderer::EmitProjectResponsiveCSS` (`avaui/src/renderer/
HTMLRenderer.cpp`), que emite un `@media (min-width: Xpx)` por
breakpoint y delega en la cascada CSS del navegador para resolver cuál
aplica. `RenderTheme::Apply`/`ApplyToComponent` (`avaui/src/theme/
RenderTheme.cpp`) — lo que `BuildLiveRender` (Fase 9) usa para pintar el
canvas del Designer — no recibe ningún ancho de viewport y nunca toca
`ProjectStyleSheet::Breakpoints()`: cambiar el device preset en el
Designer hoy cambia el tamaño del canvas pero nunca activa un
breakpoint, a diferencia de lo que pasaría en una exportación Web real
con el mismo proyecto. `ava::platform::mobile::ISafeArea`/
`SafeAreaInsets` (`avalang/platform/interfaces/services/mobile/
ISafeArea.h`, real) es "safe area" de verdad, pero es una consulta en
vivo al hardware (observer `OnSafeAreaChanged`) desde el backend Android
— no existe ninguna tabla de insets por perfil de dispositivo en ningún
lugar del repo, porque en tiempo de diseño no hay hardware que
consultar. Ninguno de los tres archivos se tocó.

Nota aparte: los tamaños de ejemplo de la sección 31 del documento de
arquitectura (`390×844`, `768×1024`, `1280×800`, `1920×1080`) no
coinciden con los reales de `kDevicePresets` salvo el de iPhone
(`390×844`) — son solo ilustrativos. `responsive.h`/`.cpp` sigue los
valores reales de `kDevicePresets` (código, no prosa del documento),
por la misma razón que Fase 7 prefirió el CSV real sobre cualquier otra
fuente.

Nuevo, en `runtime/avastudio/src/designer/`:

* `responsive.h`/`.cpp` — `DeviceProfile` (`name`, `DeviceKind`,
  `LayoutSize`, `SafeAreaInsets`) y `DeviceProfiles()` formalizan
  "device profiles" (sección 31: perfiles `Desktop`/`Tablet`/`Mobile`/
  `Custom`). Como `kDevicePresets` vive sin exportar dentro de
  `designer_canvas.cpp`, `DeviceProfiles()` es un arreglo nuevo que
  repite esos mismos siete nombres/tamaños (no se inventaron
  resoluciones distintas) en vez de una reutilización directa — no hay
  forma de incluir un `constexpr` de un `.cpp` de `panels/` desde
  `designer/` sin invertir la dependencia. Cada entrada se clasificó en
  un `DeviceKind` (`Responsive` para el 0×0 existente, que no encaja en
  ninguno de los cuatro perfiles de la sección 31 y se documenta así en
  vez de forzarlo dentro de uno de ellos). `MakeCustomProfile(name,
  width, height)` cubre el perfil `Custom` que no tiene entrada fija en
  `kDevicePresets` hoy. Los `SafeAreaInsets` por perfil son datos nuevos
  y curados (valores de referencia habitualmente citados para cada
  clase de dispositivo — p. ej. 47/34 top/bottom para iPhone con notch,
  24/20 para iPad con Face ID) para el uso estático de vista previa del
  Designer, deliberadamente separados de `ava::platform::mobile::
  ISafeArea` (consulta en vivo, otra fuente, otro propósito): se
  reutiliza únicamente la forma del dato (`SafeAreaInsets`, alias
  directo de `ava::platform::mobile::SafeAreaInsets`), no el mecanismo
  de consulta. `ApplySafeArea(frame, insets)` reduce un `LayoutRect` por
  esos insets — la mitad "guía visual" que un futuro overlay de safe
  area consumirá, sin overlay real todavía (mismo patrón que
  `OverlayItemKind::DropIndicator` en Fase 7).
* `responsive.h`/`.cpp` (continuación) — `ResolveResponsiveStyle(styles,
  typeLower, viewportWidth, isPureLayoutContainer)` es el "breakpoints"
  que faltaba en el pipeline de diseño: parte de `styles.Resolve(...)`
  (la misma base que ya usa `RenderTheme`, sin cambiarla) y encima
  recorre `styles.Breakpoints()` en el mismo orden que
  `HTMLRenderer::EmitProjectResponsiveCSS` ya recorre para generar CSS,
  aplicando (`MergeOnto`) cada breakpoint cuyo `minWidthPx <=
  viewportWidth` — igual que un `@media (min-width: Xpx)` que sí
  coincide con el viewport real. No se ordenan los breakpoints por
  ancho: se preserva el orden de `Breakpoints()` a propósito, porque es
  el mismo orden en que el CSS generado los declara, y en CSS un
  breakpoint declarado después gana en empate de especificidad — copiar
  ese orden es lo que hace que el resultado en el Designer coincida con
  lo que un export Web real mostraría, en vez de inventar un criterio
  de "mayor ancho gana" que podría no coincidir si el archivo de estilos
  del proyecto no está ordenado ascendente.
* `CMakeLists.txt` con la fuente nueva.

Verificado con `g++ -fsyntax-only` sobre todo `designer/*.cpp`: compila
limpio.

Nada de esto está conectado todavía: `designer_canvas.cpp` sigue usando
su propio `kDevicePresets`/`DrawDevicePresetBar` y `BuildLiveRender`
sigue sin pasar ningún ancho de viewport a `RenderTheme::Apply`, así que
un breakpoint de proyecto sigue sin tener efecto visible en el canvas
del Designer aunque sí lo tendría en un export Web real — ese es
exactamente el gap que `ResolveResponsiveStyle` deja listo para cerrar
el día que `BuildLiveRender`/`RenderTheme` se extiendan para aceptarlo
(fuera del alcance de `designer/`, que no muta esas dos funciones
reales). Migrar el combo de `designer_canvas.cpp` a `DeviceProfiles()` y
dibujar el marco de dispositivo + guía de safe area con `ApplySafeArea`
queda, igual que el resto de fases, para cuando ese panel se reconstruya
sobre `DesignerSurface`.

## Advanced (Fase 11)

La sección 65 del documento de arquitectura excluye explícitamente un
"complex animation editor" y un "full responsive breakpoint editor" del
alcance inmediato, y la sección 64 lista Fase 11 como siete temas
(`themes`, `design tokens`, `animations`, `component extraction`,
`assets`, `accessibility tools`, `visual states`) sin pedir un editor
completo de cada uno. Igual que en fases anteriores, primero se
investigó qué de esto ya existe real en el repo antes de escribir nada
nuevo.

* **Themes / Design tokens** — `avalang::ui::ITheme`/`DefaultTheme`/
  `ThemeProvider` (`avaui/src/theme/`) ya son reales y completos, pero
  `ITheme` solo permite consultar un token por nombre (`Color(name)`/
  `Font(name)`, con fallback) — no hay forma de enumerar qué tokens
  existen, necesario para un futuro panel de tokens del Designer.
  `theme_tokens.h`/`.cpp` agrega `KnownDesignTokens()`, un catálogo con
  los 33 nombres de color y 12 de fuente reales copiados de
  `DefaultTheme.cpp` (no inventados) — mismo criterio que
  `DeviceProfiles()` en Fase 10 repitiendo `kDevicePresets`.
  `ResolveColorToken`/`ResolveFontToken` son wrappers finos sobre
  `ITheme::Color`/`Font`, solo para que el Designer no llame al tema
  directo.

* **Accessibility tools** — hallazgo principal: `avalang::ui::
  accessibility::AccessibilityTree::Create(ComponentTree*)`
  (`avaui/src/accessibility/`) ya es real y completo, y toma
  exactamente el mismo `ComponentTree` que ya usa `UiComponentTree` en
  `designer/types.h` — el gap no es el runtime de accesibilidad (ya
  cerrado en una fase anterior de `AvaUI_Universal_Multiplatform_
  Implementation_Plan.md`, no de este documento), es que nada en
  `designer/` lo usaba todavía. `accessibility_inspector.h`/`.cpp`
  agrega `BuildAccessibilityTree` (reenvía directo a `Create`, no
  reimplementa nada), `MissingLabelDiagnostics` (nodos con rol
  interactivo — Button/TextInput/CheckBox/RadioButton/ComboBox/Link —
  y `Label()` vacío) y `TabOrder` (nodos `Focusable`, ni `Disabled` ni
  `Hidden`, en el mismo orden de `AccessibilityTree::Nodes()`, que es
  orden de documento). `TextContrastRatio`/`MeetsWcagAA` reutilizan
  `ContrastRatio` de `theme_tokens.h` (fórmula de luminancia relativa
  WCAG 2.x estándar, sobre `avalang::ui::Color` vía el `common::
  ParseColor` ya existente, no un parser de hex nuevo) contra los
  umbrales 4.5:1 (texto normal) / 3:1 (texto grande) de WCAG AA.

* **Animations** — `parser::AnimationSpec`, `theme::
  ProjectAnimationSheet`, `animation::AnimationController`/`Easing`/
  `AnimationBinding::WireAnimations` (`avaui/src/animation/`,
  `avaui/src/theme/`) ya son reales y completos para reproducir
  animaciones en tiempo de ejecución. Gap real encontrado: ningún
  `AnimationSpec` llega nunca a `DesignDocument` — solo existen a nivel
  de AST del parser y se consumen directo en el pipeline de
  construcción del árbol en vivo (`WireAnimations`), así que hoy no hay
  forma de listar "las animaciones de este nodo" desde el Designer sin
  tocar `DesignDocument` (fuera de alcance, mismo criterio que
  `BuildLiveRender` en Fase 9/10). `animation_authoring.h`/`.cpp` se
  queda deliberadamente en la capa que sí está disponible:
  `DescribeAnimation` (resumen legible de un `AnimationSpec` crudo),
  `GroupAnimationsByTarget` y `EasingLabel` (nombres legibles para el
  `EasingFunction` real) — listo para cuando `DesignDocument` exponga
  animaciones por nodo, sin inventar ese mecanismo ahora.

* **Visual states** — confirmado en `avaui/src/commands/
  SceneCommandWalker.cpp` (`ActiveInteractiveState`) que los 4 estados
  reales son `"hover"`, `"focus"`, `"active"` y `"disabled"` (no
  "pressed", como podría sugerir el nombre común), con prioridad
  disabled > active > focus > hover, y que hoy el override de estado
  solo se aplica a los tipos `"button"` y `"link"` en el path desktop
  nativo (mismo gap ya documentado en la Fase 10 previa del plan de
  AvaUI: TextBox/ComboBox/CheckBox/RadioButton no lo reciben ahí).
  `visual_states.h`/`.cpp` agrega `DesignerVisualState` (enum con esos
  4 estados + `Default`) y `ResolveVisualState`, que reproduce
  exactamente el mismo merge que `SceneCommandWalker::
  ResolveInteractiveOverride` hace en tiempo real (`Resolve()` seguido
  de `ResolveState(...).MergeOnto(...)`) pero para un estado elegido
  explícitamente en vez del que resulte de la posición real del mouse
  — el contrato que un futuro selector "Design-time state: Hover ▾" en
  el Properties Panel necesitaría, sin tener que mover el mouse de
  verdad para ver cómo se vería un botón deshabilitado.

* **Assets** — la sección 54 pide un "Asset Browser" solo
  "posteriormente"; se encontró que `avaui/src/resources/
  IResourceProvider.h`/`ResourceProvider.cpp` ya tiene un
  `ResourceType` real (`Font`/`Image`/`Icon`/`Localization`) con listas
  de extensión reales (`.ttf/.otf/.fon`, `.png/.bmp/.jpg/.jpeg/.gif`,
  `.lang/.txt`) — `asset_catalog.h` reusa ese mismo `ResourceType`
  como `AssetKind` (alias, no un enum paralelo) en vez de inventar uno
  con las categorías textuales del documento (`images/icons/fonts/
  audio/resources`). Nota honesta: `ResourceType` no tiene `Audio` —
  el documento lo menciona como categoría deseada pero no existe en
  ningún lado del runtime real, así que no se inventó aquí tampoco.
  `ClassifyAssetPath` clasifica por extensión con las mismas listas
  reales de `ResourceProvider.cpp` (Image e Icon comparten extensión en
  el sistema real, así que tampoco se distinguen aquí — el propio
  `ResourceProvider::FindFileWithExtensions` depende de que el
  llamador pase el `ResourceType` explícito, no lo infiere). Sin
  Browser de UI, como pide la sección 54 para "posteriormente".
  `IsRelativeAssetPath` valida la regla de la sección 54 ("nunca rutas
  absolutas"): rechaza `/...`, `\...`, `C:...` y esquemas tipo
  `http://`.

* **Component extraction** — hallazgo principal: no existe ningún
  mecanismo de registro de componentes definidos por el usuario en
  todo el repo — `ComponentRegistry` (Fase 2) es un singleton con una
  lista fija `entries_` de tipos built-in, sin ninguna API para
  registrar uno nuevo en tiempo de ejecución. Convertir un subtree
  extraído en un tipo reusable de verdad (la sección 42 completa)
  necesitaría ese registro, que no existe — implementarlo ahora sería
  inventar arquitectura nueva no descrita en el documento, el mismo
  tipo de exceso de alcance que la sección 65 pide evitar. Se entrega
  solo la mitad que sí es honesta hoy: `component_extraction.h`/`.cpp`
  con `ValidateExtractionCandidate(tree, nodeId)`, que reutiliza
  `studio::design::FindNodeById` (ya real, mismo helper que usan todos
  los `lifecycle_commands.cpp`) y `ComponentRegistry::Instance().Find`
  para rechazar: nodo inexistente, la raíz del documento, o un tipo con
  `designerCapabilities.canDuplicate == false` — la validación que un
  futuro ítem de menú "Create Component" necesitaría para habilitarse/
  deshabilitarse con motivo, sin implementar el comando de extracción
  en sí hasta que exista dónde guardar la definición resultante.

Nuevo, en `runtime/avastudio/src/designer/`: `theme_tokens.h`/`.cpp`,
`visual_states.h`/`.cpp`, `animation_authoring.h`/`.cpp`,
`accessibility_inspector.h`/`.cpp`, `asset_catalog.h`/`.cpp`,
`component_extraction.h`/`.cpp`, y `CMakeLists.txt` (de
`runtime/avastudio/`) actualizado con los 6 `.cpp` nuevos.

Verificado con `g++ -fsyntax-only` sobre los 6 archivos nuevos contra
los headers reales del repo (`ITheme.h`, `ProjectStyleOverrides.h`,
`AvauiParser.h`, `AccessibilityTree.h`, `IResourceProvider.h`,
`design_document.h`, `component_registry.h`): todos compilan limpio,
sin necesitar antlr4-runtime ni glm vendorizados para este subconjunto.

Como siempre: nada de esto está conectado a ninguna pantalla de
AvaStudio todavía (no hay panel de tokens, ni inspector de
accesibilidad, ni lista de animaciones, ni selector de estado de
diseño, ni browser de assets, ni ítem de menú de extracción) — son los
contratos formales, listos para cuando esos paneles se construyan
sobre `DesignerSurface`, igual que el resto de `designer/`.

## Contenedores vacíos en Design

En el runtime un contenedor sin hijos (`Container`, `Column`, `Row`, `Stack`,
`Grid`, `Flex`, `ScrollView`, `ListView`, `Page`) mide 0 de alto, así que al
arrastrarlo al canvas no se veía ni se podía usar como destino de otro
arrastre. `LayoutEngine::SetEmptyContainerMinSize` reserva un tamaño mínimo
solo para contenedores vacíos; `BuildLiveRender` lo activa
(`reserveEmptyContainerSpace`, 120x56) únicamente en modo Design del canvas.
Preview, exportación y el runtime no lo activan, por lo que su layout no
cambia. Un `width`/`height` explícito siempre prevalece sobre el mínimo.

`Dialog` no se dibuja en el canvas: se lista en la bandeja de diálogos
(`DrawDialogTray`).
