# AvaUI — Layout Engine

Ver `10_AVAUI.md` sección 5 para la decisión de fondo (el
algoritmo se promueve a `core/src/ui/`, operando sobre `Component`). Este
documento baja al detalle: qué hace el algoritmo hoy, tal como está
implementado, y cómo queda la firma nueva.

## 1. El algoritmo hoy (as-built), `studio/src/design/layout_engine.cpp`

Es corto (~90 líneas) y hace exactamente una cosa: recorre un
`DesignNode` y produce un `Rect` por cada nodo (`LayoutResult::rects`,
`node_uid → Rect`). No sabe nada de ImGui -- `Rect` es `{x, y, w, h}` en
`float` plano.

### 1.1 Los tres modos de contenedor

Dos funciones mutuamente recursivas: `EstimateHeight` (cuánto alto va a
ocupar un subárbol, sin todavía decidir posiciones) y `LayoutNode` (la
pasada real que sí asigna `x/y` y escribe en `result`). Un contenedor
tiene que saber el alto de cada hijo *antes* de poder apilarlos sin
superponerse -- de ahí que haga falta la pasada de estimación separada.

- **`row`**: reparte el ancho disponible en partes iguales entre los
  hijos (`width / children.size()`, sin wrap ni tamaños flexibles por
  hijo); el alto del row es el del hijo más alto.
- **`stack`**: todos los hijos reciben el mismo rect completo
  (overlap real, sin z-index explícito más allá del orden de dibujo);
  el alto del stack es el del hijo más alto.
- **Todo lo demás** (`column`, `page`, `grid`, `flex`, o cualquier tipo
  no reconocido): fallback a apilado vertical simple, ancho completo
  para cada hijo, alto = suma de los altos estimados de los hijos.

### 1.2 Leaves: altura fija, sin medición real

Un nodo sin hijos (o un container vacío) recibe siempre
`kDefaultLeafHeight = 32.0f`, sin importar su tipo ni contenido. No hay
noción de "tamaño natural" -- un `Text` con una línea y un `Text` con
tres líneas hoy ocupan el mismo alto. Esto es una limitación conocida y
documentada en el propio header (`layout_engine.h`): la medición real de
texto queda pendiente de que el canvas dibuje widgets reales (lo cual ya
pasó, Fase 10 de `docs/history/DESIGNER_VIEW_SESSIONS.md` -- pero el layout todavía no
se actualizó para usarla).

### 1.3 La raíz siempre ocupa todo el espacio disponible

`ComputeLayout(root, available_space)` llama `LayoutNode(root,
available_space, result)`, y `LayoutNode` escribe
`result.rects[node.node_uid] = rect` **antes** de mirar los hijos -- o
sea que el rect de la raíz es siempre exactamente el `available_space`
que le pasó el caller, sin importar cuánto midan sus hijos sumados. Esto
es intencional (`page` debe llenar el canvas incluso vacía) pero tiene
una consecuencia que ya se documentó del lado de `designer_canvas.cpp`:
el espacio entre el último hijo y el borde inferior real de `page` no
tiene rect propio -- por eso existe el `##node_body_drop_area` filler en
el canvas (ver el fix de hover-stealing de la sesión anterior).

### 1.4 Costo conocido, no arreglado acá

`LayoutNode`'s fallback de columna llama `EstimateHeight(child, ...)`
por cada hijo para saber dónde poner el siguiente -- y `EstimateHeight`
recorre el subárbol completo de ese hijo de nuevo (no memoiza nada). Es
decir, un árbol de profundidad *d* recalcula el subárbol de cada rama
más de una vez según cuántos ancestros columna tenga arriba. Para los
tamaños de árbol que maneja el Designer hoy (una pantalla a la vez) es
imperceptible; si esto se generaliza a documentos grandes conviene
memoizar `EstimateHeight` por `(node_uid, width)` igual que ya se hace
con `eval_cache` para las display-props (ver `designer_canvas.cpp`). No
es parte de este cambio -- queda anotado para no perderlo de vista.

## 2. Grid/Flex: no son layouts reales todavía

Ambos caen en el fallback de columna (sección 1.1). Esto es honesto en
el propio nombre del tipo pero no en el resultado visual -- un `grid`
con 4 hijos hoy se ve idéntico a un `column` con 4 hijos. No hay
decisión tomada todavía sobre cuándo vale la pena escribir el algoritmo
real de cada uno (necesitan props propias: columnas/filas para grid,
`flex-grow`/`flex-basis` por hijo para flex, que el catálogo de
componentes ni siquiera expone hoy). Queda fuera del alcance de la
migración a `core` -- se migra el comportamiento actual tal cual,
fallback incluido.

## 3. La firma nueva (post Fase B de `10_AVAUI.md`)

Dos cambios sobre la firma actual:

1. Recibe `ava::ui::Component` en vez de `DesignNode`.
2. La medición de leaves deja de ser una constante fija y pasa a ser
   inyectada -- así el mismo algoritmo sirve para ImGui (mide con
   `ImGui::CalcTextSize`) y para cualquier backend futuro, sin que
   `core/src/ui/` tenga que saber que ImGui existe.

```cpp
// core/src/ui/layout.h

namespace ava::ui {

struct Rect { float x = 0, y = 0, w = 0, h = 0; };

struct LayoutResult {
    // Clave: el propio puntero Component* del nodo (identidad estable
    // dentro de un mismo árbol/frame -- ver sección 4 sobre por qué no
    // puede ser un node_uid acá: Component no tiene ese campo, ver
    // 10_AVAUI.md sección 8).
    std::unordered_map<const Component*, Rect> rects;
};

// Devuelve el alto que debería ocupar un leaf de `width` de ancho dado
// su contenido real -- el caller decide cómo medir (texto vs backend).
// Sin medidor custom, LayoutDefaults::kLeafHeight (32.0f, el mismo
// valor de hoy) es el fallback -- así un test o un backend headless
// puede llamar ComputeLayout sin tener que inventar un medidor.
using LeafMeasurer = std::function<float(const Component& leaf, float available_width)>;

extern const float kDefaultLeafHeight; // = 32.0f, sin cambios de valor

LayoutResult ComputeLayout(const Component& root, Rect available_space,
                            const LeafMeasurer& measurer = nullptr);

} // namespace ava::ui
```

El cuerpo (`EstimateHeight`/`LayoutNode`) se porta casi literal desde
`studio/src/design/layout_engine.cpp` -- el único cambio real de lógica
es reemplazar el `return kDefaultLeafHeight;` de `EstimateHeight` por:

```cpp
if (node.GetChildren().empty()) {
    return measurer ? measurer(node, width) : kDefaultLeafHeight;
}
```

El backend ImGui pasa un measurer que llama `ImGui::CalcTextSize` sobre
el display-prop ya evaluado (mismo valor que `designer_canvas.cpp` ya
calcula hoy vía `EvalPropertyExpr`/`eval_cache` -- ver el doc de widget
lifecycle (todavía no escrito, ver `00_SYSTEM_ARCHITECTURE.md`) para
dónde vive esa evaluación una vez formalizada).

## 4. El adaptador `DesignNode → Component`

Studio sigue editando `DesignNode` (sección 2 de
`10_AVAUI.md` explica por qué eso está bien). Para
layoutear necesita convertir el árbol activo a un `Component` temporal
antes de llamar `ComputeLayout`. Diseño propuesto:

```cpp
// studio/src/design/component_bridge.h (nuevo archivo)
std::shared_ptr<ava::ui::Component> ToRuntimeComponent(const DesignNode& node);
```

Recorre `DesignNode` y arma un `Component` equivalente en `type` +
`children` (mínimo indispensable para que el layout dé el mismo
resultado). **No** copia `node_uid` (`Component` no tiene ese campo, y
no debería -- ver la pregunta abierta correspondiente en
`10_AVAUI.md` sección 8).

Esto deja un problema pendiente de resolver en la implementación (no en
este doc): `LayoutResult::rects` queda indexado por `const Component*`,
pero todo el resto del canvas (`DrawNode`, selección, drag/drop) sigue
indexado por `node_uid` de `DesignNode`. Dos caminos posibles, ninguno
elegido todavía:

- **(a)** `ToRuntimeComponent` devuelve también un mapa paralelo
  `Component* → node_uid` (o directamente `node_uid → Component*`)
  construido en el mismo recorrido, y el canvas hace una traducción
  extra al leer `LayoutResult`.
- **(b)** en vez de cachear por puntero, `ComputeLayout` opcionalmente
  acepta un segundo `LeafMeasurer`-like callback `NodeKeyFn` para que el
  caller decida la clave del mapa (Studio le pasaría algo que lee un
  campo lateral; un backend sin ese concepto usa el puntero por
  default).

Recomendación: **(a)** es más simple de implementar y no ensucia la
firma de `ComputeLayout` con un segundo callback -- se resuelve la
próxima vez que se toque este archivo, no hace falta decidirlo ahora.

El cacheo (para no reconstruir el `Component` temporal cada frame) sigue
el mismo patrón que ya existe para `resolved_root` en
`component_resolver.cpp` (ver `docs/history/DESIGNER_VIEW_SESSIONS.md` 9.16):
invalidar cuando `doc.dirty` cambia o el árbol se edita, no en cada
frame.

## 5. Qué NO cambia con esta migración

- El comportamiento visual del Designer no cambia (mismos rects que
  hoy, mismo fallback de Grid/Flex, mismo `kDefaultLeafHeight` mientras
  nadie pase un `LeafMeasurer` custom).
- `studio/src/design/layout_engine.{h,cpp}` no desaparece de entrada --
  puede quedar como una capa fina que llama al de `core` vía el
  adaptador, y borrarse recién cuando ya no quede nada que lo referencie
  (mismo criterio de "no romper en el medio" que usó la migración del
  parser en la Fase A de `10_AVAUI.md`).

## 6. Preguntas abiertas

- (a) vs (b) de la sección 4, sin resolver todavía.
- ¿`LeafMeasurer` necesita ver algo más que `Component` + ancho
  disponible? (ej.: el backend ImGui hoy también depende del *display
  prop evaluado*, que no vive en `Component` sino que lo calcula Studio
  contra una VM -- ver `el doc de widget lifecycle (todavía no escrito, ver `00_SYSTEM_ARCHITECTURE.md`)`. Puede que el measurer
  necesite recibir ese string ya evaluado como tercer parámetro en vez
  de tener que recalcularlo él mismo.)
- Memoización de `EstimateHeight` (sección 1.4): ¿vale la pena resolverla
  como parte de la migración a `core`, ya que se está tocando el
  archivo igual, o se deja para después por separado?
