# AvaUI — Plan de trabajo (Fase 12 en adelante)

Basado en la revisión recibida, pero contrastado contra el código real del
repo (no solo contra la estructura de carpetas). Dos hallazgos cambian el
orden de prioridades respecto a lo que sugiere la reseña — están explicados
abajo antes del plan en sí.

---

## 0. Dos correcciones a la revisión, antes de planear nada

### 0.1 El punto "Renderer debería ser el último paso" — ya está resuelto

La reseña propone invertir `render/` y `renderer/` porque asume que el
Renderer es parte del Render Tree. No es así: ya revisé
`docs/AVALANG_UI_IMPLEMENTATION_PLAN.md` y el pipeline documentado y
**ya implementado** (Fases 6, 8, 9) es exactamente el que la reseña pide:

```
Component Tree -> Layout -> State -> Render Tree -> Scene Graph
                -> Render Commands -> Renderer -> Backend
```

`render/` (Fase 6) construye el árbol de dibujables; `commands/` (Fase 8)
serializa eso a comandos de dibujo; `renderer/` (Fase 9) es quien *consume*
esos comandos, no quien los produce. La confusión es solo de nombres de
carpeta (`render` vs `renderer` se parecen mucho), no de arquitectura.
**No hay nada que reordenar.** Si querés, en la Fase 12 (abajo) le cambio
el nombre a `render/` por `render_tree/` para que no vuelva a confundir a
nadie, pero es cosmético.

### 0.2 El hallazgo real que la reseña no llegó a ver: hay DOS pipelines de UI, no uno

La reseña marca como duda menor la coexistencia de `core/src/ui/` y `ui/`.
Fui a verificar qué tan grave es, y es más importante de lo que parece:

| | `core/src/ui/` | `ui/` (el módulo nuevo) |
|---|---|---|
| Qué es | Component Tree **interpretado**, atado a `vm/value.h` | Motor **nativo** C++ compilado, Fases 1-11 |
| Quién lo usa hoy | `public/src/c_api.cpp` (API C), `avahost` (server web + `html_renderer.cpp`), `studio` (`engine_bridge.cpp`) | Nadie todavía — es standalone |
| Tiene parser de `.avaui` | Sí (vía `core/src/frontend`) | **No** — confirmado en la nota de la Fase 2: *"Sin parser, `.avaui -> ComponentTree` queda para `parser/`"* |
| Tiene renderer HTML | Sí — `avahost/src/rendering/html_renderer.cpp` | Sí — `ui/src/renderer/HTMLRenderer.cpp` (Fase 10) |

Es decir: **hoy existen dos renderers de HTML distintos** para dos
representaciones de componentes distintas, y el motor nuevo (Fases 1-11,
que ya diste por "terminadas") **todavía no puede cargar un `.avaui` real**
— solo puede recibir árboles armados a mano en C++. `avahost` y `studio`
siguen corriendo 100% sobre el camino viejo (`core/src/ui`); el módulo
`ui/` nuevo no está conectado a nada todavía.

Esto no es necesariamente un error — puede ser intencional (motor nuevo en
paralelo, sin tocar lo que ya funciona en producción) — pero es una
decisión arquitectónica de fondo que hay que tomar explícita y
conscientemente, no dejar que se resuelva sola. La dejo como Fase 12,
antes que ninguna otra cosa, porque todo lo demás (parser, controls,
themes) depende de qué se decida acá.

---

## 1. Orden de fases propuesto

```
Fase 12 — Decisión de convergencia core/src/ui <-> ui/           (decisión, no código)
Fase 13 — Congelar las interfaces internas                        (freeze de ABI)
Fase 14 — Parser .avaui -> ComponentTree (módulo ui/)              (gap crítico)
Fase 15 — Resources (fonts/images/icons/localization)              (bloquea Fase 16 y 17)
Fase 16 — Theme system                                             (bloquea Fase 17)
Fase 17 — Controls: primer control real (Button)                   (valida todo lo anterior)
Fase 18 — Controls: resto del set base (Text, TextBox, Column/Row/Stack, Image)
Fase 19 — Animation                                                (post-controls)
Fase 20 — Integración real con AvaHost y AvaStudio                 (cierra el círculo con Fase 12)
Fase 21 — Linux/macOS (retomar, hoy "en estudio")
```

Cada fase abajo tiene: qué NO se toca, qué se entrega, y criterio de
"terminado" (mismo formato que ya usa `docs/AVALANG_UI_PROGRESS.md`, para
que puedas pegar el resultado ahí directamente).

---

## Fase 12 — Decisión de convergencia `core/src/ui` ↔ `ui/`

**No es una fase de código, es una fase de decisión escrita.** El riesgo
de saltarla es que en la Fase 20 (integración con AvaHost/Studio)
descubras que hiciste el motor nuevo incompatible con una premisa que
nadie escribió.

Preguntas a responder y dejar documentadas en un
`docs/AVAUI_CONVERGENCE_DECISION.md` nuevo:

1. **¿`ui/` reemplaza a `core/src/ui` eventualmente, o conviven para
   siempre?** El propio `AVALANG_UI_IMPLEMENTATION_PLAN.md` ("Integration
   With AvaHost") dice que AvaHost debería terminar usando el mismo
   pipeline que producción — eso implica reemplazo, no convivencia
   indefinida. Si es así, decilo explícitamente y ponele una fase futura
   (Fase 20 abajo).
2. **Si conviven, ¿por cuánto tiempo y con qué límite claro?** (ej.:
   "`core/src/ui` sigue siendo el único camino para `.ava` scripts
   dinámicos vía VM; `ui/` es solo para apps compiladas/nativas" — es una
   distinción legítima, pero tiene que quedar escrita para que nadie la
   rompa sin querer).
3. **¿Quién es dueño de qué nombres?** Ahora mismo ambos usan
   `namespace ava::ui` / `avalang::ui` con conceptos parecidos
   (`Component`, `ComponentTree`) pero incompatibles entre sí. Si van a
   convivir, hay que evitar que alguien confunda un `ava::ui::Component`
   (VM) con un `avalang::ui::IComponent` (motor nativo) — son tipos que no
   se pueden mezclar.

**Entregable:** `docs/AVAUI_CONVERGENCE_DECISION.md` con la decisión y su
justificación. Sin este documento, no se empieza la Fase 14.

---

## Fase 13 — Congelar las interfaces internas

Esto es lo que la reseña llama correctamente "la siguiente gran meta
técnica" y coincido en que va antes de `controls/`. Hoy `ui/include/avalang/ui/`
ya tiene las 9 interfaces (`IComponent`, `ILayoutNode`, `IRenderNode`,
`ISceneNode`, `RenderCommand`, `IRenderer`, `IState`, `IEventDispatcher`,
`ComponentTree`) construidas fase a fase sin que nadie las haya revisado
juntas de punta a punta.

- Revisión cruzada de las 9 interfaces buscando inconsistencias entre
  fases (ej.: ¿`ComponentId` se usa igual en `IComponent`, `ILayoutNode` e
  `ISceneNode`? ¿alguna fase inventó su propio tipo de id o de color?).
- Confirmar que ninguna fase futura (`controls/`, `theme/`) va a necesitar
  romper una firma ya publicada. Si algo huele a que sí, cambiarlo ahora
  que romperlo no cuesta nada (`AbiVersion()` sigue en 11, sin
  consumidores externos todavía).
- Congelar `AbiVersion()` en un número (ej. 12) marcado como "estable" en
  `UIModule.h`, y a partir de ahí cualquier cambio breaking a una interfaz
  ya congelada exige bump de versión + nota en el progreso, no un edit
  silencioso.
- Sobre el punto de la reseña "Renderer debería ser el último paso": acá
  es el momento de, si querés, renombrar `render/` → `render_tree/` para
  que el nombre no vuelva a confundir a nadie (ver 0.1). Cosmético, cero
  riesgo, buen momento porque estás tocando todo igual.

**Entregable:** las 9 interfaces sin cambios pendientes conocidos +
`AbiVersion()` congelado + nota en `AVALANG_UI_PROGRESS.md`.

---

## Fase 14 — Parser `.avaui` → `ComponentTree` (el gap crítico)

Sin esto, todo lo que sigue (`controls/`, `theme/`) se sigue probando con
árboles armados a mano en C++, nunca con un archivo `.avaui` real. Es el
mayor riesgo de que la Fase 17 en adelante construya sobre supuestos que
no sobreviven al primer archivo real.

- Definir si reutiliza el parser de `core/src/frontend` (compartir
  gramática/AST con el lenguaje `.ava`) o si `.avaui` necesita su propio
  front-end. Dado que `docs/architecture/17_AVAUI_FILE_FORMAT.md` ya
  existe, partir de ahí en vez de inventar de cero.
- `ui/src/parser/`: recorrer el AST/archivo `.avaui` y construir un
  `ComponentTree` real (Fase 2) — properties, slots, ids.
- Casos de error: `.avaui` inválido, componente desconocido (¿falla duro o
  cae a un tipo genérico, igual que hace `LayoutEngine` con `TypeName` no
  reconocido en la Fase 3?). Definir la política y que sea consistente
  con el resto del pipeline.
- Un test end-to-end real: `.avaui` de ejemplo → parser → ComponentTree →
  Layout → Render Tree → Scene Graph → Commands → `HTMLRenderer` →
  HTML final. Esta es la primera vez que las 11 fases se prueban juntas
  con un archivo de verdad en vez de código C++ sintético.

**Entregable:** `ui/src/parser/` con al menos un `.avaui` de ejemplo
corriendo end-to-end hasta HTML. `UIModule::AbiVersion()` bump.

---

## Fase 15 — Resources (fonts/images/icons/localization)

La reseña tiene razón en crearla ya, y en el repo ya está reservada
(`ui/src/resources/README.md`, "Futuro -- carga de recursos consumidos
por render/"). La subo antes que Theme porque un theme sin manera de
resolver una fuente o un ícono es solo una lista de colores.

- `IResourceProvider` (o similar): resolver `path lógico -> recurso real`
  (fuente, imagen, ícono), con al menos un backend "filesystem" (Windows).
- Localization: aunque sea el diseño nomás (formato de string tables), no
  hace falta implementarlo entero — pero si `theme/` o `controls/` van a
  asumir que el texto puede venir de una key localizable, esa decisión
  tiene que existir antes de que `Text`/`Button` (Fase 18) se escriban
  asumiendo texto plano.
- Enganchar con `GdiRenderer::OnDrawImage` (hoy solo BMP, según el
  progreso de Fase 11) — decidir si la carga de PNG/JPEG vive acá
  (Resources) o en el renderer. Documentar la decisión, ya que hoy no
  está escrita en ningún lado.

**Entregable:** interfaz de recursos + un provider funcional + decisión
de localization documentada.

---

## Fase 16 — Theme system

También coincido en crearla ahora, ya reservada
(`ui/src/themes/README.md`).

- Definir qué es un "theme" en términos del pipeline existente: ¿un
  theme setea *defaults* del property bag de `IComponent` antes de
  Layout, o resuelve valores en el paso Render Tree (Fase 6), donde ya se
  leen `backgroundColor`/`borderColor`/etc.? Esto no es un detalle menor:
  cambia si Theme depende de Components o de Render.
- Al menos: color palette, tipografía base, y un mecanismo de
  "override" por componente individual (para que Button pueda pisar el
  theme).
- Sin implementar controles todavía — la reseña tiene razón en no
  adelantarse.

**Entregable:** `ITheme`/`ThemeProvider` + un theme por defecto aplicado
sobre el pipeline existente sin tocar `controls/` (porque no existe
todavía).

---

## Fase 17 — Controls: Button (primer control real)

Ahora sí, con Resources y Theme ya resueltos, arranca `controls/`
(reservada, "No antes de Fase 11" — ya cumplido). Un solo control primero,
a propósito, para no descubrir 6 problemas de interfaz a la vez.

- `Button` como primer caso porque ya tiene tratamiento especial
  documentado en Fase 6 (Render Tree: "Button → Rectangle+Text+children")
  y en Fase 3 (Layout: fallback `ArrangeStack` genérico). Es el control
  que más fases ya "conocen" parcialmente.
- Validar que Theme (Fase 16) y Resources (Fase 15) alcanzan para
  resolverlo sin agregar hooks especiales ad-hoc — si Button necesita algo
  que Theme/Resources no previeron, es señal de volver a esas fases, no de
  parchear Button.
- Evento de click end-to-end: `IEventDispatcher` (Fase 5, ya con
  hit-testing) → callback de usuario. Esta es la primera vez que Events
  se prueba contra un control real en vez de un mock.

**Entregable:** `Button` funcional, con evento de click probado
end-to-end sobre el motor nativo Windows (Fase 11).

---

## Fase 18 — Controls: resto del set base

Recién acá el resto: `Text`, `TextBox`, `Column`/`Row`/`Stack` (ya tienen
tratamiento en Layout, falta el control en sí), `Image`, `CheckBox`,
`RadioButton` — la lista del Toolbox que ya menciona
`core/src/ui/registry.h`.

- Uno por uno, mismo criterio que Button: primero el que ya tiene más
  soporte parcial en fases previas.
- `TextBox` es el más nuevo (input de usuario, no solo output) — vas a
  necesitar foco + teclado de Fase 5 probado de verdad por primera vez acá.

**Entregable:** set base de controles + doc de progreso actualizado por
cada uno.

---

## Fase 19 — Animation

Reservada (`ui/src/animation/README.md`), correctamente pospuesta por la
reseña hasta tener controles reales para animar. Sin esto no hay forma de
validar una animación contra algo visual real.

- Definir si ataca Scene Graph (transform/opacity, Fase 7, que ya tiene
  los campos) vía interpolación entre frames, o si necesita su propio
  scheduler de tiempo.
- Empezar por transform/opacity (ya existen en `ISceneNode`) antes que
  cualquier cosa nueva.

---

## Fase 20 — Integración real con AvaHost y AvaStudio

Acá se cierra la decisión de la Fase 12. Si la Fase 12 decidió que `ui/`
reemplaza (parcial o totalmente) a `core/src/ui`:

- Migrar `avahost/src/rendering/html_renderer.cpp` a consumir el pipeline
  de `ui/` (Parser Fase 14 → ... → `HTMLRenderer` Fase 10) en vez de su
  propio camino actual sobre `core/src/ui`.
- Mismo para `studio/src/engine/engine_bridge.cpp` — el doc de plan ya
  dice "AvaStudio usa el mismo pipeline que producción, sin renderer de
  preview separado". Hoy eso no es cierto todavía; esta fase lo hace
  cierto.
- Si la Fase 12 decidió que conviven permanentemente, esta fase se
  reduce a documentar el límite y verificar que nadie lo cruzó sin querer.

**Esta es la fase de mayor riesgo de todo el plan** — toca código en
producción (`avahost`, `studio`), no solo el módulo nuevo aislado. No
empezarla sin la Fase 12 escrita y sin las Fases 14-18 estables.

---

## Fase 21 — Linux/macOS

Hoy "en estudio" (stub, pausado a propósito). Queda al final del plan a
propósito — retomarlos antes de congelar controls/theme/animation en
Windows solo va a duplicar trabajo si algo cambia.

---

## 2. Resumen para pegar en `AVALANG_UI_PROGRESS.md`

| # | Fase | Depende de | Bloquea |
|---|---|---|---|
| 12 | Decisión convergencia core/ui ↔ ui/ | — | 14, 20 |
| 13 | Congelar interfaces | — | 15-19 |
| 14 | Parser .avaui | 12, 13 | 15-19 |
| 15 | Resources | 14 | 16, 17 |
| 16 | Theme | 15 | 17 |
| 17 | Controls: Button | 15, 16 | 18 |
| 18 | Controls: resto | 17 | 19 |
| 19 | Animation | 18 | — |
| 20 | Integración AvaHost/Studio | 12, 14-18 | — |
| 21 | Linux/macOS | 17-19 (Windows estable) | — |

## 3. Lo que NO cambiaría de la reseña original

- Punto 2 (Controls vacío por ahora): correcto, ya reflejado arriba.
- Punto 3 (Theme, crear la carpeta ya): correcto, ya está creada y
  reservada en el repo — solo faltaba diseñarla, cubierto en Fase 16.
- Punto 4 (Resources): mismo caso, Fase 15.
- Punto 5 (Animation, no implementar todavía): correcto, Fase 19.
- La puntuación de arquitectura/modularidad de la reseña me parece
  razonable — el único ajuste real es que "organización 9.8/10, solo
  revisar core/src/ui" se queda corto: no es un tema de organización de
  carpetas, es una decisión de producto (¿dos motores de UI en paralelo,
  para siempre o temporalmente?) que conviene resolver por escrito antes
  de seguir apilando fases arriba del motor nuevo.
