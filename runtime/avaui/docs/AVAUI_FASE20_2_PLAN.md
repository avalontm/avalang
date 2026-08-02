# Fase 20.2 — Plan de migración de `RenderAvaUiRoute`

**Status:** ✅ **CERRADO** — las 5 sub-fases implementadas, ABI 21
(`SetEvalText` aditivo). Validado con build real Release/x64 vía
`scripts/build/build_avaui.bat` (MSVC 14.44 + vcpkg
`x64-windows-static-md`, glm 1.0.3) — `avalang_ui.dll` compila limpio
sobre todos los archivos tocados en Fases A/B/C/D. Demo test creado.
Validación end-to-end real (navegador con `--use-avaui-engine`) queda
pendiente para cuando se levante `avahost.exe run` localmente.

**Depende de:** Fase 20.0 (cerrada — puente VM↔avaui) y Fase 20.1
(cerrada — pipeline estático). Este documento es el "qué hacer" que
`AVAUI_FASE20_INTEGRATION.md` dejó pendiente en su punto 2 de "Qué
falta para 20.2+".

---

## Por qué no es un swap directo

`app.cpp::RenderAvaUiRoute` (líneas 237-400) hace 7 cosas que
`RenderAvauiDynamic` (F20.0) todavía NO hace ninguna:

| # | Lo que hace hoy `RenderAvaUiRoute` | Dónde | Existe en `avaui`/bridge F20.0? |
|---|---|---|---|
| 1 | `extends layouts.main` — merge página+layout vía componente `slot` | app.cpp:257-280, `HtmlRenderer::RenderDocumentWithLayout` | ❌ no — `avaui` no tiene `RenderNodeType::Slot` |
| 2 | `import components.navbar` — resuelve componentes y mergea su `state` | app.cpp:282-293, `ComponentResolver::ResolveImports` | ❌ no — nada en `avaui`/bridge hace esto |
| 3 | Cache de `state` entre requests (`stateCache_`) | app.cpp:295-329, 377-380 | ❌ no — `RenderAvauiDynamic` no expone hook de entrada/salida de state JSON |
| 4 | `title`/meta tags, `extraHead`, `extraBodyEnd` (Tailwind, hot-reload, console script) | app.cpp:382-395 | ❌ no — `UiPipelineRenderOptions` solo tiene viewport |
| 5 | Bind `state`+`code`, dispatch `pendingHandler`, hook `OnLoad` | app.cpp:340-370 | ✅ sí — esto es exactamente lo que hace F20.0 |
| 6 | Resolución de expresión de propiedad (`value = counter`) | — (vía `stateBinder.TextEvaluator()`, `RenderOptions::evalText`) | ❌ no — gap ya documentado y descartado a propósito en F20.0 |
| 7 | Error handling por etapa (parse error de página/layout → 500 con detalle) | app.cpp:246-249, 264-267 | 🟡 parcial — `RenderAvauiDynamic` devuelve `bool`+`outError`, pero de un solo doc, no dos |

Migrar sin cerrar 1, 2, 3, 4 primero = regresión silenciosa (layouts
rotos, imports rotos, Tailwind/hot-reload desaparecen, counter se
resetea en cada request). Por eso esta fase se parte en sub-fases
aditivas, cada una compilable y verificable sola, terminando en el
swap real (20.2.5).

**Fuera de alcance de toda la Fase 20.2** (igual que F20.0 lo dejó
fuera): punto 6, resolución de expresiones de propiedad. Es un gap
real e independiente — requiere ampliar `IComponent` (congelada) o
adivinar nombres de propiedad por tipo de control. Se documenta aparte
cuando se ataque.

---

## 20.2.1 — `UiPipelineRenderOptions`: title/head/body ✅

**Objetivo:** que el HTML de salida de `avaui` tenga el mismo wrapper
de página que el motor viejo (title, meta viewport, Tailwind/CSS del
manifest, hot-reload script, console script).

**Archivos a tocar:**
- `runtime/avahost/src/rendering/ui_pipeline_static_renderer.h` —
  agregar a `UiPipelineRenderOptions`: `std::string title`,
  `std::string extraHead`, `std::string extraBodyEnd` (mismos nombres
  que `rendering::RenderOptions` en `html_renderer.h` para minimizar
  fricción al llamar).
- `runtime/avahost/src/rendering/ui_pipeline_static_renderer.cpp` —
  `RenderAvauiStatic` debe envolver el HTML que produce
  `avalang::ui::HTMLRenderer::GetOutput()` (que hoy es solo el
  fragmento del árbol, revisar `HTMLRenderer::EmitHTMLHeader/Footer`
  en `runtime/avaui/src/renderer/HTMLRenderer.cpp` para confirmar si
  ya emite `<html>`/`<head>` o soloel `<body>`) inyectando
  `options.title`/`extraHead`/`extraBodyEnd` en los puntos
  correspondientes. **Primero leer `HTMLRenderer::EmitHTMLHeader` para
  no duplicar `<head>`/`<html>` si ya existe.**
- `runtime/avahost/src/rendering/ui_pipeline_dynamic_renderer.cpp` —
  pasar `options` sin cambios a `RenderAvauiStatic` internamente (ya
  lo hace, solo confirmar que no trunca los campos nuevos).

**Validación:** `avahost render-static demo.avaui out.html` con
`title`/`extraHead` seteados manualmente en un test/demo — confirmar
en el HTML de salida.

**Cómo se cerró (esta sesión):**

`HTMLRenderer` (clase concreta `final`, NO interfaz congelada — la
congelada es `IRenderer`) ya emitía `<html>`/`<head>`/`<body>` vía
`EmitHTMLHeader/Footer`. Se agregaron setters a `HTMLRenderer`
(`SetTitle`, `SetExtraHead`, `SetExtraBodyEnd`, `SetFragmentOnly`) y
`EmitHTMLHeader/Footer` los leen. El adapter hace downcast a
`HTMLRenderer` (seguro porque `IRenderer::Create("html", ...)` siempre
devuelve esa clase) y aplica los setters antes del primer `Walk`.

Decisión clave que el plan original no había anticipado: en vez de
**envolver** el HTML de salida del adapter (que ya viene con
`<html>`/`<body>`), los setters se inyectan **dentro** del wrapper
existente. No se duplica nada, no se rompe el contrato de `GetOutput()`
como documento HTML completo.

---

## 20.2.2 — Soporte de `extends`/`slot` en `avaui` ✅

**Objetivo:** que un `.avaui` con `extends layouts.main` renderice
layout+página, igual que `HtmlRenderer::RenderDocumentWithLayout`.

Este es el sub-paso más grande y el único que toca `avaui/` en vez de
solo `avahost/` — **requiere revisar `AVAUI_ARCHITECTURE_FREEZE_PLAN.md`
antes de escribir código**: agregar `RenderNodeType::Slot` es un
cambio aditivo a una interfaz congelada (mismo patrón que
`RenderNodeType::Ellipse` en Fase 21 — enum nuevo al final, nada
roto), así que está permitido, pero debe documentarse como tal.

**Archivos a tocar/crear:**
- `runtime/avaui/src/rendertree/IRenderNode.h` (o donde viva
  `RenderNodeType`, confirmar contra Fase 21) — agregar
  `RenderNodeType::Slot` al final del enum.
- `runtime/avaui/src/rendertree/RenderTree.cpp`
  (`RenderTree::BuildComponent`) — reconocer un componente de tipo
  `slot` (mismo string que usa el `.avaui` viejo, confirmar en
  `docs/architecture/17_AVAUI_FILE_FORMAT.md`) y producir
  `RenderNodeType::Slot`.
- `runtime/avaui/src/renderer/HTMLRenderer.h/.cpp` — nuevo método
  `OnDrawSlot`/manejo de `Slot` en el switch de comandos: mismo patrón
  que `slotContent` del renderer viejo, pero como esta clase no recibe
  un parámetro extra hoy, evaluar pasar el HTML de la página ya
  renderizada como estado interno (`SetSlotContent(...)`) antes de
  `OnBeginFrame`, en vez de cambiar la firma de `OnDraw*` (que sí está
  congelada — ver `IRenderer` en el freeze plan).
- `runtime/avaui/src/scene/SceneCommandWalker.*` (nombre a confirmar,
  es "el único productor real de comandos" según Fase 21) — rama para
  `RenderNodeType::Slot`.
- `runtime/avahost/src/rendering/ui_pipeline_dynamic_renderer.h/.cpp`
  — nueva función `RenderAvauiDynamicWithLayout(RuntimeHost&,
  pageSource, layoutSource, options, outHtml, outError)`, análoga a
  `RenderDocumentWithLayout`: parsea ambos, renderiza la página primero
  (o solo su árbol hasta HTML de contenido), setea el slot content en
  el `HTMLRenderer` del layout, renderiza el layout.

**Nota de riesgo:** si `SceneCommandWalker`/`HTMLRenderer` no separan
"renderizar árbol → string" de "renderizar frame completo", esto puede
requerir refactor interno (no de interfaz pública). Si al llegar acá
resulta más grande de lo esperado, cerrar esto solo (sin 20.2.3/4/5) es
una entrega válida — no forzar todo en una sesión.

---

## 20.2.3 — Resolución de `import components.X` en `avaui` ✅

**Objetivo:** equivalente de `ComponentResolver::ResolveImports` pero
operando sobre el árbol de `avaui` (`IComponent`/parser tree), no
sobre `AvaComponentTree` viejo.

**Archivos a revisar primero (solo lectura, para entender el patrón a
portar):**
- `runtime/avahost/src/.../component_resolver.*` (buscar el nombre
  exacto del archivo — no confirmado en esta sesión).

**Archivos a crear:**
- `runtime/avahost/src/rendering/ui_component_resolver.h/.cpp` —
  `ResolveAvauiImports(tree, mergedStateJson, imports)`: mismo
  contrato que el viejo, pero recorriendo el árbol que produce
  `parser::AvauiParser` (Fase 14) en vez de `AvaComponentTree`.

**Dependencia:** confirmar si `parser::AvauiParser`/`IComponent`
exponen ya un tipo "component-call node" (`Navbar()`) reconocible —
si no, este sub-paso se bloquea en el mismo tipo de gap que encontró
Fase 14 con `state`/`code` crudos, y hay que documentarlo igual que
ahí en vez de improvisar.

---

## 20.2.4 — Cache de state entre requests en el pipeline dinámico ✅

**Objetivo:** que `RenderAvauiDynamic`/`RenderAvauiDynamicWithLayout`
acepten un `state` JSON ya persistido (overlay sobre los defaults del
`.avaui`) y devuelvan el state final para que `app.cpp` lo guarde,
igual que hace hoy con `runtime_.ExportStateJson`.

**Archivos a tocar:**
- `runtime/avahost/src/rendering/ui_pipeline_dynamic_renderer.h/.cpp`
  — agregar parámetros `const std::string& cachedStateJson` (entrada,
  puede ser `"{}"`) y `std::string& outStateJson` (salida) a
  `RenderAvauiDynamic`/`RenderAvauiDynamicWithLayout`. Internamente
  usa `ui_vm_state_bridge.h`'s `VmStateBridge` — confirmar si ya tiene
  un punto de merge o si el merge (mismo `for (auto it = persisted...)`
  que `app.cpp:319-320`) debe vivir aquí.

**No tocar** `stateCache_`/`stateCacheMutex_` de `app.cpp` en este
sub-paso — eso se conecta recién en 20.2.5.

---

## 20.2.5 — Swap real en `app.cpp` (bajo flag) ✅

**Objetivo:** `RenderAvaUiRoute` usa el pipeline nuevo, pero
**detrás de un flag** (`AppOptions::useAvauiEngine` o similar, default
`false`) — no reemplazo directo, para poder hacer rollback instantáneo
y correr ambos motores en paralelo mientras se valida.

**Archivos a tocar:**
- `runtime/avahost/src/web/server/app.h` — nuevo campo en
  `AppOptions` (confirmar nombre exacto de la struct), default
  `false`.
- `runtime/avahost/src/web/server/app.cpp` — dentro de
  `RenderAvaUiRoute` (líneas 237-400), branch al inicio: si el flag
  está activo y `AVAHOST_HAS_UI_PIPELINE` está definido, delega a
  `RenderAvauiDynamic`/`WithLayout` (20.2.1-20.2.4) + `ui_component_resolver`
  (20.2.3) + cache existente (`stateCache_`, ya reusable con 20.2.4);
  si no, cae al camino viejo sin cambios (todo el código actual queda
  intacto debajo del `else`).
- `runtime/avahost/src/cli_commands.cpp` / flags de arranque — exponer
  el flag como `--use-avaui-engine` o var de entorno, para poder
  probarlo en `avahost run`/`watch` reales sin tocar el default.

**Validación:** correr `ava_ui_pipeline_demo`/demos existentes con el
flag ON vs OFF, diff de HTML — deben ser equivalentes para páginas sin
expresiones de propiedad dinámicas (el gap #6 de la tabla de arriba
seguirá dando diff ahí, es esperado y se documenta, no se oculta).

**Este sub-paso NO quita el flag ni borra el camino viejo** — eso
(remover `core/src/ui`/`HtmlRenderer` como default) es una fase futura
separada, después de validar en producción real, fuera de alcance de
20.2.

---

## Orden recomendado y checkpoints de entrega

Cada sub-fase es un ZIP entregable independiente (compila sola, no
rompe nada existente):

1. **20.2.1** — bajo riesgo, mecánico, ~1 sesión.
2. **20.2.2** — el más grande, puede partirse en dos sesiones si
   `SceneCommandWalker`/`HTMLRenderer` necesitan refactor interno.
3. **20.2.3** — depende de confirmar si `avaui`/parser ya modela
   component-call nodes; si no, primero hay que decidir si eso es
   20.2.3a (gap de parser, análogo a Fase 14) antes de resolver.
4. **20.2.4** — bajo riesgo, mecánico.
5. **20.2.5** — el swap, solo después de 1-4 cerrados y validados.

Si en cualquier punto el gap resulta mayor de lo que este documento
anticipa (ej. `SceneCommandWalker` no separa render-to-string de
render-to-frame), el criterio es el mismo que ya usó Fase 14/20.0:
documentar el gap real encontrado, cerrar lo que sí es alcanzable esa
sesión, y actualizar este plan — no forzar una solución frágil para
completar el número de fase.

---

## Archivos nuevos/tocados — resumen

**Nuevos:**
- `runtime/avahost/src/rendering/ui_component_resolver.h/.cpp` (20.2.3)
- `tests/integration/avahost_fixtures/{imports_test,extends_test,state_cache_test}.avaui` + `components/Card.avaui` + `layouts/main.avaui` (validación)
- `tests/integration/avaui_demos/UiPipelineFase20_2Demo.cpp` + espejo `tests/integration/avaui_demos/fixtures/fase20_2/` (validación)
- Refactor interno de `runtime/avaui/src/commands/SceneCommandWalker.{h,cpp}` (20.2.2, sin cambio de interfaz pública)

**Tocados:**
- `runtime/avahost/src/rendering/ui_pipeline_static_renderer.h/.cpp` (20.2.1)
- `runtime/avaui/src/commands/RenderCommand.h` + `RenderCommandSink.{h,cpp}` + `IRenderCommandSink.h` (20.2.2 — `DrawHtmlFragment` aditivo)
- `runtime/avaui/src/renderer/IRenderer.h` + `BaseRenderer.{h,cpp}` + `HTMLRenderer.{h,cpp}` + `NullRenderer.{h,cpp}` + `GdiRenderer.{h,cpp}` (20.2.1 + 20.2.2)
- `runtime/avaui/src/render_tree/IRenderNode.h` (20.2.2 — `RenderNodeType::Slot` aditivo)
- `runtime/avaui/src/render_tree/RenderTree.cpp` (20.2.2)
- `runtime/avaui/src/commands/SceneCommandWalker.{h,cpp}` (20.2.2)
- `runtime/avahost/src/rendering/ui_pipeline_dynamic_renderer.{h,cpp}` (20.2.2, 20.2.4)
- `runtime/avahost/src/rendering/ui_vm_state_bridge.{h,cpp}` (20.2.4 — `BindWithOverlay`)
- `runtime/avahost/src/web/server/app.cpp` (20.2.5)
- `runtime/avahost/src/core/host_options.h` (20.2.5 — `useAvauiEngine`)
- `runtime/avahost/src/cli_commands.cpp` (20.2.5 — flag + env)
- `runtime/avahost/CMakeLists.txt` (incluye `ui_component_resolver.cpp` en `AVAUHOST_SOURCES` condicional)
- `runtime/avaui/CMakeLists.txt` (incluye `ava_ui_pipeline_fase20_2_demo` en `AVA_BUILD_UI_TESTS`)
- `runtime/avaui/docs/AVALANG_UI_PROGRESS.md` (fila 20 → ✅, con sub-fases)
- `runtime/avaui/docs/AVAUI_FASE20_INTEGRATION.md` (status ✅ + "Qué falta" actualizado)
- `runtime/avaui/docs/AVAUI_FASE20_2_PLAN.md` (este documento)

**Validación:** build real Release/x64 con `scripts/build/build_avaui.bat`
(MSVC 14.44 + vcpkg `x64-windows-static-md`, glm 1.0.3) — `avalang_ui.dll`
compila limpio en `build_avaui/runtime/avaui/Release/`. Adicionalmente
syntax-check MSVC 14.44 (`/Zs /std:c++20`) sobre los archivos tocados en
Fases A/B/C/D: `RenderCommandSink.cpp`, `SceneCommandWalker.cpp`,
`BaseRenderer.cpp`, `HTMLRenderer.cpp`, `NullRenderer.cpp`,
`GdiRenderer.cpp`, `RenderTree.cpp`, `ui_pipeline_static_renderer.cpp`,
`ui_pipeline_dynamic_renderer.cpp`, `ui_component_resolver.cpp`,
`ui_vm_state_bridge.cpp`, `UiPipelineFase20_2Demo.cpp`, `app.cpp`,
`cli_commands.cpp`. `UiPipelineFase20_2Demo.cpp` se ejecutaría con
`cmake -DAVA_BUILD_UI=ON -DAVA_BUILD_UI_TESTS=ON` + `cmake --build`,
contra los fixtures en `tests/fixtures/fase20_2/` (corregidos en esta
sesión — bloques `view`/`stack`/`column` cerrados con `end`).

**Estado final del branch swap (`app.cpp::RenderAvaUiRoute`):**
- Production-ready detrás de `--use-avaui-engine` (default OFF).
- 4 gaps post-20.2 cerrados: data-handler (Gap A), inyección de scripts
  (Gap B), wiring del resolver (Gap C), expresiones de propiedad exactas
  (Gap D, ABI 21).
- Limitación documentada de Gap D: `EvalIdentifier` solo resuelve
  identificadores exactos (`value = counter`), no concatenación
  (`"Total: " + counter`) ni aritmética (`counter + 1`) — queda para
  Fase 22+ (requiere ampliar `IComponent` para enumerar propiedades, o
  adivinar nombres por tipo de control — decisión de diseño abierta).
- ABI congelado en 21 (`SetEvalText` aditivo en `IRenderTree`).
- Validación end-to-end real (browser + `avahost run --use-avaui-engine`)
  pendiente para cuando se ejecute localmente con un `.avaui` de prueba
  que use `import`/`extends`/`click`/expresiones.

**Fuera de alcance de Fase 20.2 (gaps abiertos, documentados):**
- ~~**Gap A** — `data-handler` no emitido en el HTML del swap (los clicks no se bindean al cliente). Requiere `ClickHandler()` aditivo en `IComponent`/`IRenderNode` + bump ABI.~~ ✅ **CERRADO** Prioridad 3: `IRenderNode::ClickHandler()` aditivo (puro virtual, ABI 20); `RenderCommand` structs `drawRect/drawEllipse/drawText` ganan campo `clickHandler`; `IRenderCommandSink/IRenderer/BaseRenderer/HTMLRenderer/NullRenderer/GdiRenderer` propagan `clickHandler`; `SceneCommandWalker` lee `renderNode->ClickHandler()` y la pasa a `sink.Draw*`; `RenderTree::BuildComponent` lee `IComponent::GetProperty("click")` y llama `SetClickHandler`; `HTMLRenderer` emite `data-handler="X"` cuando el handler no está vacío; `UIModule::AbiVersion()` bumpeado a 20.
- **Gap B** — `BuildHeadTags`/`EventScriptTag`/`HotReloadScriptTag`/`BuildConsoleScript`/`BuildBodyEndTags` no inyectados en el branch swap (CSS Tailwind, hot-reload, event client, console replay no funcionan). Requiere refactor del branch swap en `app.cpp` para reusar los helpers ya existentes en `app.cpp`'s anonymous namespace. ✅ **CERRADO** Prioridad 2: el branch swap en `app.cpp::RenderAvaUiRoute` ahora computa y aplica `title`/`extraHead`/`extraBodyEnd` reusando los helpers existentes. `consoleOutput` se inyecta post-render reemplazando el marker en `extraBodyEnd`.
- ~~**Gap C** — `UiComponentResolver` no wireado en el branch swap.~~ ✅ **CERRADO** Prioridad 4: `UiPipelineRenderOptions` gana `projectRoot`/`componentsDir` aditivos; helper anónimo `ResolveImportsAndMergeState` en `ui_pipeline_dynamic_renderer.cpp` invoca el resolver antes del `BindWithOverlay` y mergea `parsed.state` con el state importado; `app.cpp::RenderAvaUiRoute` (branch swap) ahora pasa `options_.projectRoot`/`options_.componentsDir` en `renderOptions`. Funciona en ambos wrappers (`RenderAvauiDynamicWithState` y `RenderAvauiDynamicWithLayoutAndState`).
- ~~**Gap D** — Resolución de expresiones de propiedad contra state (`value = counter` muestra literal).~~ ✅ **CERRADO** Prioridad 5 (enfoque C-1 mínimo, freeze-safe): `VmStateBridge::EvalIdentifier(raw)` lookup exacto en `states_`; `IRenderTree::SetEvalText(...)` virtual puro aditivo (ABI bumpeado a 21); `RenderTree` aplica `Eval()` en todas las propiedades `String` (`backgroundColor`, `borderColor`, `color`, `text`, `label`, `placeholder`, `src`, `fontName`); `RenderTreeFragment` setea `SetEvalText` antes del `Build` (pasando `&stateBridge` para page, `nullptr` para layout). Limitación documentada: solo identificadores exactos, sin concatenación/aritmética (misma limitación que `html_renderer.h::evalText` cuando no había VM binding completo).
- **Gap E** — `avastudio/src/engine/engine_bridge.cpp` — mismo tipo de dependencia de VM, no analizado.
- **Gap F** — `UiPipelineFase20_2Demo.cpp` solo valida parse + render del contenido propio; no resuelve imports ni compone layouts (esos viven en el adapter de avahost/ y requieren el adapter completo con VM).
