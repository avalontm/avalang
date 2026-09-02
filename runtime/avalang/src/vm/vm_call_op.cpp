#include "vm.h"
#include "vm_internal.h"

namespace ava {

void OpCall(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm) {
    uint8_t save_a = in.a;
    avastd::vector<Value> args(
        frame.registers.begin() + in.a + 1,
        frame.registers.begin() + in.a + 1 + in.b);
    const Value& callee = frame.registers[in.a];

    if (callee.type == ValueType::Bound) {
        auto* bound = static_cast<BoundMethod*>(callee.obj);
        if (bound->proto->is_async) {
            size_t frame_idx_bound_async = static_cast<size_t>(&frame - vm.frames_.data());
            Value task = vm.StartAsyncBoundCall(callee, args);
            vm.frames_[frame_idx_bound_async].registers[save_a] = task;
        } else {
        avastd::vector<Value> all_args;
        all_args.push_back(bound->instance);
        all_args.insert(all_args.end(), args.begin(), args.end());
        CallFrame callee_frame;
        callee_frame.proto = bound->proto;
        callee_frame.registers.resize(avastd::max<size_t>(callee_frame.registers.size(), bound->proto->num_registers));
        for (size_t i = 0; i < all_args.size() && i < callee_frame.registers.size(); ++i) {
            callee_frame.registers[i] = all_args[i];
        }
        callee_frame.argc = static_cast<uint32_t>(args.size());
        callee_frame.ret_slot = static_cast<int>(save_a);
        size_t frame_idx_bound = static_cast<size_t>(&frame - vm.frames_.data());
        vm.frames_.push_back(callee_frame);
        Value result = vm.ExecuteFrame(vm.frames_.size() - 1);
        
        if (vm.is_coroutine_suspended_) {
            return;
        }
        
        vm.CloseUpvalues(vm.frames_.back());
        vm.frames_.pop_back();
        vm.frames_[frame_idx_bound].registers[save_a] = result;
        }
    } else if (callee.type == ValueType::Class) {
        auto* cls = static_cast<ClassObj*>(callee.obj);
        auto* inst = new InstanceObj();
        inst->cls = cls;
        inst->attrs = cls->instance_defaults;
        
        // Bug #13 (ver vm.cpp, mismo patron duplicado aca): `cls` viene de
        // `callee`, no de un `new` recien hecho -- copiar `callee` retiene
        // correctamente en vez de armar una Value manual que despliega una
        // Release() de mas al salir de scope.
        Value cls_val = callee;
        inst->attrs["__class__"] = cls_val;
        
        auto base_it = cls->attrs.find("__base__");
        if (base_it != cls->attrs.end()) {
            inst->attrs["__base__"] = base_it->second;
        }
        
        Value v; v.type = ValueType::Instance; v.obj = inst;
        frame.registers[save_a] = v;
        
        auto init_it = cls->methods.find("__init__");
        if (init_it != cls->methods.end()) {
            avastd::vector<Value> init_args;
            init_args.push_back(v);
            init_args.insert(init_args.end(), args.begin(), args.end());
            
            CallFrame init_frame;
            init_frame.proto = init_it->second;
            init_frame.registers.resize(init_it->second->num_registers);
            for (size_t i = 0; i < init_args.size() && i < init_frame.registers.size(); ++i) {
                init_frame.registers[i] = init_args[i];
            }
            init_frame.argc = static_cast<uint32_t>(args.size());
            init_frame.ret_slot = -1;
            vm.frames_.push_back(init_frame);
            vm.ExecuteFrame(vm.frames_.size() - 1);
            
            if (vm.is_coroutine_suspended_) {
                return;
            }
            
            vm.CloseUpvalues(vm.frames_.back());
            vm.frames_.pop_back();
        }
    } else if (callee.type == ValueType::Native) {
        auto* native = static_cast<NativeObj*>(callee.obj);
        avastd::vector<ava_value_t> c_args;
        
        if (native->is_primitive_method) {
            c_args.push_back(native->primitive_this);
            c_args.reserve(args.size() + 1);
            for (auto& a : args) c_args.push_back(ToC(a));
        } else {
            c_args.reserve(args.size());
            for (auto& a : args) c_args.push_back(ToC(a));
        }
        
        size_t frame_idx_native = static_cast<size_t>(&frame - vm.frames_.data());
        ava_value_t c_result = native->fn(
            reinterpret_cast<AvaVM*>(&vm),
            c_args.empty() ? nullptr : c_args.data(),
            c_args.size(),
            native->user_data
        );
        vm.frames_[frame_idx_native].registers[save_a] = FromC(c_result);
    } else if (callee.type == ValueType::Function) {
        auto* closure = static_cast<Closure*>(callee.obj);
        if (closure->proto->is_async) {
            size_t frame_idx_async = static_cast<size_t>(&frame - vm.frames_.data());
            Value task = vm.StartAsyncCall(callee, args);
            vm.frames_[frame_idx_async].registers[save_a] = task;
        } else {
            CallFrame callee_frame;
            callee_frame.proto = closure->proto;
            callee_frame.closure = avastd::shared_ptr<Closure>(closure, [](Closure*) {});
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

            vm.CloseUpvalues(vm.frames_.back());
            vm.frames_.pop_back();
            vm.frames_[frame_idx_fn].registers[save_a] = result;
        }
    } else {
        AVA_THROW(MakeNonCallableError(vm, frame, callee));
    }
}

void OpReturn(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm) {
    // Handled by ExecuteFrame return statement
}

void OpClosure(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm) {
    auto& child_proto = frame.proto->child_protos[in.b];
    auto* closure = new Closure();
    closure->proto = child_proto;
    for (size_t i = 0; i < child_proto->upvalue_descs.size(); ++i) {
        auto& uvd = child_proto->upvalue_descs[i];
        if (uvd.from_parent_local) {
            // See VM::FindOrCreateUpvalue (vm.cpp) -- interns by register
            // index so sibling closures created in this frame that capture
            // the same local share one Upvalue instead of each getting a
            // disconnected copy.
            closure->upvalues.push_back(vm.FindOrCreateUpvalue(frame, uvd.index));
        } else {
            // Bug #3 fix, kept in sync with OpCode::CLOSURE in vm.cpp (this
            // function is defined but not wired to any dispatcher -- same
            // situation as bug #2's fix, corrected here too for
            // consistency). uvd.index chains into this frame's own
            // closure's upvalues (an ancestor beyond the immediate parent).
            auto* parent_closure = frame.closure.get();
            if (parent_closure && uvd.index < parent_closure->upvalues.size()) {
                closure->upvalues.push_back(parent_closure->upvalues[uvd.index]);
            } else {
                closure->upvalues.push_back(avastd::make_shared<Upvalue>());
            }
        }
    }
    Value v; v.type = ValueType::Function; v.obj = closure;
    frame.registers[in.a] = v;
}

void OpGetUpval(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm) {
    auto* closure = frame.closure.get();
    if (closure && in.b < closure->upvalues.size()) {
        // Dereference `location` (see vm.cpp's OpCode::GETUPVAL case for the
        // full rationale) instead of reading the stale `value` snapshot.
        frame.registers[in.a] = *closure->upvalues[in.b]->location;
    } else {
        frame.registers[in.a] = Value::Nil();
    }
}

void OpSetUpval(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm) {
    auto* closure = frame.closure.get();
    if (closure && in.b < closure->upvalues.size()) {
        // Write through `location` (see vm.cpp's OpCode::SETUPVAL case) --
        // do NOT repoint it here, or the shared box breaks on first write.
        *closure->upvalues[in.b]->location = frame.registers[in.a];
    }
}

Value OpBaseCall(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm) {
    auto& this_val = frame.registers[0];
    auto* attr_name = static_cast<StringObj*>(K[in.b].obj);
    if (this_val.type == ValueType::Instance) {
        auto* inst = static_cast<InstanceObj*>(this_val.obj);
        // Bug #14 (herencia de 3+ niveles, `base.__init__()` encadenado
        // crashea con stack overflow): ANTES esta línea leía siempre
        // `inst->attrs["__base__"]`, que es fijo (se cachea una sola vez
        // en la instancia al crearla, y siempre apunta al padre inmediato
        // de la clase MÁS DERIVADA). Con 2 niveles (Dog:Animal) eso
        // coincide con "la base de la clase que está corriendo ahora", así
        // que funcionaba por casualidad. Con 3+ niveles (Puppy:Dog:Animal)
        // deja de coincidir a partir del segundo hop: dentro de
        // Dog.__init__ (ejecutándose porque Puppy.__init__ ya llamó a
        // base.__init__ una vez), un SEGUNDO base.__init__() volvía a leer
        // el mismo `inst->attrs["__base__"]` = Dog (no Animal), encontraba
        // Dog.__init__ de nuevo y se volvía a llamar a sí mismo -- recursión
        // infinita. `frame.base_lookup_class` (seteado por el CALL de
        // instanciación con la clase hoja, y propagado más abajo en este
        // mismo opcode con la clase base recién resuelta) es la clase que
        // está corriendo ESTE frame, así que buscar el `__base__` DE ESA
        // CLASE (no el de la instancia) avanza un nivel real por cada
        // base.xxx() en vez de quedarse pegado en el mismo. Fallback (frame
        // sin tag, ej. invocado desde un método normal vía BoundMethod en
        // vez de la cadena de constructor/base-call) reproduce el
        // comportamiento viejo -- sigue siendo correcto para un solo hop.
        ClassObj* base_cls = nullptr;
        if (frame.base_lookup_class) {
            auto owner_base_it = frame.base_lookup_class->attrs.find("__base__");
            if (owner_base_it != frame.base_lookup_class->attrs.end() &&
                owner_base_it->second.type == ValueType::Class) {
                base_cls = static_cast<ClassObj*>(owner_base_it->second.obj);
            }
        } else {
            auto base_it = inst->attrs.find("__base__");
            if (base_it != inst->attrs.end() && base_it->second.type == ValueType::Class) {
                base_cls = static_cast<ClassObj*>(base_it->second.obj);
            }
        }
        if (base_cls) {
            auto method_it = base_cls->methods.find(attr_name->data);
            if (method_it != base_cls->methods.end()) {
                // Bug #17: base.metodo_async() nunca envolvia su
                // resultado en un Task -- este bloque siempre ejecutaba
                // el metodo base de forma sincrona via ExecuteFrame
                // directo, sin chequear is_async en absoluto (a
                // diferencia de los dos otros caminos de llamada, ya
                // arreglados: CALL sobre ValueType::Function -- funcion
                // libre -- y CALL sobre ValueType::Bound -- obj.metodo()
                // -- bug #16). Fix: si el metodo base es async, armar un
                // BoundMethod sintetico (mismo patron que OpGetAttr en
                // vm_classes.cpp) y despachar por StartAsyncBoundCall,
                // propagando base_cls como base_lookup_class para que un
                // SEGUNDO base.xxx() dentro de este metodo async siga
                // avanzando la cadena de herencia (mismo mecanismo del
                // bug #14).
                if (method_it->second->is_async) {
                    avastd::vector<Value> call_args(in.c);
                    for (uint16_t i = 0; i < in.c; ++i) {
                        call_args[i] = frame.registers[1 + i];
                    }
                    auto* synthetic_bound = new BoundMethod();
                    synthetic_bound->proto = method_it->second;
                    synthetic_bound->instance = this_val;
                    Value bound_val;
                    bound_val.type = ValueType::Bound;
                    bound_val.obj = synthetic_bound;

                    size_t frame_idx_base_async = static_cast<size_t>(&frame - vm.frames_.data());
                    Value task = vm.StartAsyncBoundCall(bound_val, call_args, base_cls);
                    vm.frames_[frame_idx_base_async].registers[in.a] = task;
                    return task;
                }

                avastd::vector<Value> args(in.c + 1);
                args[0] = this_val;
                for (uint16_t i = 0; i < in.c; ++i) {
                    args[i + 1] = frame.registers[1 + i];
                }
                CallFrame callee_frame;
                callee_frame.proto = method_it->second;
                callee_frame.registers.resize(avastd::max<size_t>(method_it->second->num_registers, args.size()));
                for (size_t i = 0; i < args.size() && i < callee_frame.registers.size(); ++i) {
                    callee_frame.registers[i] = args[i];
                }
                callee_frame.argc = static_cast<uint32_t>(in.c);
                callee_frame.ret_slot = static_cast<int>(in.a);
                // Propaga la clase dueña de ESTE método (base_cls) al frame
                // que está por correr, para que si ese método a su vez hace
                // otro base.xxx(), el próximo hop se resuelva desde acá y
                // no vuelva a caer en el mismo nivel -- ver comentario del
                // bug #14 más arriba.
                callee_frame.base_lookup_class = base_cls;

                size_t frame_idx = static_cast<size_t>(&frame - vm.frames_.data());

                vm.frames_.push_back(callee_frame);
                Value result = vm.ExecuteFrame(vm.frames_.size() - 1);

                if (vm.is_coroutine_suspended_) {
                    return result;
                }

                vm.CloseUpvalues(vm.frames_.back());
                vm.frames_.pop_back();
                vm.frames_[frame_idx].registers[in.a] = result;
                return result;
            }
        }
    }
    AVA_THROW(MakeFrameError(frame, "base() call failed - __base__ not found"));
}

} // namespace ava