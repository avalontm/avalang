#include "vm.h"
#include "vm_internal.h"

namespace ava {

void OpCall(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm) {
    uint8_t save_a = in.a;
    std::vector<Value> args(
        frame.registers.begin() + in.a + 1,
        frame.registers.begin() + in.a + 1 + in.b);
    const Value& callee = frame.registers[in.a];

    if (callee.type == ValueType::Bound) {
        auto* bound = static_cast<BoundMethod*>(callee.obj);
        std::vector<Value> all_args;
        all_args.push_back(bound->instance);
        all_args.insert(all_args.end(), args.begin(), args.end());
        CallFrame callee_frame;
        callee_frame.proto = bound->proto;
        callee_frame.registers.resize(std::max<size_t>(callee_frame.registers.size(), bound->proto->num_registers));
        for (size_t i = 0; i < all_args.size() && i < callee_frame.registers.size(); ++i) {
            callee_frame.registers[i] = all_args[i];
        }
        callee_frame.argc = static_cast<uint32_t>(args.size());
        // FASE 1: guardar en que registro del frame llamador hay que
        // escribir el resultado cuando ResumeFromTop() desenrolle este
        // frame en un resume() posterior (yield anidado).
        callee_frame.ret_slot = static_cast<int>(save_a);
        // Ver nota en OpBaseCall más abajo: `frame` puede quedar colgante
        // si push_back reasigna vm.frames_. Guardamos el índice antes.
        size_t frame_idx_bound = static_cast<size_t>(&frame - vm.frames_.data());
        vm.frames_.push_back(callee_frame);
        Value result = vm.ExecuteFrame(vm.frames_.size() - 1);
        
        if (vm.is_coroutine_suspended_) {
            return;
        }
        
        vm.frames_.pop_back();
        vm.frames_[frame_idx_bound].registers[save_a] = result;
    } else if (callee.type == ValueType::Class) {
        auto* cls = static_cast<ClassObj*>(callee.obj);
        auto* inst = new InstanceObj();
        inst->cls = cls;
        inst->attrs = cls->instance_defaults;
        
        Value cls_val;
        cls_val.type = ValueType::Class;
        cls_val.obj = cls;
        inst->attrs["__class__"] = cls_val;
        
        auto base_it = cls->attrs.find("__base__");
        if (base_it != cls->attrs.end()) {
            inst->attrs["__base__"] = base_it->second;
        }
        
        Value v; v.type = ValueType::Instance; v.obj = inst;
        Retain(v);
        frame.registers[save_a] = v;
        
        auto init_it = cls->methods.find("__init__");
        if (init_it != cls->methods.end()) {
            std::vector<Value> init_args;
            init_args.push_back(v);
            init_args.insert(init_args.end(), args.begin(), args.end());
            
            CallFrame init_frame;
            init_frame.proto = init_it->second;
            init_frame.registers.resize(init_it->second->num_registers);
            for (size_t i = 0; i < init_args.size() && i < init_frame.registers.size(); ++i) {
                init_frame.registers[i] = init_args[i];
            }
            init_frame.argc = static_cast<uint32_t>(args.size());
            // El resultado de __init__ se descarta (-1), tanto en el
            // camino recursivo normal como en el desenrollado por
            // ResumeFromTop() tras un yield anidado dentro de __init__.
            init_frame.ret_slot = -1;
            // No necesitamos escribir en `frame` después de esto (el
            // resultado del __init__ se descarta), pero el push_back
            // igual puede invalidar `frame`; no se vuelve a usar tras
            // este bloque en esta rama, así que no hace falta el índice.
            vm.frames_.push_back(init_frame);
            vm.ExecuteFrame(vm.frames_.size() - 1);
            
            if (vm.is_coroutine_suspended_) {
                return;
            }
            
            vm.frames_.pop_back();
        }
    } else if (callee.type == ValueType::Native) {
        auto* native = static_cast<NativeObj*>(callee.obj);
        std::vector<ava_value_t> c_args;
        
        if (native->is_primitive_method) {
            c_args.push_back(native->primitive_this);
            c_args.reserve(args.size() + 1);
            for (auto& a : args) c_args.push_back(ToC(a));
        } else {
            c_args.reserve(args.size());
            for (auto& a : args) c_args.push_back(ToC(a));
        }
        
        ava_value_t c_result = native->fn(
            reinterpret_cast<AvaVM*>(&vm),
            c_args.empty() ? nullptr : c_args.data(),
            c_args.size(),
            native->user_data
        );
        frame.registers[save_a] = FromC(c_result);
    } else if (callee.type == ValueType::Function) {
        auto* closure = static_cast<Closure*>(callee.obj);
        CallFrame callee_frame;
        callee_frame.proto = closure->proto;
        callee_frame.closure = std::shared_ptr<Closure>(closure, [](Closure*) {});
        callee_frame.registers.resize(closure->proto->num_registers);
        for (size_t i = 0; i < args.size() && i + 1 < callee_frame.registers.size(); ++i) {
            callee_frame.registers[i + 1] = args[i];
        }
        callee_frame.argc = static_cast<uint32_t>(args.size());
        callee_frame.ret_slot = static_cast<int>(save_a);
        size_t frame_idx_fn = static_cast<size_t>(&frame - vm.frames_.data());
        vm.frames_.push_back(callee_frame);
        Value result = vm.ExecuteFrame(vm.frames_.size() - 1);
        
        if (vm.is_coroutine_suspended_) {
            return;
        }
        
        vm.frames_.pop_back();
        vm.frames_[frame_idx_fn].registers[save_a] = result;
    } else {
        throw std::runtime_error("attempt to call a non-callable value");
    }
}

void OpReturn(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm) {
    // Handled by ExecuteFrame return statement
}

void OpClosure(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm) {
    auto& child_proto = frame.proto->child_protos[in.b];
    auto* closure = new Closure();
    closure->proto = child_proto;
    for (size_t i = 0; i < child_proto->upvalue_descs.size(); ++i) {
        auto& uvd = child_proto->upvalue_descs[i];
        if (uvd.from_parent_local) {
            auto upval = std::make_shared<Upvalue>();
            upval->location = &frame.registers[uvd.index];
            upval->value = frame.registers[uvd.index];
            Retain(upval->value);
            closure->upvalues.push_back(upval);
        }
    }
    Value v; v.type = ValueType::Function; v.obj = closure;
    Retain(v);
    frame.registers[in.a] = v;
}

void OpGetUpval(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm) {
    auto* closure = frame.closure.get();
    if (closure && in.b < closure->upvalues.size()) {
        frame.registers[in.a] = closure->upvalues[in.b]->value;
    } else {
        frame.registers[in.a] = Value::Nil();
    }
}

void OpSetUpval(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm) {
    auto* closure = frame.closure.get();
    if (closure && in.b < closure->upvalues.size()) {
        Release(closure->upvalues[in.b]->value);
        closure->upvalues[in.b]->value = frame.registers[in.a];
        closure->upvalues[in.b]->location = &closure->upvalues[in.b]->value;
        Retain(closure->upvalues[in.b]->value);
    }
}

Value OpBaseCall(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm) {
    auto& this_val = frame.registers[0];
    auto* attr_name = static_cast<StringObj*>(K[in.b].obj);
    if (this_val.type == ValueType::Instance) {
        auto* inst = static_cast<InstanceObj*>(this_val.obj);
        auto base_it = inst->attrs.find("__base__");
        if (base_it != inst->attrs.end() && base_it->second.type == ValueType::Class) {
            auto* base_cls = static_cast<ClassObj*>(base_it->second.obj);
            auto method_it = base_cls->methods.find(attr_name->data);
            if (method_it != base_cls->methods.end()) {
                std::vector<Value> args(in.c + 1);
                args[0] = this_val;
                for (uint16_t i = 0; i < in.c; ++i) {
                    args[i + 1] = frame.registers[1 + i];
                }
                CallFrame callee_frame;
                callee_frame.proto = method_it->second;
                callee_frame.registers.resize(std::max<size_t>(method_it->second->num_registers, args.size()));
                for (size_t i = 0; i < args.size() && i < callee_frame.registers.size(); ++i) {
                    callee_frame.registers[i] = args[i];
                }
                callee_frame.argc = static_cast<uint32_t>(in.c);
                callee_frame.ret_slot = static_cast<int>(in.a);

                // `frame` es una referencia a un elemento de vm.frames_.
                // El push_back de abajo puede reasignar el buffer interno
                // del vector (si excede su capacidad), lo que invalida
                // cualquier referencia/puntero previo a sus elementos --
                // incluida `frame`. Guardamos el índice ANTES de tocar el
                // vector y lo usamos para escribir el resultado, igual
                // que hace el resto del dispatcher (frames_[frame_idx])
                // en vez de reusar `frame` después del push/pop.
                size_t frame_idx = static_cast<size_t>(&frame - vm.frames_.data());

                vm.frames_.push_back(callee_frame);
                Value result = vm.ExecuteFrame(vm.frames_.size() - 1);

                // FASE 1 (async/await): si la ejecución del método base se
                // suspendió (yield/await anidado), NO hacer pop_back ni
                // escribir el resultado -- el callee_frame debe quedar
                // vivo en vm.frames_ para que resume() lo retome más
                // tarde exactamente donde quedó. Propagamos `result` (el
                // valor "yielded") hacia arriba sin tocarlo, igual que
                // hace CALL en vm.cpp.
                if (vm.is_coroutine_suspended_) {
                    return result;
                }

                vm.frames_.pop_back();
                vm.frames_[frame_idx].registers[in.a] = result;
                return result;
            }
        }
    }
    throw std::runtime_error("base() call failed - __base__ not found");
}

} // namespace ava