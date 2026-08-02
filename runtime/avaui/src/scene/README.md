# Phase 7 -- Scene Graph

Transformaciones, visibilidad, clipping, opacidad, z-order y tracking de regiones sucias para rendering incremental.

## Interfaces Públicas

- **`ISceneNode`** (`avalang/ui/scene/ISceneNode.h`)
  - Enriquece el nodo de render con propiedades de escena.
  - `Transform` struct: position, scale, rotation (local to parent).
  - `LocalTransform()`, `SetLocalTransform()` — transformación local.
  - `WorldTransform()` — transformación en espacio global (computed).
  - `IsVisible()`, `SetVisible()` — visibilidad.
  - `Opacity()`, `SetOpacity()` — opacidad (0.0 = transparent, 1.0 = opaque).
  - `ZOrder()`, `SetZOrder()` — z-order para painter's algorithm.
  - `ClipBounds()`, `SetClipBounds()`, `IsClipped()` — clipping.
  - `GetDirtyRegion()`, `MarkDirty()`, `ClearDirtyRegion()` — dirty tracking.
  - `GetRenderNode()` — acceso read-only al nodo de render.

- **`ISceneGraph`** (`avalang/ui/scene/ISceneGraph.h`)
  - Manager del scene graph.
  - `Create()` factory.
  - `Build(renderRoot)` — construye scene graph desde render tree.
  - `Root()`, `FindNode(componentId)` — acceso.
  - `UpdateTransforms()` — recalcula transformadas globales.
  - `ComputeDirtyRegions()` — calcula qué nodos necesitan redibujo.
  - `ForEachInRenderOrder(visitor)` — iteración por z-order (para rendering).
  - `ForEachDirtyNode(visitor)` — iteración solo nodos sucios.
  - `Invalidate()`, `IsDirty()` — estado de validez.

## Implementación Interna

- **`SceneNode`** (`SceneNode.h/.cpp`)
  - Implementación concreta de ISceneNode.
  - Mantiene local + world transforms.
  - Jerarquía parent/children.
  - Visibility, opacity, z-order, clipping.
  - Dirty region tracking.

- **`Transform.h`**
  - `Transform::ToMatrix()` — convierte a matriz 4x4 (GLM).

- **`SceneGraph`** (`SceneGraph.h/.cpp`)
  - Recorre RenderTree, construye SceneTree mirror.
  - `UpdateTransforms()` — propagación top-down de transformadas.
  - `ComputeDirtyRegions()` — marca nodos sucios.
  - `ForEachInRenderOrder()` — iteración sorted por z-order.

## Flujo de Construcción

```
Render Tree (sin transforms/visibility)
        ↓
   For each render node:
        ↓
   Create SceneNode (mirror hierarchy)
   Initialize: local transform = identity, visible = true, opacity = 1.0
        ↓
Scene Graph (hierarchy with transforms, visibility, clipping)
        ↓
UpdateTransforms():
  root.worldTransform = identity
  for each child: child.worldTransform = parent.worldTransform * child.localTransform
        ↓
ComputeDirtyRegions():
  for each node: if changed → mark dirty
        ↓
Renderer (consume scene graph in z-order)
```

## Propiedades Soportadas

- **Transform**: position (x,y), scale (sx,sy), rotation (radians)
- **Visibility**: on/off
- **Opacity**: 0.0..1.0 (inherited by children)
- **Z-Order**: int (higher = on top, applied at same hierarchy level)
- **Clipping**: rect (x,y,w,h) + enabled flag
- **Dirty Region**: rect + isDirty flag

## Ejemplo

```cpp
// Fase 6: Render Tree
auto renderTree = IRenderTree::Create();
renderTree->Build(componentRoot, layoutEngine);

// Fase 7: Scene Graph
auto sceneGraph = ISceneGraph::Create();
sceneGraph->Build(renderTree->Root());

// Actualizar transformadas
sceneGraph->UpdateTransforms();

// Calcular regiones sucias (para rendering incremental)
sceneGraph->ComputeDirtyRegions();

// Renderizar en orden (z-order)
sceneGraph->ForEachInRenderOrder([renderer](const auto& node) {
    if (node->IsVisible()) {
        renderer->DrawNode(node);
    }
});

// O solo nodos sucios (optimización):
sceneGraph->ForEachDirtyNode([renderer](const auto& node) {
    if (node->IsVisible()) {
        renderer->RedrawNode(node);
    }
});
```

## Limitaciones / Pendientes

- **Transformadas**: composición simple (sin matrices full 4x4 todavía).
  - Futuro: matrix multiplication full.

- **Opacity**: tracking pero sin alpha blending real (eso es Renderer).
  - Futuro: propagation a children, pre-multiplication en render.

- **Clipping**: bounds stored, no clipping actual (eso es Renderer).
  - Futuro: scissor test / SVG clip-path support.

- **Dirty regions**: simple rect, no history / merge optimization.
  - Futuro: dirty region union, coalescing.

- **Z-order**: simple integer, sin z-fighting resolution.
  - Futuro: layer stacking context (CSS z-index rules).

## Interoperabilidad

- **← Render Tree (Phase 6)**: entrada (espeja jerarquía).
- **→ Renderer (Phase 9+)**: salida (consumida para rendering).
- **← State/Events (Phase 4-5)**: pueden mutar transforms/visibility.
