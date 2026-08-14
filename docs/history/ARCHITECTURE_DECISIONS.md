# Architecture Decisions

## Fase 0 — Limpieza avaui_json (PLAN_CENTRALIZACION_AVAUI.md)

Eliminados `runtime/avastudio/src/design/avaui_json.h` y `.cpp`.
Formato JSON paralelo sin caller real, no referenciado en CMakeLists.
El formato de archivo real es `.avaui` (texto), manejado por
`AvauiParser` (lectura) y `avaui_text.cpp` (escritura, Studio-only por ahora).

> **Nota de ejecución (Fase 0, pasada real):** la entrada anterior
> describía la eliminación como un hecho, pero los archivos seguían
> presentes en el repo (sin compilar, sin callers externos, solo se
> autoreferenciaban). La eliminación física se ejecutó ahora con
> `git rm`. Verificado: `grep -rn "avaui_json\|ComponentTreeToJson\|
> ComponentTreeFromJson"` sobre `runtime/` y `CMakeLists.txt` devuelve
> cero resultados. La entrada se mantiene como registro de la decisión
> original; esta nota solo corrige la desincronización doc/repo.

## Fase 1 — Writer `.avaui` promovido a `avaui/src/parser/AvauiWriter`

Creado `runtime/avaui/src/parser/AvauiWriter.{h,cpp}` (namespace
`avalang::ui::parser`, exportado con `AVA_UI_API`): serializa un
`avalang::ui::IComponent*` a texto `.avaui` vía `WriteAvaui(root,
AvauiWriteOptions)`, donde `AvauiWriteOptions` es un struct extensible
(`code_behind`, `initial_state` como `vector<AvauiStateEntry>`,
`imports`, `extends`) — sin depender de `PropertyRow` (tipo de
`avastudio`).

Es el inverso canónico de `AvauiParser::Parse` para
`avalang::ui::IComponent`. NO usa la C API `ava_ui_write_avaui_text`
ni toca el pipeline VM (`runtime/avalang/src/ui/avaui_text.{h,cpp}`,
namespace `ava::ui::Component`, deliberadamente separado — ver
advertencia 0.4 del plan y `docs/AVAUI_CONVERGENCE_DECISION.md`).

Migrados los 5 callers de `studio::design::WriteAvauiText`:
- `design_document.cpp` (`SaveAvauiFile`)
- `main.cpp` (`get_active_avaui_document`)
- `editor_panel.cpp` (`ToggleTabViewMode` Design→Code)
- `plugin_host.cpp` (`DesignAddComponentTrampoline`)
- `plugin_host.cpp` (`DesignEditComponentTrampoline`)

Eliminados `runtime/avastudio/src/design/avaui_text.{h,cpp}` y sus
entradas en `runtime/avastudio/CMakeLists.txt` (dos listas:
`STUDIO_SOURCES` y `ava_studio_grid_flex_regression_test`). También se
eliminó `PropValueToString` de `design_document.cpp` (helper sin uso
tras la migración). El test de regresión ya linkea `avalang_ui`, así
que no necesita reemplazo de fuente — el writer viene de la librería.

Pendiente: test de round-trip `Parse(Write(Parse(s))) == Parse(s)`
sobre samples reales (fase de tests, no bloqueante para el build).

## Fase 2 — Layout engine paralelo (código muerto eliminado)

`runtime/avastudio/src/design/layout_engine.{h,cpp}` era código muerto:
sin callers (`grep -rn "design/layout_engine.h\|ComputeLayout("` sobre
`runtime/avastudio/src` devuelve cero), sin entrada en ningún
`CMakeLists.txt`. `designer_canvas.cpp` ya usa `avalang::ui::LayoutEngine`
real vía `BuildLiveRender` desde antes de este plan. Se eliminaron los
archivos muertos con `git rm`.

**Subpunto verificado (limitación conocida, no resuelta en esta fase):**
`width`/`height`/`padding`/`margin`/`spacing` no pasan por
`EvalBool`/`EvalNumber` en `LayoutEngineImpl.cpp`. Verificado:
`grep -n "evalText_\|stateBridge\|EvalBool\|EvalNumber\|EvalProperty"`
sobre `runtime/avaui/src/layout/` devuelve cero resultados. El motor de
layout lee `PropertyValue` directamente vía `component->GetProperty(name)`
y lo interpreta con helpers propios (`ReadNumber`/`TryReadNumber`).

Esto es un límite conocido documentado en
`docs/architecture/property-state-binding.md` (sección "Límite conocido:
propiedades de layout"): resolverlo requiere threadear un evaluador de
estado hasta `LayoutEngine::Compute` — un cambio de interfaz mayor que
NO corresponde a la Fase 2. Queda como trabajo futuro ("Fase 2b"
potencial) si se necesita `width = miVariable` o similar.

## Fase 3 — Coerción de propiedades unificada vía `AvauiPropertyCoercion`

`runtime/avastudio/src/design/state_eval.cpp` tenía un `LooksNumeric`
manual que espejaba el criterio de `core/src/ui/avaui_text.cpp`
(`LooksNumeric` del namespace anónimo del parser VM) para inferir si
un valor de `state` era número/bool/string al construir la VM. Era una
copia del criterio, justificada por el comentario "solo tiene la C
API (`ava_value_t`), no el `Value` interno del core" — un tercer
criterio paralelo que podía desincronizarse.

Reemplazado por `avalang::ui::parser::InferValue` (de
`runtime/avaui/src/parser/AvauiPropertyCoercion`, el criterio
centralizado que ya usa el parser `.avaui`, el writer y
`live_render_bridge`). `state_eval.cpp` ahora llama `InferValue` y
mapea el `PropertyValue` resultante a `ava_value_t` según el
`PropertyType` (`Bool`→`AVA_BOOL`, `Number`→`AVA_NUMBER`,
`String`/`Nil`→`AVA_STRING`). Eliminado `LooksNumeric` del archivo.

Actualizado el comentario de cabecera de `state_eval.h`: ya no aplica
la justificación "espejado a mano porque solo tiene la C API" — ahora
delega al criterio centralizado en `avalang_ui`. La separación
conceptual "lo evaluado es solo para display, nunca se escribe de
vuelta al `PropertyRow`" se mantiene intacta (no se tocó ese
comportamiento).

**Nota arquitectónica:** el plan pedía exponer
`ava_ui_eval_property_bool`/`_number` en la C API de `avalang`
(`runtime/avalang/api/`). No se hizo así porque `InferValue` vive en
`avalang_ui` (que NO depende de `avalang` — la dependencia es al
revés), y `EvalBool`/`EvalNumber` de `RenderTree` operan sobre
`IComponent*` con callback `evalText_` (runtime eval, no coerción
estática). `avastudio` ya linkea `avalang_ui` y tiene sus include
paths, así que `state_eval.cpp` puede usar `InferValue` directamente
sin necesidad de exponer nada en la C API de `avalang`. El
"Definition of Done" del plan ("un solo lugar decide qué es
bool/número/texto/identificador de estado") se cumple: ese lugar es
`AvauiPropertyCoercion::InferValue`.

## Fase 4 — Convergencia del backend de la C API a `avalang_ui`

### 4.1 — `avalang` ahora depende de `avalang_ui`

`runtime/avalang/CMakeLists.txt`: añadido
`target_link_libraries(avalang PRIVATE avalang_ui)` y
`target_include_directories(avalang PRIVATE ${CMAKE_SOURCE_DIR}/runtime/avaui/src)`.
La dependencia es unidireccional: `avalang` → `avalang_ui` (no al revés).
`avalang_ui` sigue sin depender de `avalang`.

### 4.2 — `c_api.cpp` reescrito

`runtime/avalang/api/src/c_api.cpp` era el único punto donde los
tipos `ava::ui::Component`/`ava::ui::ComponentTree` (el pipeline VM,
ahora eliminados) salían al exterior vía la C API como `AvaComponent`/
`AvaComponentTree` opacos. Reescrito para que `AvaComponent`/
`AvaComponentTree` ahora envuelvan `avalang::ui::IComponent`/
`avalang::ui::ComponentTree` (de `avalang_ui`).

Cambios clave en `c_api.cpp`:

- `struct AvaComponent` ahora tiene `avalang::ui::IComponent* comp` y
  `std::unique_ptr<avalang::ui::ComponentTree> owned_tree` (era
  `std::shared_ptr<ava::ui::Component>` + `shared_ptr<ComponentTree>`).
- `struct AvaComponentTree` ahora tiene
  `std::unique_ptr<avalang::ui::ComponentTree> tree`.
- `ToPropertyValue`/`ToAvaValue`: convierten entre `ava_value_t` (C
  ABI) y `avalang::ui::PropertyValue` (era `ava::ui::PropertyValue`).
- Eventos: `IComponent` almacena eventos como propiedades regulares
  cuyo key está en `IsEventPropertyName()` (de
  `avalang::ui::events/AutoBind.h`). No hay bolsa de eventos separada
  — a diferencia del eliminado `ava::ui::Component` que tenía
  `GetAllEvents()`.
- Layout: como `IComponent` no tiene un campo `int layout_` (lo tenía
  `ava::ui::Component`), `ava_ui_set_layout`/`ava_ui_get_layout`
  almacenan layout como una propiedad interna `__layout` de tipo
  Number. `ComponentToJson` lee `__layout` y hace fallback a
  `LayoutNameToId(TypeName())`.
- `ComponentToJson` (serialización a JSON del árbol) reescrito sobre
  `IComponent*`, excluyendo `id` y `__layout` del bloque
  `properties`.

**Problema de compilación resuelto — `extern "C"` + namespace alias:**

`avalang.h` abre `#ifdef __cplusplus extern "C" { #endif` en su
línea 23. Originalmente `c_api.cpp` incluía `avalang.h` primero, lo
que envolvía todos los includes subsiguientes en `extern "C"`. Una
sentencia `using avalang_ui = avalang::ui;` dentro de `extern "C"`
es ilegal en MSVC (C2187). Solución: mover `#include "avalang.h"`
al final (después de los includes C++), y eliminar el alias
`avalang_ui` — usar `avalang::ui::` fully-qualified en todo el
archivo. El `using namespace ava;` sí funciona (los headers de VM
cierran sus propios `namespace ava { }` y los `extern "C"` de
`avalang.h` y `builtin.h` están balanceados).

**Problema de compilación resuelto — `static` + `extern "C"`:**

`ComponentToJson` es `static` (internal linkage) pero estaba
definida dentro del bloque `extern "C"` (external linkage). MSVC
reporta C2732 ("linkage specification contradicts earlier
specification"). Solución: mover la definición de `ComponentToJson`
fuera de `extern "C"`, antes del bloque, junto a los demás helpers
`static`.

### 4.3 — Routes en `AvauiParser` y `AvauiWriter`

`runtime/avaui/src/parser/AvauiParser.{h,cpp}`: añadidos
`RouteParameter` (name, kind `Required`/`Optional`, constraint),
`RouteDeclaration` (route_template, parameters), y `ParseRoute()`
con regex `\{(\w+)(\?)?(?::(\w+))?\}`. `ParsedAvaui::routes` cambió
de `vector<string>` a `vector<RouteDeclaration>`.

`runtime/avaui/src/parser/AvauiWriter.{h,cpp}`: añadido
`AvauiRouteEntry` y `routes` en `AvauiWriteOptions`. El writer
excluye `__layout` de las propiedades escritas.

`c_api.cpp`: `RoutesToJson` serializa `RouteDeclaration` al formato
JSON de la C API
(`[{"template": "/products/{id}", "parameters": [...]}]`).
`ava_ui_write_avaui_text` parsea el JSON de rutas de vuelta (parser
simple línea-por-línea — funcional para el caso común).

### 4.4 — Eliminación de 12 archivos en `runtime/avalang/src/ui/`

Eliminados con `git rm`:
- `component.{h,cpp}` — `ava::ui::Component` (reemplazado por
  `avalang::ui::IComponent`)
- `tree.{h,cpp}` — `ava::ui::ComponentTree` (reemplazado por
  `avalang::ui::ComponentTree`)
- `property.cpp` — lógica de propiedades del Component VM
- `event.cpp` — lógica de eventos del Component VM
- `layout.{h,cpp}` — layout IDs del Component VM
- `registry.{h,cpp}` — registry de tipos de Component VM
- `avaui_text.{h,cpp}` — parser/writer del pipeline VM (reemplazado
  por `AvauiParser`/`AvauiWriter` en `avalang_ui`)

### 4.5 — `CMakeLists.txt` de `avalang` actualizado

Removidos los 12 archivos eliminados de `CORE_SOURCES` en
`runtime/avalang/CMakeLists.txt`. Solo permanece
`src/ui/builtins.cpp` (registra `ui.log`/`ui.alert`/`ui.navigate`
en la VM — no depende de `Component` ni ningún archivo eliminado).

### Estado final

- `avalang_ui` (`avalang_ui.dll`): biblioteca compartida para todo
  lo UI — `IComponent`, `ComponentTree`, `AvauiParser`,
  `AvauiWriter`, `LayoutEngine`, `RenderTree`, eventos, coerción.
  No depende de `avalang`.
- `avalang` (`avalang.dll`): VM/core. Ahora depende de
  `avalang_ui` (para la C API `ava_ui_*` que envuelve
  `IComponent`). Mantiene `builtins.cpp` (registra `ui.*` en la VM).
- `avastudio` (`ava_studio.exe`): IDE. Depende de `avalang` y
  `avalang_ui`. Usa `AvauiWriter` directamente (Fase 1) y
  `AvauiPropertyCoercion::InferValue` (Fase 3).
- Cero duplicación de lógica UI entre `avalang` y `avastudio`.
- Build green: `avalang_ui.dll` + `avalang.dll` + `ava_studio.exe`.

## Fase 5 — Centralización de helpers UI duplicados en `avahost` y `avastudio`

### 5.1 — `NumberToDisplayString` y `LooksLikeCall` promovidos a `AvauiPropertyCoercion`

`runtime/avaui/src/parser/AvauiPropertyCoercion.{h,cpp}`: añadidos
`NumberToDisplayString(double)` (formatea un double como string sin
trailing `.000000`, via `ostringstream`) y `LooksLikeCall(string)`
(true si el string termina en `)` y contiene `(`). Ambos exportados
con `AVA_UI_API`.

Eliminadas 3 copias de `NumberToDisplayString`/`FormatAvaNumber`:
- `avahost/src/runtime/runtime_host.cpp` — `NumberToDisplayString`
- `avastudio/src/design/state_eval.cpp` — `NumberToDisplayString`
- `avastudio/src/engine/engine_bridge.cpp` — `FormatAvaNumber`

Eliminadas 2 copias de `LooksLikeCall`:
- `avahost/src/runtime/runtime_host.cpp`
- `avastudio/src/design/state_eval.cpp`

Todas reemplazadas por `using` declarations del helper centralizado en
`avalang::ui::parser::`.

### 5.2 — `LooksNumeric` eliminado de `avahost`

`avahost/src/runtime/runtime_host.cpp` y
`avahost/src/rendering/ui_vm_state_bridge.cpp` tenían cada uno su
propia copia de `LooksNumeric` (la misma gramática
dígitos/un-punto/signo-opcional que Fase 3 ya había eliminado de
`avastudio/state_eval.cpp`).

Reemplazado por `avalang::ui::parser::InferValue` de `avaui`:
- `runtime_host.cpp` `BindState()`: usa `InferValue(raw)` para
  decidir Bool/Number/String al pushear VM globals.
- `ui_vm_state_bridge.cpp` `RawTextToPropertyValue()`: ahora delega
  directamente a `InferValue(raw)`.
- `PropertyValueToRawText()` en `ui_vm_state_bridge.cpp`: usa
  `NumberToDisplayString` para el caso Number.

`avahost` ya linkea `avalang_ui` (`target_link_libraries` en su
CMakeLists, línea 112), pero `runtime_host.cpp` se compila siempre
(incluso si `AVA_BUILD_UI=OFF`). Los includes/usos de
`AvauiPropertyCoercion` están protegidos con
`#ifdef AVAHOST_HAS_UI_PIPELINE` para no romper builds sin UI.

### Estado final tras Fase 5

- `avalang::ui::parser::InferValue` es el único criterio de coerción
  texto→valor en todo el ecosistema (parser, writer, avastudio,
  avahost).
- `avalang::ui::parser::NumberToDisplayString` es el único
  formateador double→string para display.
- `avalang::ui::parser::LooksLikeCall` es el único detector de
  expresiones de llamada.
- Cero copias de `LooksNumeric` en `runtime/`.
- Build green: `avalang_ui.dll` + `avalang.dll` + `ava_studio.exe`.
