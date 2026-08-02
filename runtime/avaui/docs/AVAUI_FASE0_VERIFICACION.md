# AVAUI — Fase 0: verificación previa a la Fase 12

`AVAUI_PLAN_FASE12_PLUS.md`, sección "0. Dos correcciones a la revisión,
antes de planear nada", se plantea como paso previo a toda la
planificación (Fases 12+). No es una fase de código — es la
verificación de dos afirmaciones antes de construir el resto del plan
sobre ellas. Quedan resueltas acá.

## 0.1 — "Renderer debería ser el último paso": verificado, correcto

El plan afirmaba que esto ya estaba resuelto arquitectónicamente, sin
cita de código. Se verificó contra el código real:

- `ui/include/avalang/ui/render/IRenderTree.h` (Fase 6): no incluye
  nada de `renderer/`. `IRenderTree::Build()` solo depende de
  `IComponent` y `LayoutEngine`.
- `ui/src/render/RenderTree.cpp`: sus únicos includes son
  `IComponent.h`, `ILayoutNode.h`, `LayoutEngine.h` — cero
  dependencia hacia `renderer/`.
- `ui/include/avalang/ui/renderer/IRenderer.h` (Fase 9): incluye
  `avalang/ui/commands/RenderCommand.h` (Fase 8) y está documentado
  explícitamente como "Converts render commands (Phase 8) into
  backend-specific output". `ProcessCommands()` es el único punto de
  entrada de datos hacia el renderer.

Dirección de dependencia confirmada:
`render/` (Fase 6) → `commands/` (Fase 8, serializa) → `renderer/`
(Fase 9, consume) → backend. Nunca al revés. **No hay nada que
reordenar.** Coincide con lo que decía el plan; la diferencia es que
ahora está comprobado línea por línea, no solo afirmado.

Acción pendiente (cosmética, ya registrada para la Fase 13): renombrar
`render/` → `render_tree/` para que el parecido de nombres con
`renderer/` no vuelva a generar la misma duda. Cero riesgo, no se
ejecuta acá.

## 0.2 — "Hay dos pipelines de UI, no uno": verificado, y ya resuelto

Esta corrección es la que dio origen a la Fase 12. Su verificación
contra código real (namespaces `ava::ui` duplicados, colisión de
`ComponentTree`, tabla de consumidores) y la decisión resultante ya
quedaron documentadas en `docs/AVAUI_CONVERGENCE_DECISION.md`
(entregado). No se repite acá — este archivo solo dejaba pendiente
confirmar 0.1, que es lo que se hizo arriba.

## Conclusión

Las dos correcciones previas a la Fase 12 están verificadas contra el
código real, no solo asumidas. No generan trabajo de código nuevo en
esta fase — 0.1 no requiere cambios (el renombre cosmético queda en
Fase 13, ya anotado en `AVAUI_CONVERGENCE_DECISION.md`), y 0.2 ya se
resolvió en la Fase 12. **La Fase 12 queda confirmada como
válidamente apoyada en el código real; se puede avanzar a la Fase 13.**
