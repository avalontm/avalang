// Fase 5 (Async Runtime). set_timeout(callback, delayMs): agenda
// `callback` (funcion AvaLang, sin args) para correr tras delayMs usando
// el PAL (VmPlatformAccessor::Get().Timer()), no Sleep() bloqueante.
// Ver core/src/vm/vm_async.cpp para el porque del PostAsyncTask/pump en
// vez de llamar la VM directo desde el hilo del timer.
#include "builtin.h"
#include "../vm/vm.h"
#include "../vm/vm_platform_accessor.h"

ava_value_t builtin_set_timeout(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data) {
    (void)user_data;
    ava_value_t result;
    result.type = AVA_NIL;

    if (count < 2) return result;

    auto* raw_vm = reinterpret_cast<ava::VM*>(vm);
    auto callback = ava::FromC(args[0]);
    auto delay_val = ava::FromC(args[1]);

    if (callback.type != ava::ValueType::Function) {
        return result;
    }

    uint32_t delay_ms = 0;
    if (delay_val.type == ava::ValueType::Number) {
        delay_ms = static_cast<uint32_t>(delay_val.n < 0 ? 0 : delay_val.n);
    }

    raw_vm->OnAsyncTimerScheduled();

    uint64_t handle = ava::VmPlatformAccessor::Get().Timer().ScheduleOnce(
        delay_ms, [raw_vm, callback]() {
            raw_vm->PostAsyncTask([raw_vm, callback]() {
                raw_vm->Call(callback, {});
            });
            raw_vm->OnAsyncTimerConsumed();
        });

    result.type = AVA_NUMBER;
    result.as.n = static_cast<double>(handle);
    return result;
}

// Fase 5 (Async Runtime) - sub-fase 5.3. sleep_async(co, delayMs): agenda
// el resume() REAL de una coroutine tras delayMs, usando el mismo camino
// PostAsyncTask/PumpAsyncEvents que set_timeout. A diferencia de
// set_timeout (que llama una funcion nueva cada vez), esto reanuda una
// coroutine ya suspendida en un `yield` -- mismo mecanismo que
// builtin_resume (vm->Call sobre un Value tipo Coroutine, ver
// vm_call.cpp), solo que el Call ocurre mas tarde en vez de inmediato.
// Uso tipico en AvaLang:
//
//   func worker()
//       print("antes")
//       yield
//       print("despues del delay")
//   end
//
//   co = coroutine(worker)
//   resume(co)              # corre hasta el primer yield
//   sleep_async(co, 500)    # reanuda automaticamente 500ms despues
//
// No requiere tocar coroutine.cpp/vm_call.cpp: reusa la suspension
// cooperativa que ya existe (Fase 1 del plan de async/await), el PAL
// solo decide CUANDO se dispara el resume.
ava_value_t builtin_sleep_async(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data) {
    (void)user_data;
    ava_value_t result;
    result.type = AVA_NIL;

    if (count < 2 || args[0].type != AVA_COROUTINE) return result;

    auto* raw_vm = reinterpret_cast<ava::VM*>(vm);
    auto co_val = ava::FromC(args[0]);
    auto delay_val = ava::FromC(args[1]);

    uint32_t delay_ms = 0;
    if (delay_val.type == ava::ValueType::Number) {
        delay_ms = static_cast<uint32_t>(delay_val.n < 0 ? 0 : delay_val.n);
    }

    raw_vm->OnAsyncTimerScheduled();

    uint64_t handle = ava::VmPlatformAccessor::Get().Timer().ScheduleOnce(
        delay_ms, [raw_vm, co_val]() {
            raw_vm->PostAsyncTask([raw_vm, co_val]() {
                raw_vm->Call(co_val, {});
            });
            raw_vm->OnAsyncTimerConsumed();
        });

    result.type = AVA_NUMBER;
    result.as.n = static_cast<double>(handle);
    return result;
}

ava_value_t builtin_delay(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data) {
    (void)user_data;
    ava_value_t result;
    result.type = AVA_NIL;

    uint32_t delay_ms = 0;
    if (count >= 1) {
        auto delay_val = ava::FromC(args[0]);
        if (delay_val.type == ava::ValueType::Number) {
            delay_ms = static_cast<uint32_t>(delay_val.n < 0 ? 0 : delay_val.n);
        }
    }

    auto* raw_vm = reinterpret_cast<ava::VM*>(vm);
    return ava::ToC(raw_vm->CreateTimerTask(delay_ms));
}

// Fase 5 (Async Runtime) - sub-fase 5.4. clear_timeout(handle): cancela
// un timer agendado por set_timeout()/sleep_async() antes de que
// dispare. Si ya disparo (o el handle es invalido/0), es un no-op --
// mismo comportamiento que clearTimeout en JS. Nota: no decrementa
// async_pending_timers_ aca; si el timer no habia disparado todavia el
// contador va a quedar "de mas" hasta que el proceso salga -- no hay
// forma barata de saber desde este lado si WinTimer::Cancel llego a
// tiempo o el callback ya estaba en vuelo (carrera inherente a cancelar
// un timer). No afecta correctitud del event loop (PumpAsyncEvents sigue
// drenando lo que SI quedo encolado), solo hace que HasPendingAsyncWork()
// pueda reportar "trabajo pendiente" un poco de mas en el peor caso.
ava_value_t builtin_clear_timeout(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data) {
    (void)vm;
    (void)user_data;
    ava_value_t result;
    result.type = AVA_NIL;

    if (count < 1 || args[0].type != AVA_NUMBER) return result;

    uint64_t handle = static_cast<uint64_t>(args[0].as.n);
    if (handle != 0) {
        ava::VmPlatformAccessor::Get().Timer().Cancel(handle);
    }
    return result;
}
