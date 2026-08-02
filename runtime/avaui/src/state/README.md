# ui/src/state

Fase 4 -- State System. Implementacion interna (`StateImpl`,
`StateBindingImpl`) de las interfaces publicas declaradas en
`include/avalang/ui/state/` (`IState`, `StateBinding`). `IState` es una
celda de valor reactivo (reutiliza `PropertyValue` de la Fase 2) con
suscripcion/notificacion de cambios. `StateBinding` es un enlace
unidireccional RAII: aplica el valor actual del estado a una propiedad
de un `IComponent` (Fase 2) al crearse, y vuelve a aplicarlo en cada
cambio futuro via `SetProperty`; al destruirse se desuscribe solo.
Sin rendering. Ver docs/AVALANG_UI_IMPLEMENTATION_PLAN.md y
docs/AVALANG_UI_PROGRESS.md.
