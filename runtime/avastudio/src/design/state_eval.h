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
// Deliberadamente separado de avalang::ui::IComponent/PropertyRow: lo que se
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

std::string EvalPropertyExpr(AvaVM* vm, const std::string& raw_value);

std::string GetDisplayPropertyKey(const std::string& node_type);

void BindCodeBehind(AvaVM* vm, const DesignDocument& doc);

bool InvokeHandler(AvaVM* vm, const std::string& handler_name, std::string* out_error = nullptr);

} // namespace studio::design
