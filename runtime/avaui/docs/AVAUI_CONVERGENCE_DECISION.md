# AVAUI — Decisión de convergencia `core/src/ui` ↔ `ui/`

Fase 12 de `AVAUI_PLAN_FASE12_PLUS.md`. No es una fase de código: es la
decisión escrita que las Fases 14-20 dan por asumida. Alcance: solo
Windows (ver "Alcance actual" en `AVALANG_UI_PROGRESS.md`); Linux/macOS
quedan fuera de esta decisión hasta la Fase 21.

## 0. Corrección a un dato del propio `AVAUI_PLAN_FASE12_PLUS.md`

El plan (sección 0.2, punto 3) asume que la colisión de nombres es
`ava::ui::Component` (VM) vs. `avalang::ui::IComponent` (motor nativo) —
es decir, namespaces distintos con conceptos parecidos. Se verificó
contra el código real y **no es así**: los dos módulos usan el mismo
namespace.

```cpp
// core/src/ui/component.h
namespace ava { namespace ui { class Component ... } }

// core/src/ui/tree.h
namespace ava { namespace ui { class ComponentTree ... } }

// ui/include/avalang/ui/UIModule.h
namespace ava { namespace ui { class UIModule ... } }

// ui/include/avalang/ui/components/IComponent.h
// ui/include/avalang/ui/components/ComponentTree.h
namespace ava { namespace ui { class IComponent ...; class ComponentTree ...; } }
```

Es decir, hoy mismo `ava::ui::ComponentTree` está definido **dos veces**
con contenidos incompatibles (`core/src/ui/tree.h` — wrapper simple de
`shared_ptr<Component>` — y `ui/include/avalang/ui/components/ComponentTree.h`
— interfaz abstracta con factory, `CreateComponent`, `FindById`). Hoy no
rompe la build porque ningún `.cpp` incluye ambos headers a la vez, pero
es un choque de ODR (One Definition Rule) latente: el día que
`avahost` o `studio` necesiten incluir algo de `ui/` sin dejar de incluir
`core/src/ui/`, el build falla o — peor — linkea con el símbolo
equivocado según orden de inclusión. Esto no es un riesgo hipotético de
"Fase 20 quizás"; es un bug de nombres que ya existe hoy, agravado por
el hecho de que ambos son headers-only en su mayoría (fácil que el
compilador nunca vea el conflicto hasta que alguien mezcle un TU).

Esta corrección no cambia el orden de fases del plan pero sí agrega un
ítem concreto a la Fase 13 (ver sección 3 de este documento).

## 1. Pregunta 1 — ¿`ui/` reemplaza a `core/src/ui`, o conviven para siempre?

**Decisión: reemplazo planeado, no convivencia indefinida.**

Justificación:

- `docs/AVALANG_UI_IMPLEMENTATION_PLAN.md` ("Integration With AvaHost")
  ya documenta que AvaHost debe terminar consumiendo el mismo pipeline
  que producción — eso es incompatible con mantener dos motores de UI
  para siempre.
- `core/src/ui` es deliberadamente simple (un `Component` con
  `shared_ptr`, sin layout engine propio, sin scene graph, sin render
  commands) porque nunca fue diseñado como motor de UI final — es la
  representación mínima que la VM necesita para exponer algo por
  `ava_ui_*` (C API) mientras el motor nativo no existía. `ui/` es el
  reemplazo funcional de esa pieza, con 11 fases de profundidad que
  `core/src/ui` no tiene ni va a tener (no hay Fase planeada para
  agregarle Scene Graph o Render Commands a `core/src/ui`).
- Mantener ambos "para siempre" duplicaría trabajo de mantenimiento en
  cada fase futura de controles/theme/animation (habría que decidir,
  control por control, si vive en los dos lados) sin ganar nada a
  cambio.

## 2. Pregunta 2 — ¿Por cuánto tiempo conviven, y con qué límite?

**Límite explícito, vigente desde esta fase hasta que la Fase 20 se
cierre:**

- **`core/src/ui` es, mientras dure la convivencia, el único camino
  para árboles de UI producidos dinámicamente por la VM** — scripts
  `.ava` que construyen componentes en runtime y los exponen vía
  `ava_ui_*` (`public/src/c_api.cpp`). Nadie migra este camino antes de
  Fase 20.
- **`ui/` es, desde ya, el único camino para el contenido declarado en
  archivos `.avaui`** (Fase 14 en adelante) — compilado, no
  interpretado por la VM. Ningún parser de `.avaui` se escribe contra
  `core/src/ui`; eso sería trabajo tirado, porque el reemplazo (punto 1)
  ya está decidido.
- **`avahost` y `studio` siguen sobre `core/src/ui` hasta la Fase 20.**
  No se migra `html_renderer.cpp` ni `engine_bridge.cpp` antes de que
  Fases 14-18 dejen `ui/` con parser + resources + theme + controles
  base estables — migrar antes sería mover producción a un motor que
  todavía no puede resolver un `.avaui` real end-to-end.
- **Ningún `.cpp` incluye headers de `core/src/ui/` y de
  `ui/include/avalang/ui/` en la misma unidad de traducción** hasta que
  el punto 3 (renombre de namespace) esté resuelto. Es una regla dura,
  no una preferencia — ver ODR arriba.
- Cierre de la convivencia: cuando la Fase 20 migre `avahost`/`studio`,
  `core/src/ui` pasa a "solo VM dinámica", ya sin consumidores de
  producción del lado de render. No se borra en Fase 20 (`public/src/c_api.cpp`
  lo sigue necesitando para exponer árboles dinámicos), pero deja de
  tener ningún camino de renderizado propio — a partir de ahí, si un
  árbol de `core/src/ui` necesita dibujarse, se traduce a
  `ava::ui::IComponent` (ver Fase 20, "camino de traducción" pendiente
  de diseñar ahí, no acá).

## 3. Pregunta 3 — ¿Quién es dueño de qué nombres?

**Decisión: renombrar el namespace del motor nuevo de `ava::ui` a
`avalang::ui`, ya en la Fase 13 (freeze de interfaces), antes de que
exista un solo consumidor externo del ABI congelado.**

Razonamiento:

- El choque ya es real (sección 0), no potencial — dos definiciones de
  `ComponentTree` en el mismo namespace.
- El propio path de include del motor nuevo ya usa `avalang/ui/...`
  (`ui/include/avalang/ui/UIModule.h`) — el namespace `ava::ui` fue
  probablemente copiado del patrón de `core/src/ui` sin querer, no una
  decisión deliberada de compartir namespace. Alinear el namespace con
  el path de include (`avalang::ui`) es la corrección de menor
  sorpresa, y es gratis ahora mismo: `AbiVersion()` sigue en 11, cero
  consumidores externos.
- Después del renombre: `ava::ui::*` queda reservado exclusivamente
  para `core/src/ui` (VM/legacy); `avalang::ui::*` queda reservado
  exclusivamente para el motor nativo (`ui/`). Nadie vuelve a confundir
  un `ava::ui::Component` con un `avalang::ui::IComponent` porque ya no
  pueden ni compilar como si fueran lo mismo por accidente.
- Este renombre se ejecuta en la Fase 13, no acá, porque la Fase 12 es
  solo la decisión escrita — pero queda registrado acá como parte de la
  Pregunta 3 para que la Fase 13 no lo redescubra desde cero.

## 4. Resumen ejecutable (para Fase 13 y siguientes)

| Decisión | Efecto inmediato |
|---|---|
| `ui/` reemplaza a `core/src/ui` (no convivencia indefinida) | Fase 20 es la fase de migración real, no una fase opcional |
| `core/src/ui` = solo VM dinámica (`.ava` runtime) mientras dure la convivencia | Ningún parser `.avaui` (Fase 14) toca `core/src/ui` |
| `ui/` = solo `.avaui` declarado/compilado | Fase 14 en adelante construye exclusivamente sobre `avalang::ui` |
| Prohibido incluir `core/src/ui/*.h` y `ui/include/avalang/ui/**/*.h` en el mismo `.cpp` hasta el renombre | Regla de build a partir de esta fase |
| Renombrar `ava::ui` → `avalang::ui` en todo `ui/` | Ejecutar en Fase 13, antes del freeze de ABI |
| `avahost`/`studio` no se tocan antes de Fase 20 | Cero riesgo de regresión en producción durante Fases 13-19 |

**Sin este documento no se empieza la Fase 14** (regla del plan). Con
esta decisión tomada, la Fase 13 puede arrancar.
