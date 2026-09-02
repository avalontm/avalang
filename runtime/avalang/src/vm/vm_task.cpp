#include "vm.h"
#include "vm_internal.h"
#include "task.h"
#include "coroutine.h"
#include "vm_platform_accessor.h"

namespace ava {

Value VM::StartAsyncCall(const Value& closure_val, const avastd::vector<Value>& args) {
    auto* closure = static_cast<Closure*>(closure_val.obj);

    auto* co = new Coroutine();
    // co->entry es Value -- el copy-assignment ya Retiene (RAII); el
    // Retain() manual duplicaba esa retencion.
    co->entry = closure_val;
    co->status = CoStatus::Running;
    created_coroutines_.push_back(co);

    auto* task = new TaskObj();
    task->co = co;
    co->owner_task = task;
    created_tasks_.push_back(task);

    CallFrame entry_frame;
    entry_frame.proto = closure->proto;
    entry_frame.closure = avastd::shared_ptr<Closure>(closure, [](Closure*) {});
    entry_frame.registers.resize(closure->proto->num_registers);
    for (size_t i = 0; i < args.size() && i + 1 < entry_frame.registers.size(); ++i) {
        entry_frame.registers[i + 1] = args[i];
    }
    entry_frame.argc = static_cast<uint32_t>(args.size());

    coroutine_resumers_.push_back(current_coroutine_);
    saved_frames_.push_back(frames_);
    saved_exception_handlers_.push_back(avastd::move(exception_handlers_));
    exception_handlers_.clear();
    // Relocate (not close) upvalues before dropping the caller's frame
    // stack: frames_ is COPIED into saved_frames_ above (registers get
    // new heap buffers in the copy), so any Upvalue::location still
    // pointing into the original frames_ would dangle the moment
    // clear() below destroys it. Closing here would freeze a snapshot
    // and silently disconnect any closure sharing that upvalue from the
    // real register from this point on (bug #4). Instead, repoint each
    // open upvalue at the matching register in the just-made copy
    // (saved_frames_.back()), which is the buffer that actually survives
    // and eventually becomes frames_ again -- so the closure keeps
    // reading/writing the real, live value across the suspend.
    {
        auto& saved_copy = saved_frames_.back();
        for (size_t i = 0; i < frames_.size(); ++i) {
            RelocateUpvalues(frames_[i], saved_copy[i]);
        }
    }
    frames_.clear();
    frames_.push_back(entry_frame);
    current_coroutine_ = co;
    is_coroutine_suspended_ = false;

    Value result = Value::Nil();
    bool errored = false;
    Value error_val;
#if AVA_HAVE_EXCEPTIONS
    try {
        result = ResumeFromTop();
    } catch (const AvaRaiseException&) {
        errored = true;
        error_val = GetAndClearException();
    } catch (const avastd::exception& e) {
        errored = true;
        error_val = Value::String(e.what());
    }
#else
    // Sin RTTI real, no hay catch(TipoA&)/catch(TipoB&) separados -- un
    // solo AVA_CATCH + el tag manual (ver ava_type_tag() en ava_error.h y
    // el mismo patron resuelto en vm.cpp::ExecuteFrame) decide cual de
    // los dos casos originales aplica.
    AVA_TRY {
        result = ResumeFromTop();
    } AVA_CATCH(avastd::exception, e) {
        errored = true;
        error_val = (e.ava_type_tag() == 1) ? GetAndClearException() : Value::String(e.what());
    }
#endif

    current_coroutine_ = coroutine_resumers_.back();
    coroutine_resumers_.pop_back();

    co->frames = frames_;
    co->status = errored ? CoStatus::Dead : (is_coroutine_suspended_ ? CoStatus::Suspended : CoStatus::Dead);
    if (co->status == CoStatus::Dead) ReleaseDeadCoroutineState(*co);
    co->exception_handlers = exception_handlers_;
    frames_ = avastd::move(saved_frames_.back());
    saved_frames_.pop_back();
    exception_handlers_ = avastd::move(saved_exception_handlers_.back());
    saved_exception_handlers_.pop_back();
    is_coroutine_suspended_ = false;

    if (errored) {
        SettleTask(task, error_val, true);
    } else if (co->status == CoStatus::Dead) {
        SettleTask(task, result, false);
    }

    return Value::Task(task);
}

Value VM::StartAsyncBoundCall(const Value& bound_val, const avastd::vector<Value>& args,
                               ClassObj* base_lookup_class) {
    auto* bound = static_cast<BoundMethod*>(bound_val.obj);

    auto* co = new Coroutine();
    co->entry = bound_val;
    co->status = CoStatus::Running;
    created_coroutines_.push_back(co);

    auto* task = new TaskObj();
    task->co = co;
    co->owner_task = task;
    created_tasks_.push_back(task);

    CallFrame entry_frame;
    entry_frame.proto = bound->proto;
    entry_frame.registers.resize(bound->proto->num_registers);
    if (!entry_frame.registers.empty()) {
        entry_frame.registers[0] = bound->instance;
    }
    for (size_t i = 0; i < args.size() && i + 1 < entry_frame.registers.size(); ++i) {
        entry_frame.registers[i + 1] = args[i];
    }
    entry_frame.argc = static_cast<uint32_t>(args.size());
    // Bug #17: propaga la clase dueña del método (ver comentario en
    // vm.h) para que un base.xxx() DENTRO de este método async, si lo
    // hay, siga resolviendo la cadena de herencia correctamente en vez
    // de quedar pegado en el mismo nivel (mismo mecanismo del bug #14).
    entry_frame.base_lookup_class = base_lookup_class;

    coroutine_resumers_.push_back(current_coroutine_);
    saved_frames_.push_back(frames_);
    saved_exception_handlers_.push_back(avastd::move(exception_handlers_));
    exception_handlers_.clear();
    // See the matching comment in StartAsyncCall above: relocate (not
    // close) so closures sharing an upvalue with a frame on the caller
    // stack keep reading/writing the real, live register across the
    // suspend instead of getting silently disconnected into a frozen
    // snapshot (bug #4).
    {
        auto& saved_copy = saved_frames_.back();
        for (size_t i = 0; i < frames_.size(); ++i) {
            RelocateUpvalues(frames_[i], saved_copy[i]);
        }
    }
    frames_.clear();
    frames_.push_back(entry_frame);
    current_coroutine_ = co;
    is_coroutine_suspended_ = false;

    Value result = Value::Nil();
    bool errored = false;
    Value error_val;
#if AVA_HAVE_EXCEPTIONS
    try {
        result = ResumeFromTop();
    } catch (const AvaRaiseException&) {
        errored = true;
        error_val = GetAndClearException();
    } catch (const avastd::exception& e) {
        errored = true;
        error_val = Value::String(e.what());
    }
#else
    // Sin RTTI real, no hay catch(TipoA&)/catch(TipoB&) separados -- un
    // solo AVA_CATCH + el tag manual (ver ava_type_tag() en ava_error.h y
    // el mismo patron resuelto en vm.cpp::ExecuteFrame) decide cual de
    // los dos casos originales aplica.
    AVA_TRY {
        result = ResumeFromTop();
    } AVA_CATCH(avastd::exception, e) {
        errored = true;
        error_val = (e.ava_type_tag() == 1) ? GetAndClearException() : Value::String(e.what());
    }
#endif

    current_coroutine_ = coroutine_resumers_.back();
    coroutine_resumers_.pop_back();

    co->frames = frames_;
    co->status = errored ? CoStatus::Dead : (is_coroutine_suspended_ ? CoStatus::Suspended : CoStatus::Dead);
    if (co->status == CoStatus::Dead) ReleaseDeadCoroutineState(*co);
    co->exception_handlers = exception_handlers_;
    frames_ = avastd::move(saved_frames_.back());
    saved_frames_.pop_back();
    exception_handlers_ = avastd::move(saved_exception_handlers_.back());
    saved_exception_handlers_.pop_back();
    is_coroutine_suspended_ = false;

    if (errored) {
        SettleTask(task, error_val, true);
    } else if (co->status == CoStatus::Dead) {
        SettleTask(task, result, false);
    }

    return Value::Task(task);
}

void VM::ResumeAwaitingCoroutine(Coroutine* co, Value value, bool is_error) {
    if (co->status != CoStatus::Suspended) return;

    co->status = CoStatus::Running;
    coroutine_resumers_.push_back(current_coroutine_);
    saved_frames_.push_back(frames_);
    saved_exception_handlers_.push_back(avastd::move(exception_handlers_));
    exception_handlers_ = co->exception_handlers;
    frames_ = co->frames;
    current_coroutine_ = co;

    auto& top_frame = frames_.back();
    if (top_frame.proto && top_frame.pc > 0 && top_frame.pc <= top_frame.proto->instructions.size()) {
        const Instr& await_in = top_frame.proto->instructions[top_frame.pc - 1];
        if (await_in.op == OpCode::AWAIT && await_in.a < top_frame.registers.size()) {
            if (is_error) {
                top_frame.pending_await_error = true;
                top_frame.pending_await_error_value = value;
            } else {
                top_frame.registers[await_in.a] = value;
            }
        }
    }

    is_coroutine_suspended_ = false;
    Value result = Value::Nil();
    bool errored = false;
    Value error_val;
#if AVA_HAVE_EXCEPTIONS
    try {
        result = ResumeFromTop();
    } catch (const AvaRaiseException&) {
        errored = true;
        error_val = GetAndClearException();
    } catch (const avastd::exception& e) {
        errored = true;
        error_val = Value::String(e.what());
    }
#else
    // Sin RTTI real, no hay catch(TipoA&)/catch(TipoB&) separados -- un
    // solo AVA_CATCH + el tag manual (ver ava_type_tag() en ava_error.h y
    // el mismo patron resuelto en vm.cpp::ExecuteFrame) decide cual de
    // los dos casos originales aplica.
    AVA_TRY {
        result = ResumeFromTop();
    } AVA_CATCH(avastd::exception, e) {
        errored = true;
        error_val = (e.ava_type_tag() == 1) ? GetAndClearException() : Value::String(e.what());
    }
#endif

    current_coroutine_ = coroutine_resumers_.back();
    coroutine_resumers_.pop_back();

    co->frames = frames_;
    co->status = errored ? CoStatus::Dead : (is_coroutine_suspended_ ? CoStatus::Suspended : CoStatus::Dead);
    if (co->status == CoStatus::Dead) ReleaseDeadCoroutineState(*co);
    co->exception_handlers = exception_handlers_;
    frames_ = avastd::move(saved_frames_.back());
    saved_frames_.pop_back();
    exception_handlers_ = avastd::move(saved_exception_handlers_.back());
    saved_exception_handlers_.pop_back();
    is_coroutine_suspended_ = false;

    auto* task = co->owner_task;
    if (!task) return;
    if (errored) {
        SettleTask(task, error_val, true);
    } else if (co->status == CoStatus::Dead) {
        SettleTask(task, result, false);
    }
}

Value VM::CreateTimerTask(uint32_t delay_ms) {
    auto* task = new TaskObj();
    created_tasks_.push_back(task);

    OnAsyncTimerScheduled();
    VmPlatformAccessor::Get().Timer().ScheduleOnce(delay_ms, [this, task]() {
        PostAsyncTask([this, task]() {
            SettleTask(task, Value::Nil(), false);
        });
        OnAsyncTimerConsumed();
    });

    return Value::Task(task);
}

void VM::SettleTask(TaskObj* task, Value value, bool is_error) {
    if (task->done) return;
    task->done = true;
    task->has_error = is_error;
    // `value` llega por copia (el caller ya pago un Retain al construir
    // el parametro); estos copy-assignments a task->error/result ya
    // Retienen por si solos (RAII). Los Retain() manuales que seguian
    // duplicaban esa retencion.
    if (is_error) {
        task->error = value;
    } else {
        task->result = value;
    }

    auto awaiters = avastd::move(task->awaiters);
    task->awaiters.clear();
    for (auto* awaiter_co : awaiters) {
        // Copy-construccion de v desde `value` ya Retiene; la captura
        // por valor del lambda de abajo tambien copy-construye su
        // propia copia (Retiene de nuevo, correctamente, porque es una
        // referencia independiente que vive en el closure). El Retain(v)
        // manual que habia aca era una retencion de mas sobre v.
        Value v = value;
        PostAsyncTask([this, awaiter_co, v, is_error]() {
            ResumeAwaitingCoroutine(awaiter_co, v, is_error);
        });
    }
}

} // namespace ava
