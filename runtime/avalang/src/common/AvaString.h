#ifndef AVA_COMMON_AVA_STRING_H
#define AVA_COMMON_AVA_STRING_H

#include "../../platform/barekernel/stdcompat/ava_stdcompat.h"

// Fase 2 del plan (avalang_runtime_stl_barekernel_plan.md, seccion 4.1):
// "AvaString debe sustituir progresivamente a std::string, administrando
// buffer, longitud, capacidad, comparacion, concatenacion, acceso y
// conversion."
//
// Decision (cierra el punto abierto en RUNTIME_CORE_AUDIT.md Sec.5/6):
// esa sustitucion ya existe -- es avastd::string
// (platform/barekernel/stdcompat/ava_string.h). En hosted
// (AVA_HAVE_STD_LIBRARY=1) es std::string real; en BareKernel
// (AVA_HAVE_STD_LIBRARY=0) es una reimplementacion freestanding propia
// que cubre exactamente esa lista (buffer sin SSO, len_/cap_, operator==,
// operator+=, operator[], c_str()/to_string()) y aloca via
// ava_alloc/ava_free (AvaMemory, Fase 1). No se construye una segunda
// clase de string paralela: hacerlo duplicaria almacenamiento y logica de
// ownership sin ganar nada, justo el riesgo que RUNTIME_CORE_AUDIT.md
// Sec.5 advierte evitar ("sin duplicar una segunda capa de contenedores").
//
// AvaString es entonces un alias nombrado -- fija el nombre que pide la
// seccion 4.1 del plan sobre el tipo que ya cumple su contrato, en vez de
// reimplementarlo. `StringObj::data` (src/vm/value.h) usa exactamente
// este tipo por debajo.
namespace ava {

using AvaString = avastd::string;

} // namespace ava

#endif // AVA_COMMON_AVA_STRING_H
