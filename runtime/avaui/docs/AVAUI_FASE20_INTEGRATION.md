# Fase 20 — Integración con AvaHost y AvaStudio

**Objetivo original** (`AVAUI_PLAN_FASE12_PLUS.md`): cerrar la decisión
de la Fase 12 migrando `avahost`/`avastudio` para que consuman el
pipeline nuevo (`avaui/`) en vez de `core/src/ui` vía `avalang.h`.

**Status:** ✅ **CERRADO** — 20.0 (puente VM ↔ avaui) + 20.1 (adapter
estático + CLI) + 20.2 (swap detrás de flag `useAvauiEngine`, default
OFF). El camino viejo `HtmlRenderer` + `RuntimeHost`/`AvaComponent`
queda intacto; rollback instantáneo vía flag. Gaps abiertos
documentados abajo (items 4, 5, 8). Ver `AVAUI_FASE20_2_PLAN.md`
para el detalle de las 5 sub-fases + cierres de Prioridades 1-5.

---

## Hallazgo real (no anticipado por el plan): falta un puente VM ↔ avaui

El propio plan ya marcaba esta fase como *"la de mayor riesgo de todo
el plan"* y pedía no empezarla sin las Fases 14-19 estables. Al ir a
implementarla contra el código real de `avahost/src/runtime/runtime_host.h`,
apareció un gap estructural que el plan no había visto:

`RuntimeHost` (la única capa de AvaHost que toca `avalang.h`) no es un
simple wrapper de parseo — es quien conecta el árbol de componentes con
una **VM de AvaLang viva**:

- `BindState(stateJson)` — publica el bloque `state` como globals de la VM.
- `BindCodeBehind(methodsText)` — compila+corre el bloque `code`/`methods`
  de la página contra esa VM (`func OnGuardarClick() ... end` se vuelve
  invocable).
- `InvokeHandler`/`InvokeHandlerIfDefined` — llama un handler
  (`data-handler="OnGuardarClick"`) o un hook de ciclo de vida
  (`OnLoad`/`OnShow`/...) contra esa VM.
- `EvalPropertyExpr` — evalúa el texto crudo de una propiedad
  (`value = counter`) como expresión contra los globals actuales, así
  `HtmlRenderer::RenderOptions::evalText` puede mostrar el valor mutado
  en vez del texto literal.
- `ExportStateJson` — lee los globals de vuelta para persistir estado
  entre requests.

Es decir: **el 100% del comportamiento dinámico de AvaHost
(`state`, `code`, eventos, ciclo de vida) pasa por una VM de AvaLang**,
no por el árbol de componentes en sí. `avaui/` (Fases 1-19) es,
deliberadamente, un motor **sin ningún lazo con la VM**:

- `parser::AvauiParser` (Fase 14) guarda `state`/`code` como texto
  crudo sin parsear — mismo gap que ya documentan `AVAUI_FASE14_PARSER.md`
  y `AVAUI_FASE18_CONTROLS.md`.
- `state::IState` (Fase 4) es una celda C++ (`PropertyValue` + lista de
  `ChangeHandler`) totalmente aislada de `ava::Value`/`AvaVM` — no hay
  ningún camino para que un `func OnGuardarClick()` de un script `.ava`
  mute un `IState` de `avaui/`, ni al revés.
- `events::IEventDispatcher` (Fase 5) dispara `IEventHandler::OnEvent`
  en C++ — no invoca funciones AvaLang por nombre.
- `animation::WireAnimations` (Fase 19.4) sí sabe conectar un
  `trigger` a un `IState*`, pero ese `IState*` todavía tiene que
  existir y estar sincronizado con algo — y hoy nada lo sincroniza con
  una VM.

**Conclusión:** reemplazar `RuntimeHost`/`HtmlRenderer` por `avaui/` tal
como está hoy no es "cambiar el renderer" — es *perder* `state`,
`code`, eventos, y ciclo de vida para *toda* app existente de AvaHost,
en silencio (la página seguiría renderizando, solo que estática). Eso
es exactamente el tipo de regresión de producción que el plan advertía
evitar. Migrar de verdad primero necesita un puente VM ↔ `avaui`
(candidato a **Fase 20.0**, no hecho todavía): algo que exponga
`ava::Value`/`AvaVM` del lado de `avaui/` — probablemente un
`state::IState` respaldado por un global de VM en vez de una celda C++
aislada, más un mecanismo para que `events`/`animation` inviertan una
llamada AvaLang por nombre, simétrico a `RuntimeHost::InvokeHandler`.

Esto no invalida la Fase 12 (`ui/` sigue siendo, a futuro, el
reemplazo de `core/src/ui`) — solo corrige el orden: **la Fase 20 tal
como estaba escrita depende de una Fase 20.0 que no existía**, igual
que la Fase 14 dependió de descubrir el gap de parser en su momento.

---

## Qué se entrega ahora (20.1) — seguro, aditivo, cero riesgo

Dado el hallazgo de arriba, 20.1 se acota a exactamente lo que
`html_renderer.h` ya reconocía como un caso legítimo sin VM: **preview
estático** (`RenderOptions::evalText` — *"a caller with no VM/state to
bind against, e.g. `avahost build`'s static preview"*). Un documento
`.avaui` sin `state`/`code`/handlers se renderiza igual de correcto por
cualquiera de los dos motores, porque no hay nada dinámico que el motor
nuevo todavía no sepa hacer.

**Nuevo, aislado, no toca nada existente:**

- `avahost/src/rendering/ui_pipeline_static_renderer.h` / `.cpp`:
  `RenderAvauiStatic(source, options, outHtml, outError)` — corre el
  pipeline completo de `avaui/` (`parser::AvauiParser` → `RenderTheme`
  → `LayoutEngine` → `IRenderTree` → `ISceneGraph` → `SceneCommandWalker`
  → `HTMLRenderer`) contra un string de texto `.avaui`, sin ningún lazo
  con `RuntimeHost`/VM. Nunca lanza excepciones fuera de su propia
  frontera (`try/catch` interno → `bool` + `outError`).
- `avahost render-static <file.avaui> [output.html]` (nuevo comando
  CLI, `cli_commands.h/.cpp` + dispatch en `main.cpp`) — usa el
  adapter de arriba. Sin `[output.html]`, imprime el HTML a stdout.
- `avahost/CMakeLists.txt`: `if(TARGET avalang_ui)` — el comando y su
  fuente solo se compilan cuando la configuración tiene
  `AVA_BUILD_UI=ON` (que ya se agrega *antes* que `avahost` en el
  `add_subdirectory` del root `CMakeLists.txt`, así que el target
  existe a tiempo). Con `AVA_BUILD_UI=OFF` (el default), `avahost`
  compila exactamente igual que antes de esta fase, y
  `CmdRenderStatic` en runtime simplemente informa que hace falta
  reconfigurar con `-DAVA_BUILD_UI=ON`.

**Deliberadamente NO tocado en 20.1** (para no arriesgar nada en
producción, tal como pedía el plan):
- `runtime_host.cpp/.h`
- `rendering/html_renderer.cpp/.h`
- `rendering/event_binder.cpp/.h`
- `web/server/app.cpp` (el request path real de `avahost run`/`watch`)
- Nada de `avastudio/` — mismo argumento aplica ahí (`engine_bridge.cpp`
  también depende de una VM para el Designer, no solo del árbol de
  componentes).

---

## Fase 20.0 — Puente VM ↔ avaui (cerrado)

Cierra exactamente el gap que la sección de arriba identificó: un
mecanismo para que `state::IState`/eventos de `avaui/` lean y escriban
contra una `AvaVM` real, simétrico a lo que `RuntimeHost` ya hace para
`core/src/ui`. Vive enteramente del lado de AvaHost — `runtime/avaui/`
no se toca (`AVAUI_ARCHITECTURE_FREEZE_PLAN.md`, "Do NOT couple AvaUI
with AvaHost"), salvo un gap real encontrado *de camino* que sí vivía
dentro de `avaui/` y que no tiene nada que ver con la VM (ver más
abajo).

**Nuevo, aditivo:**

- `avahost/src/rendering/ui_vm_state_bridge.h/.cpp` — `VmBackedState`,
  una implementación **nueva** de la interfaz `state::IState` (Fase 4,
  congelada en Fase 13 — la firma se congeló, no el conjunto de clases
  que pueden implementarla) respaldada por un global de `RuntimeHost`
  en vez de la `PropertyValue` aislada de `StateImpl`. `VmStateBridge`
  crea una `VmBackedState` por cada key del bloque `state` de un
  `.avaui`, usando solo la API pública ya existente de `RuntimeHost`
  (`BindState`/`ExportStateJson`, sin agregar ningún método nuevo ahí)
  — `Bind()` una vez al inicio, `RefreshAll()` después de cualquier
  `InvokeHandler`/`InvokeHandlerIfDefined` para re-sincronizar cada
  celda desde los globals que el handler mutó directamente.
- `avahost/src/rendering/ui_vm_event_bridge.h/.cpp` —
  `WireVmClickHandlers`, simétrico a `RuntimeHost::InvokeHandler`:
  recorre el árbol de componentes buscando la propiedad `click =
  "Handler"` (misma convención que el `EventBinder` del pipeline viejo
  ya usa) y suscribe un handler al evento `Click` de
  `IEventDispatcher` que invoca la VM y refresca el state bridge.
- `avahost/src/rendering/ui_pipeline_dynamic_renderer.h/.cpp` —
  `RenderAvauiDynamic(RuntimeHost&, source, options, ...)`: extiende
  `RenderAvauiStatic` (Fase 20.1) con bind de `state`/`code`, wiring de
  click handlers, y el hook `OnLoad` (opcional, vía
  `InvokeHandlerIfDefined`), antes de correr el mismo pipeline
  Theme→Layout→RenderTree→SceneGraph→HTML.
- Comando CLI nuevo `avahost render-dynamic <file.avaui> [output.html]
  [--project <dir>]`, mismo patrón aislado/opt-in que `render-static`
  — compilado solo bajo `if(TARGET avalang_ui)`, con
  `AVA_BUILD_UI=OFF` (default) `avahost` compila exactamente igual que
  antes de esta fase.

**Gap real encontrado dentro de `avaui/` mismo, cerrado de camino**
(no relacionado con la VM, pero bloqueaba que el bridge de eventos
tuviera algo concreto a lo que suscribirse): `EventType::Click` existe
en `IEvent.h` desde la Fase 5, pero `EventDispatcher::PollInput` nunca
lo sintetizaba — solo emitía `MouseDown`/`MouseUp`/`MouseMove`. Se
agregó el tracking mínimo (`mouseDownTarget_`, miembro interno, no
parte de ninguna interfaz pública) para detectar un click clásico
(press y release sobre el mismo componente) y despachar `Click` en ese
caso. Cero cambios de firma en `IEventDispatcher`/`IEvent` — no
requiere bump de `UIModule::AbiVersion()`.

**Qué NO cierra esta fase, a propósito:** resolución de expresiones de
propiedad. Una propiedad cuyo texto crudo referencia state
(`value = counter`, o una expresión completa como `"Contador: " +
counter`) sigue renderizando el texto literal del `.avaui` — el bridge
conecta la VM (state y handlers ahora existen y son alcanzables), pero
nada todavía re-escribe el árbol de componentes usando esas celdas.
`IComponent` (Fase 2, congelada en Fase 13) no expone forma de
enumerar las propiedades de un componente, solo `GetProperty(name)`
con un nombre que el caller ya conoce — resolver "cualquier propiedad
que referencie state" de forma genérica exigiría ampliar esa interfaz
congelada o adivinar nombres de propiedad por tipo de control; ninguna
de las dos se hizo aquí. Queda como trabajo separado, real y no
trivial, distinto de la conectividad que esta fase sí cierra.

**Validación de esta sesión:** `g++ -std=c++20 -fsyntax-only` limpio
sobre los tres archivos nuevos de `avahost/` y sobre `EventDispatcher.cpp`
modificado, contra los include paths reales del repo
(`avahost/src`, `avaui/src`, `avalang/api/include`, `avalang/src`,
`avalang`), con `nlohmann-json3-dev`/`libglm-dev` instalados vía apt.
**No compilado con MSVC real ni enlazado de punta a punta** (sin
toolchain Windows en este sandbox, mismo estado que todas las fases
anteriores) — a diferencia de Fase 14/18, esta sesión no llegó a
correr un binario real; solo syntax-check.

---



---

## Qué falta para 20.2+ (no hecho en esta sesión)

1. ~~**Fase 20.0 — Puente VM ↔ avaui**~~ — **cerrado esta sesión**, ver
   sección "Fase 20.0 — Puente VM ↔ avaui (cerrado)" arriba.
2. ~~Migrar `RenderAvaUiRoute` (dentro de `web/server/app.cpp`) para
   que use el pipeline de `avaui/` + el puente nuevo en vez de
   `HtmlRenderer` + `RuntimeHost::BindState/BindCodeBehind/InvokeHandler`
   directamente contra `AvaComponent`.~~ **Cerrado en 20.2.5** —
   branch swap detrás de flag `useAvauiEngine` (default OFF,
   `--use-avaui-engine` CLI). El camino viejo queda intacto.
3. ~~`import`/resolución de componentes (`AVAHOST_PROGRESS.md` fila 8,
   ya marcada ⬜ **incluso en el motor viejo**).~~ **Cerrado en 20.2.3
   + Prioridad 4** — `UiComponentResolver` existe (20.2.3) y ahora está
   wireado en el branch swap de `app.cpp::RenderAvaUiRoute` vía
   `UiPipelineRenderOptions.projectRoot`/`componentsDir` (aditivos).
   `ui_pipeline_dynamic_renderer.cpp` invoca el resolver antes del
   `BindWithOverlay` y mergea `parsed.state` con el state importado
   (first-writer-wins via `MergeStateMap`). Funciona en
   `RenderAvauiDynamicWithState` y `RenderAvauiDynamicWithLayoutAndState`.
4. `avastudio/src/engine/engine_bridge.cpp` — mismo tipo de dependencia
   de VM para el Designer en vivo; ni siquiera empezado a analizar en
   esta sesión (fuera de alcance de 20.1/20.0/20.2).
5. ~~Resolución de expresiones de propiedad contra el state ya conectado
   (`value = counter` renderizando el valor real).~~ **Cerrado en
   Prioridad 5** (enfoque C-1 mínimo, freeze-safe): `VmStateBridge::EvalIdentifier`
   lookup exacto en `states_`; `IRenderTree::SetEvalText(...)` virtual
   puro aditivo (ABI bumpeado a 21); `RenderTree` aplica `Eval()` en
   todas las propiedades `String` (`backgroundColor`, `borderColor`,
   `color`, `text`, `label`, `placeholder`, `src`, `fontName`);
   `RenderTreeFragment` setea `SetEvalText` antes del `Build` (pasando
   `&stateBridge` para page, `nullptr` para layout). **Limitación
   documentada:** solo identificadores exactos, sin concatenación o
   aritmética (mismo shape que `html_renderer.h::evalText` cuando no
   había VM binding completo).
6. ~~Bug del branch swap: `pendingHandler` (vía `HandleEventRoute`)
   ignorado por el adapter.~~ **Cerrado en Prioridad A** — clicks del
   browser ahora ejecutan el handler de VM: `RenderAvauiDynamicWithState`/
   `RenderAvauiDynamicWithLayoutAndState` aceptan `pendingHandler` y
   llaman `host.InvokeHandler(pendingHandler, ...)` + `stateBridge.RefreshAll()`
   después del `OnLoad`. Si falla → 500 con detalle (mismo contrato que
   el motor viejo).
7. ~~`data-handler` no emitido en el HTML del swap (los clicks no se
   bindean al cliente).~~ **Cerrado en Prioridad 3** — Gap A resuelto:
   `IRenderNode::ClickHandler()` puro virtual + aditivo (ABI 20);
   propagado por `RenderCommand.drawRect/drawEllipse/drawText.clickHandler`,
   `IRenderCommandSink/IRenderer::DrawRectangle/DrawEllipse/DrawText`
   (default `""` para no romper callers), `SceneCommandWalker` lee
   `renderNode->ClickHandler()` y lo pasa al `sink.Draw*`, `RenderTree`
   lee la propiedad `"click"` del `IComponent` y la inyecta via
   `SetClickHandler`. `HTMLRenderer` emite `data-handler="X"` cuando
   el handler no está vacío. `UIModule::AbiVersion()` bumpeado a 20.
8. ~~`BuildHeadTags`/`EventScriptTag`/`HotReloadScriptTag`/`BuildConsoleScript`
   no inyectados en el branch swap.~~ **Cerrado en Prioridad 2** — Gap B
   resuelto: el branch swap en `app.cpp::RenderAvaUiRoute` ahora setea
   `renderOptions.title = DeriveTitleFromPath(...)`,
   `renderOptions.extraHead = BuildHeadTags(manifest) + HotReloadScriptTag()`
   si watch, y post-render computa
   `bodyEndTags + eventScript + consoleScript(consoleOutput)` e
   inyecta manualmente antes de `</body>\n</html>\n`. `consoleOutput`
   se computa después del render y se reemplaza en `extraBodyEnd`.
9. Demo test (`tests/integration/avaui_demos/UiPipelineFase20_2Demo.cpp`) solo
   valida parse + render de los fixtures; no resuelve imports ni
   compone layouts (esos viven en el adapter de avahost/ y requieren el
   adapter completo con VM).

---

## Archivos nuevos/tocados

**Nuevos (Fase 20.1):**
- `avahost/src/rendering/ui_pipeline_static_renderer.h`
- `avahost/src/rendering/ui_pipeline_static_renderer.cpp`

**Nuevos (Fase 20.0):**
- `avahost/src/rendering/ui_vm_state_bridge.h/.cpp`
- `avahost/src/rendering/ui_vm_event_bridge.h/.cpp`
- `avahost/src/rendering/ui_pipeline_dynamic_renderer.h/.cpp`

- `runtime/avaui/docs/AVAUI_FASE20_INTEGRATION.md` (este documento —
  vive en `avaui/docs/` porque continúa la numeración de fases de
  `AVAUI_PLAN_FASE12_PLUS.md`, aunque el código tocado sea de
  `avahost/`)

**Tocados:**
- `avahost/CMakeLists.txt` (bloque `if(TARGET avalang_ui)`, fuentes de
  20.0 agregadas)
- `avahost/src/cli_commands.h` (declaración `CmdRenderStatic` +
  `CmdRenderDynamic`)
- `avahost/src/cli_commands.cpp` (implementación de ambos comandos +
  líneas en `PrintUsage`)
- `avahost/src/main.cpp` (dispatch de `render-static`/`render-dynamic`)
- `runtime/avaui/src/events/EventDispatcher.h/.cpp` (síntesis de
  `EventType::Click`, ver sección Fase 20.0)
- `runtime/avaui/docs/AVALANG_UI_PROGRESS.md` (fila 20)
- `avahost/docs/AVAHOST_PROGRESS.md` (filas 12-13)

**No compilado con MSVC real** (sin toolchain Windows en este sandbox,
igual que todas las fases anteriores) — Fase 20.1: código escrito y
revisado, no ejecutado. Fase 20.0: `g++ -std=c++20 -fsyntax-only`
limpio contra los include paths reales del repo (ver detalle en la
sección Fase 20.0 arriba), tampoco enlazado ni ejecutado.
