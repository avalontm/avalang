#ifndef AVA_VM_GC_SWEEP_H
#define AVA_VM_GC_SWEEP_H

#include "../../platform/barekernel/stdcompat/ava_stdcompat.h"

namespace ava {

class VM;

// Sub-fase 7 de Fase 5 (GC), "Manejar ciclos" -- ver
// docs/architecture/GC_FASE5_OWNERSHIP_DESIGN.md. Resultado de un ciclo
// de GcCollectCycles(): cuantos Object habia vivos antes de correr,
// cuantos quedaron marcados (alcanzables) y cuantos se liberaron.
struct GcSweepStats {
    avastd::int64_t objects_before = 0;
    avastd::int64_t marked = 0;
    avastd::int64_t collected = 0;
};

// Corre un mark-sweep completo sobre `vm`: junta roots (VM::CollectGcRoots),
// marca lo alcanzable (GcTraceMark) y libera todo Object vivo que no haya
// quedado marcado, salvo Upvalue (vive por shared_ptr, no por este
// mecanismo -- ver closure.h). No es seguro llamarlo a mitad de la
// ejecucion de un opcode (ver comentario en VM::CollectGarbage).
GcSweepStats GcCollectCycles(VM& vm);

} // namespace ava

#endif // AVA_VM_GC_SWEEP_H
