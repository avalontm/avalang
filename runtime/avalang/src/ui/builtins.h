#ifndef AVA_UI_BUILTINS_H
#define AVA_UI_BUILTINS_H

#include "avalang.h"

namespace ava {
namespace ui {

// `ui.*` natives callable from AvaLang scripts (08_DESIGNER_VIEW_PLAN.md
// Fase 6: "Preview real ejecutando el arbol" via `ui.*` builtins).
//
// SEGUNDA PASADA (completa la tabla de progreso, fila "RegisterUIBuiltins
// / ui.* builtins reales"). La primera pasada (ver historial en el doc,
// Anexo 9.17) solo registraba `ui.log(...)` porque `ui.alert`/`ui.navigate`
// necesitaban un mecanismo de callback host<->VM que todavía no existía.
// Ese mecanismo ya está (VM::AlertSink/NavigateSink, mismo patrón que
// SetPrintSink -- ver core/src/vm/vm.h/.cpp y
// ava_vm_set_alert_callback/ava_vm_set_navigate_callback en avalang.h),
// así que ahora se registran las tres entradas:
//
//   ui.log(...)      -- igual que print(), con prefijo "[ui] ".
//   ui.alert(msg)     -- dispara VM::Alert -> el alert sink instalado por
//                         el host (si hay uno), o degrada a Print con
//                         prefijo "[ui:alert] " si no hay ninguno.
//   ui.navigate(route) -- dispara VM::Navigate -> el navigate sink
//                         instalado por el host, o degrada a Print con
//                         prefijo "[ui:navigate] " si no hay ninguno. El
//                         CORE no interpreta `route` de ninguna forma
//                         (no valida contra ningún RouteScanner-like) --
//                         es un string opaco que el host decide qué
//                         hacer con él, a propósito, porque el routing
//                         file-based (RouteScanner.cs) vive del lado
//                         .NET/Studio, no acá.
//
// Registrado como una unica variable global `ui` (Dict) con esas tres
// entradas -- mismo mecanismo que resuelve `str.upper()`/`dict.keys()`
// vía GETATTR (ver VM::RegisterNative de un native suelto vs. este
// caso de dict-como-namespace; GETATTR sobre un Dict cae al lookup de
// dict->index cuando no hay un builtin "dict_<attr>" registrado, ver
// core/src/vm/vm.cpp caso OpCode::GETATTR) en vez de nombres globales
// con punto literal (`RegisterNative` no soporta nombres con "." -- el
// compilador nunca genera un GETGLOBAL con punto, siempre GETGLOBAL +
// GETATTR para `a.b`).
//
// Pendiente, deliberadamente NO resuelto en esta pasada:
//   - Ningún host de este repo (Ava Studio) instala todavía un alert
//     sink ni un navigate sink -- ver studio/src/design/state_eval.*/
//     designer_canvas.cpp: el Designer's Preview mode SÍ los instala
//     (para capturarlos en su consola), pero el "Run" genérico de
//     EngineBridge (studio/src/engine/engine_bridge.cpp) todavía no.
//   - Cualquier setter imperativo sobre el arbol en edicion (ej.
//     "ui.setProp(id, key, value)") -- el modelo actual ya cubre
//     mutacion de estado via asignacion directa a globals de `state`
//     (ver studio/src/design/state_eval.h, BuildStateVM), que es el
//     mismo patron que usa AvaLang.UI (`count = count + 1`); un setter
//     imperativo aparte solo hace falta si aparece un caso que ese
//     modelo no cubra, y no hay uno documentado todavia.
void RegisterUIBuiltins(AvaVM* vm);

} // namespace ui
} // namespace ava

#endif // AVA_UI_BUILTINS_H
