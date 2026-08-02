#pragma once

#include <string>

#include "avalang.h"
#include "design/design_document.h"

namespace studio::design {

// Fase 6 (08_DESIGNER_VIEW_PLAN.md sección 2 punto 3 / secciones 9.2,
// 9.5, 9.13: "`state` sin evaluar/bindear contra la VM" -- el
// pendiente más grande que quedaba). Primera pasada, acotada a lo que
// hace falta para que el canvas muestre contenido real en vez de nada
// (ver designer_canvas.cpp: hasta esta pasada no dibujaba ningún valor
// de prop, solo el rectángulo + type/id) -- NO es la Fase 6 completa
// del plan ("Preview real ejecutando el árbol" via `ui.*` builtins,
// RegisterUIBuiltins sigue siendo un no-op, ver core/src/ui/builtins.cpp);
// esto solo evalúa expresiones sueltas de `state`/props contra una VM,
// que es exactamente lo que la sección 2 punto 3 ya señalaba como
// alcanzable SIN esperar a `ui.*`.
//
// Deliberadamente separado de DesignNode/PropertyRow: lo que se
// evalúa acá es solo para DISPLAY (canvas), nunca se escribe de vuelta
// en `doc` -- Properties sigue mostrando/editando el texto fuente
// (la expresión tal cual), y SaveAvauiFile sigue persistiendo eso, no
// el valor evaluado.
//
// Limitación heredada de avaui_text.h (documentada ahí, no resuelta
// acá): un literal de string simple ("Guardar") y un identificador
// desnudo (message) llegan a PropertyRow::value indistinguibles --
// ambos ya vienen sin comillas del parser. EvalPropertyExpr de abajo
// lo resuelve con un truco barato en vez de tocar el parser: evalúa el
// texto tal cual como expresión; si resuelve a un global real (nombre
// de una var de `state`) usa ese valor, si no (VM::GetGlobal devuelve
// Nil para cualquier nombre no definido, ver core/src/vm/vm.cpp) cae
// de vuelta al texto crudo -- que es exactamente lo que ya se mostraba
// antes de esta pasada, así que nunca es peor que el comportamiento
// actual, solo mejor cuando sí hay algo que evaluar.

// Crea una AvaVM nueva y bindea doc.initial_state como globals, en
// orden de archivo. Cada valor de state se infiere como número/bool/
// string según su texto -- mismo criterio de LooksNumeric/
// WritePropertyValue en core/src/ui/avaui_text.cpp para el camino
// inverso (escribir), espejado acá a mano porque esta parte vive del
// lado de Studio y solo tiene la C API (ava_value_t), no el Value
// interno del core.
//
// Devuelve nullptr solo si ava_vm_create() en sí falla (no debería
// pasar en la práctica; todo caller de acá debe chequear antes de
// usar). El caller es dueño de la VM devuelta y debe liberarla con
// ava_vm_destroy cuando termine (ver designer_canvas.cpp: una por
// llamada a DrawDesignerCanvas, destruida al final del mismo frame --
// no cacheada todavía, ver nota de performance en el .cpp).
AvaVM* BuildStateVM(const DesignDocument& doc);

// Evalúa `raw_value` (el texto de PropertyRow::value tal cual quedó
// después del parser -- ver la nota de arriba sobre la ambigüedad
// literal/identificador) como una expresión AvaLang contra `vm` (se
// asume que ya tiene los globals de `state` bindeados vía
// BuildStateVM, aunque no es un requisito estricto -- una expresión
// que no referencia ningún global de state evalúa igual, p.ej. un
// número o `true`/`false` sueltos).
//
// Best-effort: compila y corre `raw_value` como expresión; si falla
// compilar/correr, o si el resultado es Nil (el caso común: un
// literal de string plano tipo "Guardar" evaluado como identificador
// no resuelve a nada), devuelve `raw_value` sin tocar. `vm == nullptr`
// también devuelve `raw_value` tal cual (mismo fallback, para que un
// caller sin VM disponible no tenga que chequear dos veces).
std::string EvalPropertyExpr(AvaVM* vm, const std::string& raw_value);

// Prop key cuyo valor evaluado es "lo que se ve" en el control --
// "value" para text/textbox, "text" para button/link, "label" para
// checkbox/radiobutton, "" (sin display prop) para el resto
// (containers, image -- "src" no es texto para evaluar/mostrar así --,
// spacer, divider). Ver component_catalog.cpp para la lista completa
// de tipos y sus default_properties. Vive acá en vez de en
// component_catalog.h porque es específico de "qué evaluar y mostrar
// en el canvas" (Fase 6), no parte del catálogo estático de tipos.
std::string GetDisplayPropertyKey(const std::string& node_type);

// Fase 6, segunda pasada (08_DESIGNER_VIEW_PLAN.md Anexo 9.17/9.18):
// compila `doc.code_behind` (el bloque `methods` completo, texto
// verbatim -- ver DesignDocument::code_behind) contra `vm` UNA VEZ,
// para que cada `func nombre(...) ... end` de nivel superior quede
// registrado como global (una Closure), exactamente como
// EnsureClickHandler ya generaba el stub esperando que algo, algún
// día, lo pudiera invocar. Sin esto, `state_vm` (BuildStateVM) solo
// tenía las variables de `state` como globals -- ninguna función.
//
// Best-effort, mismo espíritu que EvalPropertyExpr: si `code_behind`
// no compila (por ejemplo, a medio escribir en Code view mientras el
// usuario tipea), esto es un no-op silencioso -- no hay nada razonable
// que mostrar en el canvas por un error de compilación de un handler,
// a diferencia de una prop de display individual. `vm == nullptr` o
// `doc.code_behind` vacío también son no-ops.
//
// Llamar UNA VEZ por (re)construcción de `vm` -- ver
// designer_canvas.cpp: se llama junto a BuildStateVM, bajo la misma
// condición de invalidación del cache (DesignerVmCacheEntry), no en
// cada click. Volver a llamarla sobre la MISMA vm re-compila y
// re-registra las funciones (SETGLOBAL simplemente pisa el global
// anterior), así que es seguro pero redundante si ya se llamó para
// esta vm -- el caller no necesita trackear si ya se llamó por su
// cuenta más que la condición de invalidación que ya usa para el
// rebuild del propio `vm`.
void BindCodeBehind(AvaVM* vm, const DesignDocument& doc);

// Invoca `handler_name()` -- una función de cero argumentos ya
// registrada como global en `vm` (típicamente por BindCodeBehind, o
// por cualquier otro código que haya corrido contra esta vm antes) --
// y descarta su valor de retorno. Pensado para "click en un botón real
// del canvas ejecuta su handler contra el estado real" (Anexo 9.17,
// pendiente 1: "wiring click -> ejecutar método"), NO para leer un
// resultado (a diferencia de EvalPropertyExpr, que sí lo necesita).
//
// Best-effort: devuelve false sin tocar nada más si `vm` es nullptr,
// `handler_name` está vacío, no compila (nombre inválido, o el global
// no es invocable -- p.ej. code_behind nunca se bindeó), o si correrlo
// lanza un error de runtime (excepción sin capturar en el handler).
// Un handler exitoso puede mutar los globals de `state` (ej.
// `count = count + 1`, igual que el modelo de AvaLang.UI) -- el
// caller es responsable de invalidar cualquier cache de evaluación
// de display-props que dependa de esos globals (ver
// DesignerVmCacheEntry::eval_cache en designer_canvas.cpp: no se
// invalida sola, `InvokeHandler` no sabe nada de ese cache).
//
// `out_error`, agregado para el modo Preview (08_DESIGNER_VIEW_PLAN.md
// Fase 6, fila "click ejecuta el methods handler real" -- limitacion 2
// de Anexo 9.18: "un error de runtime en el handler no tiene donde
// mostrarse"). Antes esta funcion descartaba el mensaje de
// compile_error/run_error; ahora, si `out_error` no es null, lo llena
// con ese mensaje en cualquier caso de `return false` que SI tenga un
// mensaje disponible (no compila, o corre y tira). Nulo por default
// para no tocar al caller existente (Ctrl+Click en modo edicion, que
// nunca miro el error) -- ver designer_canvas.cpp para el caller nuevo
// que si lo pasa (Preview mode).
bool InvokeHandler(AvaVM* vm, const std::string& handler_name, std::string* out_error = nullptr);

} // namespace studio::design
