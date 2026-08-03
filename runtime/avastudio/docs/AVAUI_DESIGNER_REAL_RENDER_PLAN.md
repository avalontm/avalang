# Plan: Design panel de Ava Studio renderiza con AvaUI real

Objetivo: que el panel Design (`designer_canvas.cpp`) deje de dibujar
controles "a mano" (estilo Win32/VB6 hardcodeado en `DrawRealWidget`) y
en su lugar corra el **mismo pipeline** que usa el runtime real
(avahost / export), para que lo que se ve en el IDE mientras se arma un
`.avaui` sea 1:1 con lo que se va a exportar. El drag&drop, selección y
demás interacción tipo VB6 (Fase 4-8 ya implementadas) se MANTIENEN
igual -- solo cambia de dónde salen las rects y cómo se pinta cada
nodo.

Este documento existe para que una sesión futura (con contexto
limpio) pueda ejecutar el plan sin tener que releer todo el repo.
Cada fase dice: qué archivos tocar, qué hacer exactamente, y cómo
verificar que quedó bien antes de pasar a la siguiente.

**No compilar en cada paso micro** -- Rulas compila localmente. Sí
parar y avisar al final de cada Fase (son puntos de verificación
naturales).

---

## Arquitectura ya confirmada (no hace falta re-investigar)

Pipeline real (igual al que usa `avahost/src/rendering/ui_pipeline_static_renderer.cpp`):

```
DesignNode tree (studio)
  -> ComponentTree/IComponent   (avaui/src/components/)
  -> RenderTheme::Apply(tree, theme)   (avaui/src/theme/RenderTheme.h)
  -> LayoutEngine::Compute(root, viewport) -> ILayoutNode tree  (avaui/src/layout/)
  -> IRenderTree::Build(root, layoutEngine) -> IRenderNode tree (avaui/src/render_tree/)
  -> ISceneGraph::Build(renderRoot); UpdateTransforms()          (avaui/src/scene/)
  -> SceneCommandWalker::Walk(scene, sink, renderer)             (avaui/src/commands/)
  -> IRenderer (HTMLRenderer / GdiRenderer / **ImGuiRenderer nuevo**)
```

`IRenderer` es abstracto (`avaui/src/renderer/IRenderer.h`), con
`BaseRenderer` (`avaui/src/renderer/BaseRenderer.h/.cpp`) ya resolviendo
transform stack / clip stack / opacity -- un backend nuevo SOLO
implementa los `OnDraw*` primitivos (`OnDrawRectangle`, `OnDrawEllipse`,
`OnDrawText`, `OnDrawImage`, `OnDrawButton`, `OnDrawHtmlFragment`).
`GdiRenderer.cpp` (mismo directorio) es la plantilla exacta a copiar
pero usando `ImDrawList` en vez de GDI -- ya lo miramos línea por
línea, no hace falta reabrirlo para redescubrir la interfaz.

**Decisión de integración** (confirmada con Rulas): NO se serializa a
texto `.avaui` y se re-parsea con `AvauiParser` en cada frame. En vez
de eso, se construye el `ComponentTree` **directamente** desde
`design::DesignNode` (bridge nuevo, ver Fase 2), guardando un mapa
`node_uid -> ComponentId`. Motivo: evita duplicar/desincronizar el
formato de texto, y da correlación 1:1 exacta y barata para
hit-testing/selección -- no hace falta reconstruir el layout/drag&drop
actual de `designer_canvas.cpp` (Fase 4-8), solo alimentarlo con las
rects que salen de `ILayoutNode::Rect()` en vez de las que hoy calcula
`design::ComputeLayout`.

`DesignNode::properties`/`events` ya son `vector<PropertyRow>` con
`{key, value}` en string "listo para mostrar" (`properties_panel.h`).
La inferencia de tipo (String/Bool/Number) y los alias de nombre
(`gap`->`spacing`, `value`->`text`) que usa `AvauiParser.cpp` para
convertir esos strings a `PropertyValue` están siendo extraídos a un
header compartido (Fase 1) para que el bridge nuevo use EXACTAMENTE
la misma lógica -- si no, Design panel y export podrían interpretar el
mismo valor distinto y desviarse con el tiempo.

---

## Fase 1 -- Extraer coerción de propiedades a header compartido

**Ya implementado, solo aplicar.** Archivos:

- **Crear** `runtime/avaui/src/parser/AvauiPropertyCoercion.h`
- **Crear** `runtime/avaui/src/parser/AvauiPropertyCoercion.cpp`

  Contenido: ver estos dos archivos en el ZIP adjunto a este plan
  (`avaui_property_coercion/`). Exponen (namespace
  `avalang::ui::parser`, `AVA_UI_API`):
  - `std::string Unquote(const std::string&)`
  - `bool LooksLikeNumber(const std::string&, double*)`
  - `PropertyValue InferValue(const std::string&)`
  - `std::string CanonicalTypeName(const std::string&)`
  - `void SetPropertyWithAlias(IComponent*, const std::string&, const PropertyValue&)`

  Es el código que ya vivía en el namespace anónimo de
  `AvauiParser.cpp` (líneas ~110-192), copiado tal cual -- ningún
  comportamiento cambia.

- **Modificar** `runtime/avaui/src/parser/AvauiParser.cpp`:
  1. Agregar `#include "parser/AvauiPropertyCoercion.h"` arriba.
  2. Borrar del namespace anónimo (líneas ~110-192 en el original):
     las definiciones de `Unquote`, `LooksLikeNumber`, `InferValue`,
     `kPropertyAliases`, `kTypeNames`, `CanonicalTypeName`,
     `SetPropertyWithAlias`. (Dejar intactos: `Line`, `StripComment`,
     `Trim`, `Tokenize`, `IsPropertyLine`, `SplitProperty`,
     `IsComponentCall`, `IsAnimateHeader` -- esos son puros del
     tokenizer/parser, no se tocan).
  3. Todo lo demás del archivo (`ParseComponent`, etc.) sigue
     compilando igual porque los nombres ahora vienen del `using`
     implícito del mismo namespace `avalang::ui::parser` (no hace
     falta calificar las llamadas).

- **Modificar** `runtime/avaui/CMakeLists.txt`: agregar
  `src/parser/AvauiPropertyCoercion.cpp` a la lista de fuentes
  (`AVA_UI_SOURCES`, buscar dónde está listado
  `src/parser/AvauiParser.cpp` y agregar la línea justo al lado).

**Verificación:** compila `avalang_ui` igual que antes, sin cambios de
comportamiento (es un refactor puro).

---

## Fase 2 -- Bridge: DesignNode -> ComponentTree + pipeline + mapa de rects

**Crear** `runtime/avastudio/src/design/live_render_bridge.h`
**Crear** `runtime/avastudio/src/design/live_render_bridge.cpp`

Responsabilidad única: dado un `design::DesignNode` (subárbol,
normalmente `doc.root`) y un viewport `width/height`, correr el
pipeline real de avaui hasta Scene Graph y devolver:

```cpp
struct LiveRenderResult {
    std::unique_ptr<avalang::ui::ComponentTree> componentTree;
    std::unique_ptr<avalang::ui::LayoutEngine> layoutEngine; // dueño de los ILayoutNode
    std::unique_ptr<avalang::ui::render::IRenderTree> renderTree;
    std::unique_ptr<avalang::ui::scene::ISceneGraph> sceneGraph;

    // node_uid (DesignNode) -> ComponentId (avaui). Clave para que
    // designer_canvas.cpp siga haciendo hit-test/selección/drag&drop
    // por node_uid como hoy, pero contra las rects reales de avaui.
    std::unordered_map<std::string, avalang::ui::ComponentId> uidToComponentId;

    // Atajo ya resuelto: node_uid -> LayoutRect (de ILayoutNode::Rect()).
    // Esto es lo que designer_canvas.cpp va a consumir en la mayoría
    // de los casos (Fase 3) en vez de recorrer ILayoutNode a mano.
    std::unordered_map<std::string, avalang::ui::LayoutRect> uidToRect;

    bool ok = false;
    std::string error;
};

// Construye TODO el pipeline (ComponentTree -> Theme -> Layout ->
// RenderTree -> SceneGraph) a partir de `root`. No dibuja nada -- ver
// designer_canvas.cpp / SceneCommandWalker para el paso de pintado.
// `root` normalmente es doc.root; para nodos "synthetic" (subárbol de
// un import resuelto por ComponentResolver, ver designer_canvas.h)
// llamar con ESE root en una construcción aparte -- no hay problema en
// tener dos LiveRenderResult vivos a la vez (cada uno con su propio
// ComponentTree), igual que hoy conviven doc.root y el árbol resuelto
// del import.
LiveRenderResult BuildLiveRender(const design::DesignNode& root, int viewportWidth,
                                 int viewportHeight);
```

Implementación de `BuildLiveRender` (pseudocódigo fiel a lo que hace
`ui_pipeline_static_renderer.cpp`, adaptado para construir el
`ComponentTree` directo en vez de parsear texto):

```cpp
LiveRenderResult BuildLiveRender(const design::DesignNode& root, int vw, int vh) {
    LiveRenderResult out;
    out.componentTree = avalang::ui::ComponentTree::Create();

    // --- 1. DesignNode -> IComponent, recursivo, guardando el mapa ---
    std::function<avalang::ui::IComponent*(const design::DesignNode&)> build =
        [&](const design::DesignNode& n) -> avalang::ui::IComponent* {
        using namespace avalang::ui;
        IComponent* comp = out.componentTree->CreateComponent(
            parser::CanonicalTypeName(n.type));
        out.uidToComponentId[n.node_uid] = comp->Id();

        if (!n.id.empty()) comp->SetProperty("id", PropertyValue(n.id));
        for (const auto& p : n.properties) {
            parser::SetPropertyWithAlias(comp, p.key, parser::InferValue(p.value));
        }
        // events (n.events, key="click" etc.): mismo tratamiento que
        // el parser real le da a una property mas -- ver como
        // AvauiParser.cpp / RenderTree.cpp leen "click" (es solo otra
        // propiedad string con el nombre del handler). Setear cada
        // n.events[i] como property `ev.key` = PropertyValue(ev.value).
        for (const auto& ev : n.events) {
            comp->SetProperty(ev.key, PropertyValue(ev.value));
        }

        for (const auto& child : n.children) {
            comp->AddChild(build(child));
        }
        return comp;
    };
    out.componentTree->SetRoot(build(root));

    // --- 2. Theme (igual que ui_pipeline_static_renderer.cpp) ---
    auto themeProvider = std::unique_ptr<avalang::ui::IThemeProvider>(
        avalang::ui::CreateDefaultThemeProvider());
    avalang::ui::RenderTheme::Apply(out.componentTree.get(), themeProvider->Current());

    // --- 3. Layout ---
    out.layoutEngine = avalang::ui::LayoutEngine::Create();
    avalang::ui::LayoutRect viewport{0.0, 0.0, (double)vw, (double)vh};
    avalang::ui::ILayoutNode* layoutRoot =
        out.layoutEngine->Compute(out.componentTree->Root(), viewport);
    if (!layoutRoot) { out.error = "LayoutEngine::Compute failed"; return out; }

    // --- 4. Render Tree ---
    out.renderTree.reset(avalang::ui::render::IRenderTree::Create());
    out.renderTree->Build(out.componentTree->Root(), out.layoutEngine.get());
    if (!out.renderTree->Root()) { out.error = "IRenderTree::Build failed"; return out; }

    // --- 5. Scene Graph ---
    out.sceneGraph.reset(avalang::ui::scene::ISceneGraph::Create());
    out.sceneGraph->Build(out.renderTree->Root());
    out.sceneGraph->UpdateTransforms();

    // --- 6. uidToRect: recorrer ILayoutNode y cruzar con uidToComponentId ---
    // (invertir el mapa id->uid una vez, después recorrer todo el
    // arbol de ILayoutNode con Children()/Rect() y llenar uidToRect).
    std::unordered_map<avalang::ui::ComponentId, std::string> idToUid;
    for (auto& kv : out.uidToComponentId) idToUid[kv.second] = kv.first;
    std::function<void(avalang::ui::ILayoutNode*)> walk = [&](avalang::ui::ILayoutNode* ln) {
        if (!ln) return;
        auto it = idToUid.find(ln->Id());
        if (it != idToUid.end()) out.uidToRect[it->second] = ln->Rect();
        for (auto* c : ln->Children()) walk(c);
    };
    walk(layoutRoot);

    out.ok = true;
    return out;
}
```

Notas de implementación real (no pseudocódigo, cuidar al escribir el
`.cpp` de verdad):

- Includes necesarios (mirar `ui_pipeline_static_renderer.cpp` como
  referencia exacta de paths): `components/ComponentTree.h`,
  `parser/AvauiPropertyCoercion.h`, `theme/ITheme.h`,
  `theme/RenderTheme.h`, `layout/LayoutEngine.h`,
  `render_tree/IRenderTree.h`, `scene/ISceneGraph.h`,
  `design/design_document.h`.
- `n.events`: confirmar en `RenderTree.cpp`/`SceneCommandWalker.cpp`
  bajo qué nombre de propiedad se lee el handler de click (buscar
  `ClickHandler()` en `IRenderNode.h` y su implementación en
  `RenderNode.cpp` para saber el nombre exacto de property que espera
  -- probablemente `"click"` tal cual, pero VERIFICAR antes de
  asumirlo, capaz espera `"onClick"` u otro nombre y hay que mapear
  `ev.key` a ese nombre igual que hace `RenderTree.cpp` para el
  parser real).
- Igual para `style` (bloque nuevo de AvaUI, ver memoria de
  sesiones previas: `ava_ui_set_style`/`ava_ui_get_style` recién
  agregado). Si `DesignNode` todavía no tiene un lugar propio para las
  properties del bloque `style` (revisar `design_document.h` de nuevo
  en esa sesión si hace falta -- al momento de este plan solo tiene
  `properties`/`events`), decidir: o el bloque `style` ya cae dentro
  de `n.properties` con las mismas keys que usa el renderer
  (`background`, `borderRadius`, etc. -- las que lee `IRenderNode`), o
  hay que sumarle un campo `style` a `DesignNode` primero. Este punto
  quedó pendiente de confirmar -- no asumir, revisar
  `17_AVAUI_FILE_FORMAT.md`/`AVAUI_ARCHITECTURE_v1_0.md` y
  `design/avaui_text.cpp` (el que ya lee/escribe el bloque `style` al
  cargar/guardar) para ver dónde termina esa data hoy en `DesignNode`.
- `synthetic` (nodo dentro de un `Componente()` importado resuelto por
  `ComponentResolver`, ver `designer_canvas.h`): ese subárbol YA es un
  `DesignNode` aparte (el resuelto, "throwaway copy" con uids frescos,
  según el comentario de `designer_canvas.h`) -- se le corre
  `BuildLiveRender` propio, igual que hoy se le corre `ComputeLayout`
  aparte. No requiere cambios en el bridge.

**Modificar** `runtime/avastudio/CMakeLists.txt`:
- Agregar `src/design/live_render_bridge.cpp` a `STUDIO_SOURCES`.
- Agregar `target_link_libraries(ava_studio PRIVATE avalang imgui_docking text_editor avalang_ui)`
  (agregar `avalang_ui` -- hoy NO está linkeado).
- Agregar a `target_include_directories(ava_studio PRIVATE ...)`:
  `${CMAKE_SOURCE_DIR}/runtime/avaui/src` (para que
  `#include "components/ComponentTree.h"` etc. resuelvan desde
  avastudio igual que resuelven desde avahost).

**Verificación:** este archivo compila solo (sin tocar
`designer_canvas.cpp` todavía) -- se puede probar con un `.cpp` de
prueba suelto o directamente seguir a la Fase 3 y verificar todo junto.

---

## Fase 3 -- ImGuiRenderer (backend nuevo de IRenderer)

**Crear** `runtime/avastudio/src/design/imgui_renderer.h`
**Crear** `runtime/avastudio/src/design/imgui_renderer.cpp`

Copiar la estructura de `runtime/avaui/src/renderer/GdiRenderer.h/.cpp`
tal cual (misma clase `: public avalang::ui::BaseRenderer`, mismos
métodos `OnDraw*` a implementar), cambiando las llamadas GDI por
`ImDrawList`:

```cpp
// imgui_renderer.h
#pragma once
#include "renderer/BaseRenderer.h"
#include "imgui.h"

namespace avalang::ui {

// Backend de IRenderer para Ava Studio: en vez de pintar a una
// ventana Win32 (GdiRenderer) o generar HTML (HTMLRenderer), vuelca
// los mismos RenderCommand a un ImDrawList ya abierto -- para que el
// panel Design pinte EXACTAMENTE lo mismo que exportaria el pipeline
// real, con controles reales de ImGui debajo.
//
// Vive en avastudio (no en avaui/src/renderer/) a proposito: es el
// unico backend con dependencia a ImGui, y avaui core no debe
// depender de ImGui (ver Fwd.h -- renderer/ esta al fondo del grafo de
// dependencias, pero NADA dice que un backend de renderer tenga que
// vivir dentro de la propia libreria avaui; GdiRenderer si vive ahi
// porque WinAPI ya es una dependencia de avaui en Windows -- ImGui no
// lo es).
class ImGuiRenderer : public BaseRenderer {
public:
    ImGuiRenderer(int width, int height);
    ~ImGuiRenderer() override = default;

    // Debe llamarse ANTES de cada Walk() -- el ImDrawList del frame
    // actual (normalmente ImGui::GetWindowDrawList() del canvas) y el
    // origen en screen-space (p0 del rect del canvas) al que sumar
    // cada coordenada del pipeline (que son relativas al viewport
    // 0,0). BaseRenderer no sabe nada de ImGui, por eso esto vive acá
    // y no en el pipeline.
    void SetTarget(ImDrawList* drawList, ImVec2 origin);

protected:
    void OnDrawRectangle(float x, float y, float width, float height,
                         const Color& fillColor, const Color& borderColor,
                         float borderWidth, float borderRadius,
                         const std::string& clickHandler,
                         const std::string& className) override;
    void OnDrawEllipse(float cx, float cy, float rx, float ry,
                       const Color& fillColor, const Color& borderColor,
                       float borderWidth, const std::string& clickHandler,
                       const std::string& className) override;
    void OnDrawText(float x, float y, const char* text, float fontSize,
                    const char* fontName, const Color& color,
                    const std::string& clickHandler,
                    const std::string& className) override;
    void OnDrawImage(float x, float y, float width, float height,
                     const char* imagePath) override;
    void OnDrawHtmlFragment(const std::string& html) override;
    void OnDrawButton(float x, float y, float width, float height,
                      const char* text, float fontSize, const char* fontName,
                      const Color& textColor, const Color& fillColor,
                      const Color& borderColor, float borderWidth,
                      float borderRadius, bool disabled,
                      const std::string& clickHandler,
                      const std::string& className) override;

private:
    ImDrawList* drawList_ = nullptr;
    ImVec2 origin_{0, 0};

    ImVec2 P(float x, float y) const { return ImVec2(origin_.x + x, origin_.y + y); }
    static ImU32 ToImU32(const Color& c) { return IM_COL32(c.r, c.g, c.b, c.a); }
};

} // namespace avalang::ui
```

Puntos a resolver al escribir el `.cpp` (mirar `GdiRenderer.cpp` línea
por línea como plantilla, ya lo tenemos leído en esta sesión):

- `OnDrawRectangle`: `drawList_->AddRectFilled(P(x,y), P(x+w,y+h), ToImU32(fillColor), borderRadius)`
  + si `borderWidth > 0`, `drawList_->AddRect(...)` encima con
  `borderColor`/`borderWidth`. `class_Name`/`clickHandler` se ignoran
  igual que en `GdiRenderer` (mismo comentario: solo HTMLRenderer usa
  `class=`).
- `OnDrawEllipse`: `drawList_->AddEllipseFilled(P(cx,cy), rx, ry, ToImU32(fillColor))`
  + `AddEllipse` para el borde si `borderWidth>0`.
- `OnDrawText`: fuente -- ImGui no tiene "cargar fuente por nombre en
  runtime" trivial; para el primer corte usar
  `ImGui::GetFont()`/`ImGui::GetFontSize()` escalado a `fontSize` via
  `drawList_->AddText(font, fontSize, P(x,y), ToImU32(color), text)`
  (parámetro `fontName` se ignora por ahora -- dejar comentado el gap,
  mismo estilo que el resto del código: "desktop stub" documentado,
  no silencioso).
- `OnDrawImage`: reusar `GetOrLoadImagePreview`/`ResolveImageSrcPath`
  que YA existen en `designer_canvas.cpp` (Fase 10 actual, decodificado
  con stb_image) -- no reinventar carga de imagen. Puede hacer falta
  moverlas a un header compartido si `imgui_renderer.cpp` las necesita
  y hoy son estáticas/anónimas dentro de `designer_canvas.cpp` --
  revisar y exponerlas si es necesario.
- `OnDrawButton`: igual que `GdiRenderer::OnDrawButton` -- dibuja el
  rect (reusa `OnDrawRectangle`) y centra el texto encima
  (`ImGui::CalcTextSize` en vez de `GetTextExtentPoint32A`).
- `OnDrawHtmlFragment`: no-op (igual que `GdiRenderer`).
- Constructor: `BaseRenderer(width, height)`, sin más estado (no hay
  back buffer como GDI -- ImGui ya tiene su propio doble buffer).
- `OnBeginFrame`/`OnEndFrame`: no hace falta overridearlos (BaseRenderer
  los deja como no-op virtual) salvo que se necesite limpiar algo por
  frame -- probablemente no.

**Modificar** `runtime/avastudio/CMakeLists.txt`: agregar
`src/design/imgui_renderer.cpp` a `STUDIO_SOURCES`.

**Verificación:** compila junto con la Fase 2. Todavía no está
conectado a `designer_canvas.cpp` -- eso es la Fase 4.

---

## Fase 4 -- Conectar todo en designer_canvas.cpp

**Estado: 4.1, 4.2, 4.3 y 4.4 implementadas** (sesión con contexto
limpio, ver historial). Pendiente: compilar localmente y la
verificación manual descrita al final de esta sección (abrir un
`.avaui` real, comparar contra Export/avahost, probar drag&drop/
selección/Ctrl+Click/doble-click/delete).

Este es el paso de mayor riesgo (toca el archivo de 1529 líneas con
toda la interacción VB6 ya afinada: selección, drag&drop Fase 4,
Ctrl+Click Fase 6, doble-click Fase 5, cache por `tab_id`). Hacerlo
en sub-pasos, cada uno verificable por separado:

### 4.1 -- Cache de LiveRenderResult por tab_id

Ya existe un cache module-local por `tab_id` en
`designer_canvas.cpp` (mencionado en el header: "cachear
state_vm/evaluación", "cachear ComponentResolver/resolved_root",
invalidado cuando `doc.dirty` cambia o `project_root` cambia). Sumar
un tercer elemento a esa misma cache entry (buscar el struct
`DesignerVmCacheEntry` mencionado en el header):

```cpp
// dentro de DesignerVmCacheEntry (o la struct que exista con ese rol)
LiveRenderResult live_render; // Fase 2 -- rebuilt junto con lo demás
                              // cuando doc.dirty cambia o el tamaño
                              // del canvas cambia (viewport distinto
                              // => hay que recomputar layout)
ImGuiRenderer imgui_renderer{0, 0}; // reconstruido junto con live_render
```

Regla de invalidación: reconstruir `live_render` (llamando
`BuildLiveRender(doc.root, canvas_w, canvas_h)`) cuando:
- `doc.dirty` (igual que hoy invalida el resto del cache), O
- el tamaño del canvas cambió desde el último build (nuevo -- hoy
  `ComputeLayout` seguramente ya recibe `size` como parámetro y lo
  recalcula cada frame sin cachear layout; con el pipeline real, correr
  Layout en cada frame también es aceptable si el árbol es chico -- si
  se nota lento, cachear por tamaño también, pero arrancar simple).

### 4.2 -- Reemplazar DrawRealWidget por SceneCommandWalker::Walk

Buscar el call site donde hoy se itera nodo por nodo llamando algo
como `DrawRealWidget(node, ..., p0, p1, ...)` dentro del recorrido de
`DrawDesignerCanvas` (el `if (node.type == "button") ImGui::Button(...)`
etc. que ya leímos, líneas ~559-650 del archivo original). Reemplazar
ESE recorrido de pintado por:

```cpp
cacheEntry.imgui_renderer.SetTarget(ImGui::GetWindowDrawList(),
                                     canvas_origin_screen_pos); // p0 del canvas completo
avalang::ui::RenderCommandSink sink;
avalang::ui::SceneCommandWalker::Walk(*cacheEntry.live_render.sceneGraph, sink,
                                       cacheEntry.imgui_renderer);
```

Esto pinta el árbol ENTERO de una sola pasada (ya no nodo por nodo con
un switch de tipo). El recorrido nodo-por-nodo que quedaba en
`designer_canvas.cpp` (el que hoy hace hit-test/selección/drag&drop
banda por banda) se mantiene EXACTAMENTE igual que hoy, pero:
- en vez de leer su rect de `design::ComputeLayout(...)`, la lee de
  `cacheEntry.live_render.uidToRect[node.node_uid]` (Fase 2).
- ya NO llama a `DrawRealWidget` (ese pintado ahora lo hizo el Walk de
  arriba, antes de este recorrido de overlay) -- el recorrido restante
  solo dibuja el rectángulo de selección/hover/drop-zone TRANSPARENTE
  encima (invisible salvo cuando hay selección/hover), usando la
  MISMA rect.

Esto significa: **el orden de pintado por frame pasa a ser**
1. `SceneCommandWalker::Walk(...)` -- pinta todos los controles reales.
2. El loop existente de `designer_canvas.cpp` -- ya no pinta controles,
   solo overlays de selección/drag&drop/handles, usando
   `uidToRect[node.node_uid]` en vez de su propio `ComputeLayout`.

### 4.3 -- Reemplazar `design::ComputeLayout` como fuente de rects

Buscar cada lugar donde el código actual llama a
`design::ComputeLayout` (o como se llame la función/estructura de
layout propio -- confirmar el nombre exacto grep-eando
`ComputeLayout` en `designer_canvas.cpp` y en
`design/design_document.h`/`.cpp`, dado que no apareció explícitamente
en los fragmentos ya leídos en esta sesión -- podría estar en un
archivo de layout propio de Ava Studio no revisado todavía, p.ej.
`design/layout.h`. **Primer paso de la Fase 4: localizar ese archivo
antes de tocar nada** con
`grep -rn "ComputeLayout" runtime/avastudio/src/`.)

Una vez localizado: cada lookup de rect por `node_uid` que hoy hace
ese motor de layout propio pasa a hacer
`cacheEntry.live_render.uidToRect[node_uid]` en su lugar (con manejo de
"no encontrado" para nodos `synthetic`, que tienen su propio
`LiveRenderResult` aparte, ver nota de Fase 2).

Todo lo demás (hit-testing por `ImGui::IsMouseHoveringRect`,
`HandleDropTarget`, bandas top/bottom para `kBefore`/`kAfter`/`kInto`
de `MoveNode`, el popup de confirmación de delete, el jump-to-code de
Fase 5) sigue funcionando igual porque solo dependía de tener "una
rect por `node_uid`" -- eso no cambia de forma, solo de origen.

### 4.4 -- Ctrl+Click / Fase 6 (ejecutar handler contra el state VM)

No debería requerir cambios: sigue usando el `AvaVM` cacheado de
`design::BuildStateVM`/`design::BindCodeBehind` (independiente del
pipeline de render). Solo verificar que, tras mutar `state`, el
`live_render` (Fase 2) también se invalide/reconstruya si algún
`display prop` depende de `state` (bindings de dos vías) -- si el
pipeline real ya lee `state` como parte de `IComponent`/properties, hay
que decidir CÓMO entra `state` al `ComponentTree` del bridge (hoy
`BuildLiveRender` en la Fase 2 NO contempla bindings de state todavía
-- pendiente explícito, ver "Fase 5" abajo).

**Verificación de la Fase 4 completa:** abrir un `.avaui` existente
con botones/checkboxes/texto, confirmar que:
- se ven con los mismos colores/fuente/border-radius que el HTML
  exportado (comparar con "Export" o con `avahost` sirviendo el mismo
  archivo).
- drag&drop de Toolbox, mover nodos (Fase 4), seleccionar (click),
  Ctrl+Click (Fase 6), doble-click (Fase 5) y delete (Fase 8) siguen
  funcionando igual que antes de este cambio.

---

## Fase 5 -- Pendiente / fuera de este plan (anotar, no implementar todavía)

- **State bindings en el preview real**: `BuildLiveRender` (Fase 2)
  hoy solo copia `n.properties` literal -- no resuelve expresiones tipo
  `text = counter` contra `doc.initial_state`/el `AvaVM` cacheado de
  Fase 6. El pipeline real de avahost para esto es
  `ui_pipeline_dynamic_renderer.cpp` (visto en el grep de la Fase 0 de
  investigación, no leído en detalle todavía) -- revisar ese archivo
  como referencia de "cómo se bindea state real" antes de intentar que
  el Design panel muestre valores de state evaluados en vivo. Hasta
  entonces, un nodo con `text = counter` va a mostrar el texto literal
  `"counter"` en el panel Design (mismo gap que ya documentaba
  `InferValue`, "no evalúa expresiones").
- **Bloque `style` del nodo -> dónde vive en `DesignNode`**: ver nota
  de la Fase 2. Confirmar antes de asumir que `n.properties` ya trae
  esas keys.
- **Selección múltiple / resize handles**: no existen hoy (por lo que
  se vio en `designer_canvas.h`), no es parte de este plan.

---

## Orden de ejecución recomendado para la próxima sesión

1. Fase 1 (mecánica, bajo riesgo, refactor puro).
2. Fase 2 (archivo nuevo, no toca nada existente -- se puede probar
   con un `.avaui` de prueba chico antes de tocar `designer_canvas.cpp`).
3. Fase 3 (archivo nuevo, ídem).
4. Fase 4, en el orden 4.1 -> 4.2 -> 4.3 -> 4.4 (cada sub-paso es
   compilable/probable por separado -- no hacer los cuatro de un saque).
5. Anotar en este mismo doc lo que se decida para la Fase 5 cuando se
   llegue a esa parte, para la sesión después de esa.
