#include "vm.h"
#include "vm_internal.h"
#include "module.h"
#include "coroutine.h"
#include "../frontend/frontend.h"
#include "../../platform/barekernel/stdcompat/ava_stdcompat.h"

namespace ava {

Value VM::ResumeFromTop() {
    Value result = Value::Nil();
    while (!frames_.empty()) {
        size_t top = frames_.size() - 1;
        result = ExecuteFrame(top);
        if (is_coroutine_suspended_) {

            return result;
        }

        int ret_slot = frames_[top].ret_slot;
        frames_.pop_back();
        if (frames_.empty()) {
            return result;
        }
        if (ret_slot >= 0 && static_cast<size_t>(ret_slot) < frames_.back().registers.size()) {
            frames_.back().registers[static_cast<size_t>(ret_slot)] = result;
        }

    }
    return result;
}

Value VM::Call(const Value& callable, const avastd::vector<Value>& args) {
    if (callable.type == ValueType::Native) {
        auto* native = static_cast<NativeObj*>(callable.obj);
        avastd::vector<ava_value_t> c_args;
        c_args.reserve(args.size());
        for (auto& a : args) c_args.push_back(ToC(a));
        ava_value_t c_result = native->fn(
            reinterpret_cast<AvaVM*>(this),
            c_args.empty() ? nullptr : c_args.data(),
            c_args.size(),
            native->user_data
        );
        return FromC(c_result);
    }

    if (callable.type == ValueType::Bound) {
        auto* bound = static_cast<BoundMethod*>(callable.obj);
        if (bound->proto->is_async) {
            return StartAsyncBoundCall(callable, args);
        }
        avastd::vector<Value> all_args;
        all_args.push_back(bound->instance);
        all_args.insert(all_args.end(), args.begin(), args.end());
        
        CallFrame frame;
        frame.proto = bound->proto;
        frame.registers.resize(bound->proto->num_registers);
        for (auto& reg : frame.registers) {
            reg = Value::Nil();
        }
        for (size_t i = 0; i < all_args.size() && i + 1 < frame.registers.size(); ++i) {
            frame.registers[i + 1] = all_args[i];
        }
        frame.argc = static_cast<uint32_t>(args.size());
        frames_.push_back(frame);
        Value result = ExecuteFrame(frames_.size() - 1);
		
        if (is_coroutine_suspended_) {
            return result;
        }
        frames_.pop_back();
        return result;
    }

    if (callable.type == ValueType::Function) {
        auto* closure = static_cast<Closure*>(callable.obj);
        if (closure->proto->is_async) {
            return StartAsyncCall(callable, args);
        }
        CallFrame frame;
        frame.proto = closure->proto;
        frame.closure = avastd::shared_ptr<Closure>(closure, [](Closure*) {});
        frame.registers.resize(closure->proto->num_registers);
        for (auto& reg : frame.registers) {
            reg = Value::Nil();
        }
        for (size_t i = 0; i < args.size() && i + 1 < frame.registers.size(); ++i) {
            frame.registers[i + 1] = args[i];
        }
        frame.argc = static_cast<uint32_t>(args.size());
        frames_.push_back(frame);
        Value result = ExecuteFrame(frames_.size() - 1);
        if (is_coroutine_suspended_) {
            return result;
        }
        frames_.pop_back();
        return result;
    }

    if (callable.type == ValueType::Coroutine) {
        auto* co = reinterpret_cast<Coroutine*>(callable.obj);
        if (co->status == CoStatus::Running) {
            AVA_THROW(avastd::runtime_error("attempt to call a running coroutine"));
        }

        co->status = CoStatus::Running;
        coroutine_resumers_.push_back(current_coroutine_);

        saved_frames_.push_back(frames_);
        saved_exception_handlers_.push_back(avastd::move(exception_handlers_));
        exception_handlers_.clear();
        frames_ = co->frames;
        current_coroutine_ = co;

        if (frames_.empty() || !frames_[0].proto) {
            auto* closure = static_cast<Closure*>(co->entry.obj);
            CallFrame resume_frame;
            resume_frame.proto = closure->proto;
            resume_frame.closure = avastd::shared_ptr<Closure>(closure, [](Closure*) {});
            resume_frame.registers.resize(closure->proto->num_registers);
            for (size_t i = 0; i < args.size() && i < resume_frame.registers.size(); ++i) {
                resume_frame.registers[i] = args[i];
            }
            resume_frame.argc = static_cast<uint32_t>(args.size());
            frames_.push_back(resume_frame);
        } else {
            // Resuming a coroutine that's already suspended on a `yield`:
            // whatever's passed to resume() now becomes the *result* of that
            // `yield` expression, not a fresh set of call arguments. The
            // frame that's about to continue is frames_.back() (the one that
            // was actively running when the whole call chain suspended), and
            // its pc already advanced past the YIELD instruction that put it
            // to sleep -- so code[pc-1] IS that YIELD instruction, and its
            // `a` field tells us exactly which register to overwrite with
            // the resume value (packed into a list the same way multi-value
            // yields are packed, so `x = yield a, b` / `resume(co, 1, 2)`
            // lines up with `x, y = yield a, b`-style unpacking).
            auto& top_frame = frames_.back();
            bool wrote_resume_value = false;
            if (top_frame.proto && top_frame.pc > 0 &&
                top_frame.pc <= top_frame.proto->instructions.size()) {
                const Instr& yield_in = top_frame.proto->instructions[top_frame.pc - 1];
                if (yield_in.op == OpCode::YIELD && yield_in.a < top_frame.registers.size()) {
                    Value resume_val = Value::Nil();
                    if (args.size() == 1) {
                        resume_val = args[0];
                    } else if (args.size() > 1) {
                        auto* list = new ListObj();
                        list->items = args;
                        resume_val.type = ValueType::List;
                        resume_val.obj = list;
                    }
                    top_frame.registers[yield_in.a] = resume_val;
                    wrote_resume_value = true;
                }
            }
            if (!wrote_resume_value) {
                // Defensive fallback (should not normally happen): keep the
                // old behavior so a malformed/unexpected suspension point
                // doesn't silently drop the resume args.
                for (size_t i = 0; i < args.size() && i < frames_[0].registers.size(); ++i) {
                    frames_[0].registers[i] = args[i];
                }
            }
        }

        is_coroutine_suspended_ = false;
        Value result = ResumeFromTop();

        current_coroutine_ = coroutine_resumers_.back();
        coroutine_resumers_.pop_back();

        co->frames = frames_;
        co->status = is_coroutine_suspended_ ? CoStatus::Suspended : CoStatus::Dead;
        if (co->status == CoStatus::Dead) ReleaseDeadCoroutineState(*co);
        frames_ = avastd::move(saved_frames_.back());
        saved_frames_.pop_back();
        exception_handlers_ = avastd::move(saved_exception_handlers_.back());
        saved_exception_handlers_.pop_back();

        is_coroutine_suspended_ = false;

        return result;
    }

    AVA_THROW(avastd::runtime_error("attempt to call a non-callable value (type=" +
                              avastd::to_string(static_cast<int>(callable.type)) + ")"));
}

} // namespace ava