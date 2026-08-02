# ui/src/layout

Fase 3 -- Layout Engine. Implementacion interna (`LayoutNode`,
`LayoutEngineImpl`) de la interfaz publica declarada en
`include/avalang/ui/layout/` (`ILayoutNode`, `LayoutEngine`,
`LayoutRect`, `LayoutAlignment`). Calcula width/height/margin/padding/
alineacion y arreglo Row/Column/Stack a partir del Component Tree
(Fase 2) -- nunca dibuja. Nombres de propiedad reconocidos
(`width`, `height`, `margin*`, `padding*`, `align`, `align-h`,
`align-v`, `spacing`) documentados en `LayoutProperties.h`. Ver
docs/AVALANG_UI_IMPLEMENTATION_PLAN.md y docs/AVALANG_UI_PROGRESS.md.
