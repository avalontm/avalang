# Fase 0 — Auditoría completa y congelar el contrato

Plan de referencia: `PLAN_AVAUI_MOTOR_UNICO.md` (root).

Esta es la checklist cerrada de "preguntas que solo `avaui` puede responder",
cada una con su dueño actual en el código y su estado de cumplimiento.

---

## Tabla de diagnóstico — estado verificado en el código

### 1. Parseo de texto `.avaui`

| Plataforma | Archivo | Estado |
|---|---|---|
| AvaHost | `runtime/avaui/src/parser/AvauiParser.cpp` | ✅ Usa el parser de `avaui` directamente |
| AvaStudio | `runtime/avastudio/src/design/avaui_text.cpp` | ⚠️ Adaptador que llama a `avalang.dll` (`ava_ui_parse_avaui_text`) — no reimprime la gramática, pero va por la C API del parser viejo, no por `AvauiParser` directamente |
| Parser viejo | `runtime/avalang/src/ui/avaui_text.cpp` | ❌ Segundo parser, vive en `avalang.dll`, expuesto via C API. AvaStudio lo usa a través de su adaptador |

**Dueño legítimo:** `avaui::parser::AvauiParser::Parse` en `runtime/avaui/src/parser/AvauiParser.cpp`

**Brecha:** AvaStudio no llama a `AvauiParser` directamente — pasa por `avalang.dll`'s C API (`ava_ui_parse_avaui_text`), que es el parser viejo. El parser de `avaui` ya existe y es el correcto (tab-aware, `extends` capturado, `__unresolvedImportCall` marcado), pero AvaStudio no lo usa. Hay DOS parsers vivos.

---

### 2. Layout (row/column/gap/padding/fill)

| Plataforma | Archivo | Estado |
|---|---|---|
| AvaHost | `runtime/avaui/src/layout/LayoutEngine.cpp` | ✅ Usa `avaui::LayoutEngine` directamente |
| AvaStudio | `runtime/avaui/src/layout/LayoutEngine.cpp` (vía `BuildLiveRender`) | ✅ Usa `avaui::LayoutEngine` — `live_render_bridge.cpp` lo llama directamente |
| Layout viejo de AvaStudio | `runtime/avastudio/src/design/layout_engine.{h,cpp}` | ❌ Muerto — nadie lo incluye ni llama (verificado: solo se incluye a sí mismo) |

**Dueño legítimo:** `avaui::LayoutEngine` en `runtime/avaui/src/layout/LayoutEngine.cpp`

**Estado:** Compartido correctamente. El layout_engine propio de AvaStudio es código muerto listo para borrar (Fase 6).

---

### 3. `extends layouts.X` + `slot()` — composición de página con layout

| Plataforma | Estado | Detalle |
|---|---|---|
| AvaStudio | ❌ **Brecha grave** | `avaui_text.cpp:291` pasa `out_extends=nullptr` a `ava_ui_parse_avaui_text` — el campo `extends` se descarta. El preview NUNCA carga ni renderiza el layout |
| AvaHost | ⚠️ Funciona, pero la lógica vive en la plataforma | `ui_pipeline_dynamic_renderer.cpp:383-627` — parsea el layout, resuelve sus imports, localiza el `Slot` vía `LayoutEngine::FindNode`, renderiza la página como fragment y la inyecta. Todo inline en AvaHost |
| `avaui` | ❌ No existe | No hay `ComposePageWithLayout` ni equivalente |

**Dueño legítimo (futuro):** `avaui::ComposePageWithLayout(pageTree, layoutTree)` en `runtime/avaui/`

**Brecha:** La composición `extends`/`slot()` no existe en `avaui`. AvaHost la implementa inline (funciona pero está en el lado equivocado). AvaStudio directamente la ignora. Esta es la brecha más grave del plan.

---

### 4. Auto-bind de eventos por convención (`OnXClick` sin `click=` explícito)

| Plataforma | Archivo | Estado |
|---|---|---|
| Parser viejo | `runtime/avalang/src/ui/avaui_text.cpp:130-175` | ✅ `DefaultEventsForType` + `AutoBindEvents` + `CollectFunctionNames` |
| `avaui` parser | `runtime/avaui/src/parser/AvauiParser.cpp` | ❌ No existe — el parser de `avaui` no hace auto-bind |
| AvaStudio | Hereda del parser viejo (vía C API) | ⚠️ Funciona indirectamente porque `ava_ui_parse_avaui_text` llama a `AutoBindEvents` |
| AvaHost | No usa auto-bind | ❌ AvaHost parsea con `AvauiParser` que no tiene auto-bind |

**Dueño legítimo (futuro):** `avaui::parser` o `avaui::ComponentResolver` — el auto-bind debe vivir en `avaui`

**Brecha:** El auto-bind solo existe en el parser viejo (`avalang.dll`). El parser de `avaui` no lo implementa. AvaHost no tiene auto-bind.

---

### 5. Lista de nombres de evento válidos

| Ubicación | Archivo | Estado |
|---|---|---|
| Copia 1 | `runtime/avalang/src/ui/avaui_text.cpp:89-96` (`EventPropNames`) | Activa — parser viejo |
| Copia 2 | `runtime/avastudio/src/design/avaui_text.cpp:34-46` (`EventPropertyNames`) | Activa — usada por `IsEventPropertyName` de AvaStudio |
| Copia 3 (implícita) | `runtime/avaui/src/events/Event.h` | Sin verificar — posible tercera copia |

**Dueño legítimo (futuro):** Una sola lista en `runtime/avaui/src/events/` o `runtime/avaui/src/parser/`

**Brecha:** La lista está copiada a mano al menos 2 veces (las dos son idénticas hoy, pero ya se desincronizaron una vez según el comentario en `avaui_text.cpp:36-39`).

---

### 6. Detección de "esto es un import a otro componente"

| Mecanismo | Archivo | Estado |
|---|---|---|
| Marca explícita | `runtime/avaui/src/parser/AvauiParser.cpp:284` (`__unresolvedImportCall`) | ✅ El parser de `avaui` marca los calls con esta propiedad |
| Heurística mayúscula | `runtime/avastudio/src/design/component_resolver.cpp:42` (`IsComponentCall` — `char.IsUpper(type[0])`) | ❌ AvaStudio usa mayúscula inicial como heurística |
| AvaHost | `runtime/avahost/src/rendering/ui_component_resolver.cpp:33-35` | ✅ Usa `__unresolvedImportCall` (el mecanismo correcto) |

**Dueño legítimo:** La marca `__unresolvedImportCall` del `AvauiParser` de `avaui`

**Brecha:** AvaStudio usa heurística de mayúscula en vez de la marca del parser. Dos mecanismos distintos.

---

### 7. Resolución de ruta de import a archivo

| Plataforma | Archivo | Estado |
|---|---|---|
| AvaStudio | `runtime/avastudio/src/design/component_resolver.cpp` (`ResolveImportPath`) | Propio, sin caché por fecha |
| AvaHost | `runtime/avahost/src/rendering/ui_component_resolver.cpp` + `dotted_path.cpp` | Propio, con caché por mtime |
| `avaui` | No existe | ❌ Sin resolver compartido |

**Dueño legítimo (futuro):** `avaui::ComponentResolver` en `runtime/avaui/`

**Brecha:** Dos resolvers separados, sin caché compartido. `ResolveDottedAvauiPath` vive en `avahost/src/component/dotted_path.cpp` — también del lado equivocado.

---

### 8. Evaluar expresiones de `state`/props contra la VM real

| Plataforma | Archivo | Estado |
|---|---|---|
| AvaStudio | `runtime/avastudio/src/design/state_eval.cpp` (`BuildStateVM`, `EvalPropertyExpr`) | Propio — crea VM, bindea state, evalúa expresiones |
| AvaHost | `runtime/avahost/src/rendering/ui_vm_state_bridge.cpp` + `ui_vm_event_bridge.cpp` | Propio — `VmStateBridge`, `EvalIdentifier`, `WireVmEventHandlers` |
| `avaui` | No existe | ❌ Sin regla de coerción compartida |

**Dueño legítimo (futuro):** `avaui::state::CoercePropertyValue` en `runtime/avaui/src/state/`

**Brecha:** Ambos usan la VM real pero las reglas de coerción de tipo (string vs número vs bool vs identificador) están replicadas a mano en cada lado. El ciclo de vida de la VM puede ser distinto (legítimo), pero la regla de interpretación del valor no debería.

---

### 9. Theme / valores por defecto

| Plataforma | Estado |
|---|---|
| Ambas | ✅ `runtime/avaui/src/theme/` — compartido, sin problema |

**Dueño legítimo:** `avaui::theme` en `runtime/avaui/src/theme/`

---

### 10. Animaciones

| Estado | Detalle |
|---|---|
| ✅ No duplicada | Ningún archivo de muestra la usa todavía. El parser de `avaui` ya parsea `animate` blocks (`AnimationSpec`). Punto a vigilar cuando se empiece a usar |

---

## Checklist cerrada — "Preguntas que solo `avaui` puede responder"

| # | Pregunta | Dueño en `avaui` | Estado |
|---|---|---|---|
| 1 | ¿Qué dice este texto `.avaui`? (parseo) | `parser::AvauiParser::Parse` | ✅ Existe, AvaStudio aún no lo usa directamente |
| 2 | ¿Qué layout extiende esta página y dónde va el contenido? | `ComposePageWithLayout` (futuro) | ❌ No existe en `avaui` |
| 3 | ¿Qué evento dispara este componente? (auto-bind) | `parser` o `ComponentResolver` (futuro) | ❌ No existe en `avaui` |
| 4 | ¿Es esto un import? | `__unresolvedImportCall` (marca del parser) | ✅ Existe, AvaStudio no la usa |
| 5 | ¿Cuánto mide este `row`/`column`? | `LayoutEngine::Compute` | ✅ Compartido |
| 6 | ¿Qué vale `state.counter`? | `state::CoercePropertyValue` (futuro) | ❌ Reglas replicadas a mano |
| 7 | ¿Qué nombres de evento son válidos? | Lista única en `avaui` (futuro) | ❌ Copiada 2+ veces |
| 8 | ¿A qué archivo corresponde este import? | `ComponentResolver` en `avaui` (futuro) | ❌ Dos resolvers separados |
| 9 | ¿Qué tema/valores por defecto aplica? | `theme::DefaultTheme` | ✅ Compartido |

---

## Próximos pasos (orden del plan)

1. **Fase 1** (bloqueante): Crear `avaui::ComposePageWithLayout` — mover la lógica de `ui_pipeline_dynamic_renderer.cpp` a `avaui`, hacer que AvaStudio deje de descartar `extends`
2. **Fase 2** (bloqueante): Portar auto-bind de eventos a `avaui`
3. **Fase 3**: Un solo resolver de imports
4. **Fase 4**: AvaStudio usa `AvauiParser` directamente
5. **Fase 5**: Eliminar `DesignNode` como modelo paralelo
6. **Fase 6**: Borrar `layout_engine.{h,cpp}` muerto de AvaStudio
7. **Fase 7**: Unificar coerción de `state`
8. **Fase 8**: Un solo serializador
9. **Fase 9**: Contrato de renderer documentado
10. **Fase 10**: Tests de consistencia
11. **Fase 11**: Eliminar código viejo
