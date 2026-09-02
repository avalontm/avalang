#include "vm.h"
#include "vm_internal.h"
#include "coroutine.h"
#include "task.h"

namespace ava {

void CollectFrameRoots(avastd::vector<CallFrame>& frames, avastd::vector<Value*>& out) {
    for (auto& f : frames) {
        for (auto& r : f.registers) out.push_back(&r);
        // Siempre se agrega, este `pending_await_error` o no: cuando esta
        // en false el Value debe ser Nil (no ref-counted, ver
        // Value::IsRefCounted()), asi que incluirlo sin condicionar no
        // cuesta nada y evita depender de que ese invariante se mantenga
        // para siempre en otro archivo.
        out.push_back(&f.pending_await_error_value);
    }
}

void CollectCoroutineRoots(Coroutine& co, avastd::vector<Value*>& out) {
    CollectFrameRoots(co.frames, out);
    for (auto& v : co.yielded_values) out.push_back(&v);
    out.push_back(&co.entry);
}

void ReleaseDeadCoroutineState(Coroutine& co) {
    co.entry = Value::Nil();
    co.frames.clear();
    co.yielded_values.clear();
}

void CollectTaskRoots(TaskObj& task, avastd::vector<Value*>& out) {
    out.push_back(&task.result);
    out.push_back(&task.error);
}

void OpYield(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm) {
    if (vm.coroutine_resumers_.empty()) {
        AVA_THROW(avastd::runtime_error("attempt to yield outside of coroutine"));
    }

    Value result = Value::Nil();
    if (in.b > 0 && in.a < frame.registers.size()) {
        avastd::vector<Value> yielded;
        for (uint8_t i = 0; i < in.b && (in.a + i) < frame.registers.size(); ++i) {
            yielded.push_back(frame.registers[in.a + i]);
        }
        auto* list = new ListObj();
        list->items = avastd::move(yielded);
        result.type = ValueType::List;
        result.obj = list;
    }

    vm.is_coroutine_suspended_ = true;
    frame.registers[in.a] = result;
}

void OpResume(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm) {
    size_t frame_idx = static_cast<size_t>(&frame - vm.frames_.data());
    auto& co_val = frame.registers[in.b];
    if (co_val.type != ValueType::Coroutine) {
        AVA_THROW(avastd::runtime_error("attempt to resume a non-coroutine"));
    }
    auto* co = reinterpret_cast<Coroutine*>(co_val.obj);
    if (co->status == CoStatus::Dead) {
        AVA_THROW(avastd::runtime_error("attempt to resume a dead coroutine"));
    }
    if (co->status == CoStatus::Running) {
        AVA_THROW(avastd::runtime_error("attempt to resume a running coroutine"));
    }

    avastd::vector<Value> args(
        frame.registers.begin() + in.b + 1,
        frame.registers.begin() + in.b + 1 + in.c);

    co->status = CoStatus::Running;
    vm.coroutine_resumers_.push_back(vm.current_coroutine_);

    vm.saved_frames_.push_back(vm.frames_);
    vm.saved_exception_handlers_.push_back(avastd::move(vm.exception_handlers_));
    vm.exception_handlers_.clear();
    // saved_frames_.push_back above made a COPY of vm.frames_ (the
    // caller's frames) -- its CallFrame::registers buffers are fresh
    // heap allocations, distinct from the ones still referenced by
    // vm.frames_. The swap below parks the caller's ORIGINAL frames_
    // buffers inside co->frames only temporarily: once the coroutine
    // suspends or dies, `co->frames = vm.frames_;` further down
    // overwrites co->frames with the coroutine's own frames, destroying
    // that parked copy and dangling any Upvalue::location still pointing
    // into it. Relocate those upvalues now to the copy in
    // saved_frames_.back(), which is what vm.frames_ gets restored to
    // (via std::move) once this resume call returns -- mirrors the
    // pattern in vm_task.cpp's StartAsyncCall/StartAsyncBoundCall.
    {
        auto& saved_copy = vm.saved_frames_.back();
        for (size_t i = 0; i < vm.frames_.size(); ++i) {
            vm.RelocateUpvalues(vm.frames_[i], saved_copy[i]);
        }
    }
    avastd::swap(vm.frames_, co->frames);
    vm.current_coroutine_ = co;

    if (vm.frames_.empty() || !vm.frames_[0].proto) {
        auto* closure = static_cast<Closure*>(co->entry.obj);
        CallFrame resume_frame;
        resume_frame.proto = closure->proto;
        resume_frame.closure = avastd::shared_ptr<Closure>(closure, [](Closure*) {});
        resume_frame.registers.resize(closure->proto->num_registers);
        vm.frames_.push_back(resume_frame);
    }
    if (vm.frames_[0].registers.size() < args.size() + 1) {
        vm.frames_[0].registers.resize(args.size() + 1);
    }
    // Same +1 offset as vm_call.cpp's VM::Call (ValueType::Coroutine
    // branch): register 0 is reserved by the compiler's calling
    // convention (`this` for methods / unused slot for free functions),
    // so real parameters start at register 1.
    for (size_t i = 0; i < args.size() && i + 1 < vm.frames_[0].registers.size(); ++i) {
        vm.frames_[0].registers[i + 1] = args[i];
    }
    vm.frames_[0].argc = static_cast<uint32_t>(args.size());

    vm.is_coroutine_suspended_ = false;
    Value result = vm.ResumeFromTop();

    vm.current_coroutine_ = vm.coroutine_resumers_.back();
    vm.coroutine_resumers_.pop_back();

    co->frames = vm.frames_;
    // `co->frames = vm.frames_;` above is a COPY -- co->frames now has
    // its own fresh registers buffers, distinct from vm.frames_'s. Any
    // upvalue opened by a closure captured *during this run of the
    // coroutine* (e.g. one that will itself be called after a later
    // resume) still points into vm.frames_'s buffer, which is about to
    // be destroyed by the move-assignment below. Relocate those
    // upvalues into the copy in co->frames first -- that copy is what
    // gets swapped back into vm.frames_ the next time this coroutine is
    // resumed, so the closure keeps sharing the real, live register.
    if (vm.is_coroutine_suspended_) {
        for (size_t i = 0; i < vm.frames_.size(); ++i) {
            vm.RelocateUpvalues(vm.frames_[i], co->frames[i]);
        }
    }
    co->status = vm.is_coroutine_suspended_ ? CoStatus::Suspended : CoStatus::Dead;
    if (co->status == CoStatus::Dead) ReleaseDeadCoroutineState(*co);
    vm.frames_ = avastd::move(vm.saved_frames_.back());
    vm.saved_frames_.pop_back();
    vm.exception_handlers_ = avastd::move(vm.saved_exception_handlers_.back());
    vm.saved_exception_handlers_.pop_back();

    vm.is_coroutine_suspended_ = false;

    vm.frames_[frame_idx].registers[in.a] = result;
}

void OpAwait(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm) {
    Value& slot = frame.registers[in.a];
    if (slot.type != ValueType::Task) {
        // Awaiting a plain value (not a Task) just yields it back unchanged,
        // same as `await` on a non-Promise in JS -- no suspension needed.
        return;
    }

    auto* task = reinterpret_cast<TaskObj*>(slot.obj);
    if (task->done) {
        if (task->has_error) {
            vm.RaiseException(task->error);
            AVA_THROW(AvaRaiseException());
        }
        frame.registers[in.a] = task->result;
        return;
    }

    if (vm.current_coroutine_ == nullptr) {
        AVA_THROW(avastd::runtime_error("'await' used outside of a running async task"));
    }
    task->awaiters.push_back(vm.current_coroutine_);
    vm.is_coroutine_suspended_ = true;
}

} // namespace ava