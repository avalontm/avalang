# Phase 6 -- Render Tree

Conversión de árbol de componentes en árbol de nodos dibujables. Cada componente se descompone en uno o más nodos de render según su tipo.

## Interfaces Públicas

- **`IRenderNode`** (`avalang/ui/render_tree/IRenderNode.h`)
  - Base abstracta para nodos dibujables.
  - `RenderNodeType` enum: Container, Row, Column, Stack, Rectangle, Text, Image, Button, etc.
  - Identidad: `ComponentId()`, `Type()`.
  - Jerarquía: `Children()`, `AddChild()`, `RemoveChild()`.
  - Propiedades visuales: colores (bg, border, fg), bordes, radio, fuente.
  - Propiedades tipo-específicas: texto (Text), ruta (Image), fuente (Font).
  - Geometría: `Rect()` (llenada por RenderTree desde LayoutEngine).
  - Draw directives: `ShouldFill()`, `ShouldStroke()`, `StrokeWidth()`.

- **`IRenderTree`** (`avalang/ui/render_tree/IRenderTree.h`)
  - Factory y manager del árbol de render.
  - `Create()` factory.
  - `Build(componentRoot, layoutEngine)` — reconstruye desde componentes.
  - `Root()`, `FindNode(componentId)` — acceso.
  - `Invalidate()`, `IsDirty()` — estado de validez.
  - `ForEach(visitor)` — iteración depth-first.

## Implementación Interna

- **`RenderNode`** (`RenderNode.h/.cpp`)
  - Implementación concreta de IRenderNode.
  - Propiedades visuales con setters/getters.
  - Jerarquía mediante vector de children.

- **`RenderTree`** (`RenderTree.h/.cpp`)
  - Recorre ComponentTree + LayoutTree.
  - Llama a `BuildComponent()` recursivamente.
  - `DecomposeButton()`, `DecomposeText()`, `DecomposeImage()`, `DecomposeContainer()`.
  - Asigna propiedades visuales desde property bag del componente.
  - Construye nodo(s) de render y enlaza hijos.

## Flujo de Descomposición

```
Component Tree (Button, Text, Row, etc.)
        ↓
   For each component:
        ↓
   Read properties (backgroundColor, borderWidth, text, etc.)
   Read layout geometry (Rect from LayoutEngine)
   Decompose by TypeName:
     • Button  → RectangleNode (bg) + TextNode (label) + children
     • Text    → TextNode (text + font properties)
     • Image   → ImageNode (src)
     • Default → ContainerNode (recurse children)
        ↓
Render Tree (all nodes have geometry + visual properties)
```

## Propiedades Reconocidas

Del property bag del componente:
- `backgroundColor` — color de fondo (string, "#RGB" o "#RRGGBB" o "#RRGGBBAA")
- `borderColor` — color del borde
- `borderWidth` — ancho del borde (number, pixels)
- `color` — color del texto/foreground
- `text` — contenido de texto (para Text, Button)
- `src` — ruta de imagen (para Image)
- `fontSize` — tamaño de fuente (number, pixels)
- `fontName` — nombre de fuente (string, default "Arial")

Futuro (Phase 7+):
- `opacity`, `visibility`, `zIndex`, `transform`, etc.

## Ejemplo

```cpp
// Fase 4: Layout + State
auto layout = LayoutEngine::Create();
layout->Compute(componentRoot, {0, 0, 800, 600});

// Fase 6: Render Tree
auto renderTree = IRenderTree::Create();
renderTree->Build(componentRoot, layout);

// Iterar nodos dibujables
renderTree->ForEach([](const auto& node) {
    std::cout << "Node: " << (int)node->Type()
              << " rect: " << node->Rect().x << ","
              << node->Rect().y << " " << node->Rect().width
              << "x" << node->Rect().height << "\n";
});
```

## Limitaciones / Pendientes

- **Fase 6 actual**: descomposición estática básica.
  - Futuro: reconocimiento de más tipos (Radio, Checkbox, Slider, etc.).
  - Futuro: customización de descomposición vía component metadata.

- **Sin propiedades avanzadas**: opacity, transform, clipping.
  - Eso es Phase 7 (Scene Graph).

- **Sin intrinsic sizing**: el tamaño viene 100% del LayoutEngine.
  - Los nodos de render son pasivos en geometry.

- **Sin dirty tracking fino**: Build() reconstruye todo.
  - Futuro: invalidation granular + rebuilds incrementales.

## Interoperabilidad

- **← Components (Phase 2)** + **← Layout (Phase 3)**: entrada.
- **→ Scene Graph (Phase 7)**: salida (antes del render).
- **→ Renderer (Phase 9+)**: consumido por backends HTML/Native/PDF.
