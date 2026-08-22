#ifndef AVA_VM_TASK_H
#define AVA_VM_TASK_H

#include "../../platform/barekernel/stdcompat/ava_stdcompat.h"
#include "value.h"

namespace ava {

struct Coroutine;

struct TaskObj {
    Coroutine* co = nullptr;
    bool done = false;
    bool has_error = false;
    Value result;
    Value error;
    avastd::vector<Coroutine*> awaiters;
};

// Sub-fase 5 de Fase 5 (GC), "Crear roots" -- ver
// docs/architecture/GC_FASE5_OWNERSHIP_DESIGN.md §6 y
// CollectFrameRoots/CollectCoroutineRoots en coroutine.h. `result`/`error`
// son raíces porque un TaskObj no participa del refcounting de Object
// (vm.h, comentario en VM::LiveTaskCount): vive mientras vive la VM,
// registrado en created_tasks_, así que cualquier Value que guarde debe
// tratarse como raíz igual que un registro de frame o un global.
void CollectTaskRoots(TaskObj& task, avastd::vector<Value*>& out);

} // namespace ava

#endif // AVA_VM_TASK_H
