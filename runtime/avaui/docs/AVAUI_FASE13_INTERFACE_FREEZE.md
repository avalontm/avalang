# AVAUI — Fase 13: freeze de las interfaces internas

Alcance: solo Windows (ver "Alcance actual" en `AVALANG_UI_PROGRESS.md`).
Depende de `docs/AVAUI_CONVERGENCE_DECISION.md` (Fase 12), ya cerrada.

## 1. Revisión cruzada de las 9 interfaces

Interfaces revisadas: `IComponent`, `ComponentTree`, `ILayoutNode`,
`IRenderNode`, `ISceneNode`, `RenderCommand`, `IRenderer`, `IState`,
`IEventDispatcher`.

### 1.1 `ComponentId` — consistente, sin acción

Un solo `using ComponentId = unsigned long long;` en `Fwd.h`, usado sin
excepción en las 9 interfaces y en toda la implementación interna
(`ui/src/`). Ninguna fase inventó su propio tipo o alias.

### 1.2 Color — inconsistente, corregido sin romper ninguna firma

Hallazgo real (no listado en el plan de revisión original): dos fases
ya publicadas representan color de forma distinta, sin ningún punto de
conversión entre ellas:

- `IRenderNode` (Fase 6): `BackgroundColor()`/`BorderColor()`/
  `ForegroundColor()` devuelven `std::string` en formato hex tipo CSS
  (`"#RGB"`, `"#RRGGBB"`, `"#RRGGBBAA"`).
- `RenderCommand` (Fase 8): usa `Color{uint8 r,g,b,a}`.

Se confirmó además que **no existe ningún código en el repo** que
recorra Render Tree/Scene Graph y produzca `RenderCommand`s — el
`IRenderCommandSink` (Fase 8) solo tiene un consumidor (`BaseRenderer`,
Fase 9) y cero productores. El pipeline documentado
(`Render Tree -> Scene Graph -> Render Commands -> Renderer`) todavía
no tiene ese tramo escrito; hoy `IRenderer::ProcessCommands()` solo se
ejercita con comandos armados a mano (tests/ejemplos), nunca con una
escena real. Esto **no se resuelve en esta fase** — escribir ese
"walker" es trabajo de pipeline real, no freeze de interfaces, y
corresponde a cuando haya algo concreto que dibujar (Fase 17, Button).
Queda anotado acá para que la Fase 17 no lo redescubra de cero.

Decisión tomada (sin romper ninguna firma publicada, porque ambas son
razonables en su propio nivel — string en Render Tree para que
Theme/`.avaui` escriban colores como texto, struct en Render Commands
para que el renderer no parsee strings en el hot path):

- Se agrega `avalang::ui::common::ParseColor(const std::string&) -> Color`
  en `ui/src/common/ColorParse.h/.cpp` (nuevo) como **el** punto de
  conversión, para que el futuro walker (o cualquier renderer que decida
  saltarse `IRenderCommandSink` y leer `IRenderNode` directo, como hace
  hoy implícitamente el patrón de `HTMLRenderer`) no reinvente su propio
  parser de hex por separado.
- Casos inválidos caen a negro opaco (`{0,0,0,255}`), mismo criterio de
  "fallback genérico y silencioso" que ya usa `LayoutEngine` con
  `TypeName` no reconocido (Fase 3) — consistente con el resto del
  pipeline, no un criterio nuevo.
- Agregado a `ui/CMakeLists.txt` (`src/common/ColorParse.cpp`).

### 1.3 Otras firmas — sin hallazgos que ameriten cambio

Se revisaron jerarquía/paternidad (`Parent()`/`Children()` con la misma
forma en `IComponent`, `ILayoutNode`, `ISceneNode`), factories (`Create()`
estático en las 6 interfaces que lo tienen — mismo patrón en todas), y
manejo de punteros no-propietarios vs. `shared_ptr` (consistente:
`IRenderNode`/`ISceneNode` usan `shared_ptr` porque Scene Graph necesita
referenciar nodos de Render Tree que pueden sobrevivir a un rebuild
parcial; `ILayoutNode`/`IComponent` usan punteros crudos porque son
propiedad exclusiva de `ComponentTree`/`LayoutEngine`). Sin cambios.

## 2. Renombre de namespace: `ava::ui` → `avalang::ui`

Ejecutado en esta fase, como quedó anotado en
`docs/AVAUI_CONVERGENCE_DECISION.md` (Pregunta 3). Alcance: **solo**
`ui/include/` y `ui/src/` (89 archivos). `core/src/ui` **no** se tocó —
sigue en `ava::ui`, es el camino legacy de la VM por decisión de la
Fase 12. Nada fuera de `ui/` referenciaba `avalang::ui` todavía (se
verificó con grep antes y después), así que el rename no tuvo blast
radius fuera del módulo.

Mecánica: reemplazo de la apertura `namespace ava {\nnamespace ui {`
por `namespace avalang {\nnamespace ui {`, de las referencias
calificadas `ava::ui::` por `avalang::ui::`, y de los comentarios de
cierre `// namespace ava` correspondientes. **Cuidado explícito**: el
módulo también depende de `ava::platform::ui::*` (la PAL, un namespace
distinto, no tocado) — el reemplazo no debía rozarlo.

Un bug real quedó atrapado por el chequeo de compilación, no por
inspección visual: `ui/src/events/EventDispatcher.h`/`.cpp` usaban
`platform::ui::IMouse`/`IKeyboard`/`MouseButton` **sin calificar**,
apoyándose en que `ava::platform` era namespace hermano de `ava::ui`
dentro del mismo `ava` envolvente — válido antes del rename, roto
después (nuestro namespace pasó a `avalang`, ya no hay `avalang::platform`).
Se corrigió calificando explícitamente a `ava::platform::ui::*` en esos
dos archivos. Sin este chequeo de compilación el bug hubiera quedado
silencioso hasta la primera build real en Windows.

Verificación: `g++ -fsyntax-only -std=c++20` sobre todos los `.cpp`
independientes de plataforma (todo excepto `platform/windows/*`, que
requieren `<windows.h>`, no disponible en este sandbox — mismo límite
ya documentado en la Fase 11). Todos compilan limpio salvo los que ya
fallaban antes por falta de `glm` en este entorno (Scene Graph, Fase 7 —
no relacionado con esta fase, mismo hueco que ya documentaba la Fase 11).
Balance de namespaces verificado por script (89 aperturas `avalang::ui`
= 89 cierres, sin fugas de `ava::ui` residual).

## 3. Renombre cosmético `render/` → `render_tree/`

Ejecutado en esta fase (ver `AVAUI_PLAN_FASE12_PLUS.md`, sección 0.1 y
Fase 13: "buen momento porque estás tocando todo igual"). Alcance:
`ui/include/avalang/ui/render/` → `ui/include/avalang/ui/render_tree/`,
`ui/src/render/` → `ui/src/render_tree/`, y todas las rutas de include
(`avalang/ui/render/...`) y fuentes en `ui/CMakeLists.txt`
(`src/render/...`) actualizadas a `render_tree`. Cero cambio de
comportamiento — el pipeline (`render_tree/` construye, `renderer/`
consume) es el mismo que ya estaba correcto (ver Fase 0,
`docs/AVAUI_FASE0_VERIFICACION.md`), solo cambia el nombre de carpeta
para que no se vuelva a confundir con `renderer/`. Verificado con el
mismo chequeo de compilación de la sección 2.

## 4. ABI congelado

`UIModule::AbiVersion()` pasa de 11 a **12**, marcado explícitamente
como frozen en `UIModule.h`: a partir de acá, cualquier cambio de
firma a una de las 9 interfaces exige bump de versión + nota en
`AVALANG_UI_PROGRESS.md`, no un edit silencioso.

## 5. Entregable

- `docs/AVAUI_FASE13_INTERFACE_FREEZE.md` (este archivo).
- `ui/src/common/ColorParse.h/.cpp` (nuevo).
- Namespace `avalang::ui` en todo `ui/include`/`ui/src` (`core/src/ui`
  sigue en `ava::ui`, intacto).
- `ui/include/avalang/ui/render_tree/`, `ui/src/render_tree/`
  (renombrados desde `render/`).
- `UIModule::AbiVersion()` = 12, marcado "frozen" en el header.
- Las 9 interfaces sin cambios de firma pendientes conocidos.

Con esto, la Fase 14 (parser `.avaui` → `ComponentTree`) puede
arrancar sobre una base con nombres sin colisión y sin firmas que se
esperen romper.
