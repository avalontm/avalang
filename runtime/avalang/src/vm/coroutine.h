#ifndef AVA_VM_COROUTINE_H
#define AVA_VM_COROUTINE_H

#include "../../platform/barekernel/stdcompat/ava_stdcompat.h"

#include "value.h"
#include "closure.h"

namespace ava {

struct Coroutine;
struct TaskObj;
struct Proto;
struct Closure;

struct ExceptionHandler {
    size_t catch_pc;
    size_t frame_idx;
};

struct CallFrame {
    avastd::shared_ptr<Proto> proto;
    avastd::shared_ptr<Closure> closure;
    avastd::vector<Value> registers;
    avastd::uint32_t pc = 0;
    avastd::string module_dir;
    int ret_slot = -1;  // Register in caller frame to write return value to; -1 = discard
    avastd::uint32_t argc = 0;  // Number of arguments actually supplied by the caller (excludes implicit `this`),
                         // used by the compiler's default-parameter prologue via OpCode::ARGC.
    // Set by VM::ResumeAwaitingCoroutine when a Task an `await` was suspended
    // on settles with an error: instead of writing the error into the await
    // register like a normal resume value, ExecuteFrame raises it as an
    // AvaLang exception the next time this frame runs, so try/catch around
    // the `await` sees it the same way it would see a synchronous raise.
    bool pending_await_error = false;
    Value pending_await_error_value;
    // Upvalues opened against this frame's own registers (i.e. this frame
    // is the parent whose locals got captured by a nested closure), keyed
    // implicitly by Upvalue::reg_index. Interned here so that two sibling
    // closures created in this same frame which capture the same local
    // share one Upvalue -- see VM::FindOrCreateUpvalue -- and so they can
    // all be closed (snapshotted, repointed to their own storage) via
    // VM::CloseUpvalues right before this frame is destroyed.
    avastd::vector<avastd::shared_ptr<Upvalue>> open_upvalues;
    // Bug #14: clase (raw ptr, no ownership -- misma convención que el
    // resto de los ClassObj* que ya circulan sueltos, ej. `cls`/`base_cls`
    // en vm_call_op.cpp; su ciclo de vida lo maneja el refcounting de
    // `Value`, no este frame) dueña del método que este frame está
    // ejecutando. Set por el opcode CALL sobre una Class (constructor,
    // `cls` = clase instanciada) y por OpBaseCall (`base_cls` = clase
    // donde se encontró el método de `base.xxx()`). Deja que OpBaseCall
    // resuelva `base.xxx()` respecto de la clase que está corriendo
    // AHORA MISMO en vez de siempre respecto de `__base__` cacheado en la
    // instancia -- eso es lo que permite que una cadena de 3+ niveles
    // (`base.__init__()` dentro de `base.__init__()`) avance un nivel por
    // llamada en vez de resolver siempre al mismo nivel. nullptr = frame
    // no rastreado (ver fallback en OpBaseCall, vm_call_op.cpp).
    ClassObj* base_lookup_class = nullptr;
};

enum class CoStatus { Suspended, Running, Dead };

struct Coroutine {
    Value entry;
    CoStatus status = CoStatus::Suspended;
    avastd::vector<CallFrame> frames;
    avastd::vector<Value> yielded_values;
    // Set only for coroutines created internally by VM::StartAsyncCall to
    // drive an `async func` call. nullptr for coroutines created by the
    // user-facing coroutine()/resume() builtins.
    TaskObj* owner_task = nullptr;
    // Exception handlers (try/catch scopes) active inside this coroutine's
    // own call stack at the moment it suspended on `await`. Restored by
    // VM::ResumeAwaitingCoroutine so a `try ... await ... catch` whose
    // await actually suspends still catches errors after resuming --
    // exception_handlers_ at VM level is per-run (cleared on every
    // StartAsyncCall/ResumeAwaitingCoroutine), so without this the
    // coroutine's own handlers would be lost on suspend.
    avastd::vector<ExceptionHandler> exception_handlers;
    Coroutine() : status(CoStatus::Suspended) {}
};

// Sub-fase 5 de Fase 5 (GC), "Crear roots" -- ver
// docs/architecture/GC_FASE5_OWNERSHIP_DESIGN.md §6. Junta un puntero a
// cada Value que vive directamente en `frames` (cada CallFrame::registers
// + pending_await_error_value): son raíces porque su vida no depende del
// refcount de ningún Object -- dependen de que el propio `frames` siga
// vivo (stack de la VM o de una Coroutine). Usado tanto por
// VM::CollectGcRoots (para frames_/saved_frames_) como para las frames de
// cada Coroutine viva (created_coroutines_). Los punteros quedan válidos
// mientras `frames` no se reasigne/destruya -- pensado para usarse y
// descartarse dentro de un mismo ciclo de GC, no para guardarse.
void CollectFrameRoots(avastd::vector<CallFrame>& frames, avastd::vector<Value*>& out);

// Igual que arriba pero para una Coroutine completa: sus frames +
// yielded_values (lo que un `yield` dejó pendiente de leer por quien la
// resuma). No incluye owner_task ni exception_handlers -- el primero se
// visita aparte via created_tasks_ (ver CollectTaskRoots en task.h), y el
// segundo no guarda Value (solo catch_pc/frame_idx).
void CollectCoroutineRoots(Coroutine& co, avastd::vector<Value*>& out);

// Sub-fase 8 de Fase 5 (GC), "Integrar coroutines" -- ver
// docs/architecture/GC_FASE5_OWNERSHIP_DESIGN.md §10. `entry`/`frames`/
// `yielded_values` de una `Coroutine` ya `CoStatus::Dead` no se leen
// nunca mas (ver los guards de "attempt to resume a dead coroutine" y
// ResumeAwaitingCoroutine que retorna si `status != Suspended`) -- pero
// nadie los liberaba, asi que quedaban reteniendo objetos para siempre
// via created_coroutines_ (raiz permanente en VM::CollectGcRoots,
// nunca se saca nada de ahi salvo en ~VM()). Llamar esto justo despues
// de que `co.status` pase a Dead suelta esas referencias con el RAII
// normal de Value (Retain/Release, sub-fase 2), no con el mark-sweep de
// ciclos -- son objetos con dueño claro (la propia Coroutine), no basura
// de ciclo.
void ReleaseDeadCoroutineState(Coroutine& co);

} // namespace ava

#endif // AVA_VM_COROUTINE_H
