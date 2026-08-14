#include "vm.h"
#include "vm_internal.h"
#include "module.h"
#include "coroutine.h"
#include "../frontend/frontend.h"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <iomanip>
#include <cmath>

namespace ava {

Value VM::ResumeFromTop() {
    Value result = Value::Nil();
    while (!frames_.empty()) {
        size_t top = frames_.size() - 1;
        result = ExecuteFrame(top);
        if (is_coroutine_suspended_) {
            // Suspendido en algun punto de `top` (o mas profundo, ya
            // propagado hacia arriba por CALL/OpBaseCall) -- frames_
            // queda intacto tal cual, listo para el proximo resume().
            return result;
        }
        // `top` termino normalmente (RETURN o fin de funcion). Sacarlo
        // y, si hay un frame padre, escribir el resultado en el
        // registro que el CALL original pidio (ret_slot) antes de
        // seguir ejecutando ese padre desde donde se habia quedado.
        int ret_slot = frames_[top].ret_slot;
        frames_.pop_back();
        if (frames_.empty()) {
            return result;
        }
        if (ret_slot >= 0 && static_cast<size_t>(ret_slot) < frames_.back().registers.size()) {
            frames_.back().registers[static_cast<size_t>(ret_slot)] = result;
        }
        // Loop: sigue con el nuevo top (el padre), continuando su pc
        // justo despues del CALL que lo suspendio.
    }
    return result;
}

Value VM::Call(const Value& callable, const std::vector<Value>& args) {
    if (callable.type == ValueType::Native) {
        auto* native = static_cast<NativeObj*>(callable.obj);
        std::vector<ava_value_t> c_args;
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
        std::vector<Value> all_args;
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
        // FASE 1 (async/await): si algo dentro de este call se suspendió
        // (yield/await anidado alcanzado desde código nativo, ej. un
        // callback pasado a un builtin), no hacer pop_back -- el frame
        // debe seguir vivo en frames_ para que resume() lo retome. El
        // llamador nativo de VM::Call() que reciba `result` en este caso
        // está recibiendo el valor "yielded", no un valor de retorno
        // normal; falta que cada builtin que invoque callbacks vía
        // VM::Call() revise vm->is_coroutine_suspended_ y propague --
        // eso queda fuera de este fix puntual, pero al menos ya no se
        // corrompe el frame suspendido aquí.
        if (is_coroutine_suspended_) {
            return result;
        }
        frames_.pop_back();
        return result;
    }

    if (callable.type == ValueType::Function) {
        auto* closure = static_cast<Closure*>(callable.obj);
        CallFrame frame;
        frame.proto = closure->proto;
        frame.closure = std::shared_ptr<Closure>(closure, [](Closure*) {});
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
        // Ver nota FASE 1 arriba (misma razón, misma solución).
        if (is_coroutine_suspended_) {
            return result;
        }
        frames_.pop_back();
        return result;
    }

    if (callable.type == ValueType::Coroutine) {
        auto* co = reinterpret_cast<Coroutine*>(callable.obj);
        if (co->status == CoStatus::Running) {
            throw std::runtime_error("attempt to call a running coroutine");
        }

        co->status = CoStatus::Running;
        coroutine_resumers_.push_back(current_coroutine_);

        saved_frames_ = frames_;
        frames_ = co->frames;
        current_coroutine_ = co;

        if (frames_.empty() || !frames_[0].proto) {
            auto* closure = static_cast<Closure*>(co->entry.obj);
            CallFrame resume_frame;
            resume_frame.proto = closure->proto;
            resume_frame.closure = std::shared_ptr<Closure>(closure, [](Closure*) {});
            resume_frame.registers.resize(closure->proto->num_registers);
            for (size_t i = 0; i < args.size() && i < resume_frame.registers.size(); ++i) {
                resume_frame.registers[i] = args[i];
            }
            resume_frame.argc = static_cast<uint32_t>(args.size());
            frames_.push_back(resume_frame);
        } else {
            for (size_t i = 0; i < args.size() && i < frames_[0].registers.size(); ++i) {
                frames_[0].registers[i] = args[i];
            }
        }

        is_coroutine_suspended_ = false;
        Value result = ResumeFromTop();

        current_coroutine_ = coroutine_resumers_.back();
        coroutine_resumers_.pop_back();

        co->frames = frames_;
        co->status = is_coroutine_suspended_ ? CoStatus::Suspended : CoStatus::Dead;
        frames_ = saved_frames_;

        return result;
    }

    throw std::runtime_error("attempt to call a non-callable value (type=" +
                              std::to_string(static_cast<int>(callable.type)) + ")");
}

} // namespace ava