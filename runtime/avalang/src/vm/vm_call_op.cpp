#include "vm.h"
#include "vm_internal.h"

namespace ava {
Value OpBaseCall(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm) {
    auto& this_val = frame.registers[in.a];
    auto* attr_name = static_cast<StringObj*>(K[in.b].obj);
    if (this_val.type == ValueType::Instance) {
        auto* inst = static_cast<InstanceObj*>(this_val.obj);
        // Bug #14 (herencia de 3+ niveles, `base.__init__()` encadenado
        // crashea con stack overflow): ANTES esta lÃ­nea leÃ­a siempre
        // `inst->attrs["__base__"]`, que es fijo (se cachea una sola vez
        // en la instancia al crearla, y siempre apunta al padre inmediato
        // de la clase MÃS DERIVADA). Con 2 niveles (Dog:Animal) eso
        // coincide con "la base de la clase que estÃ¡ corriendo ahora", asÃ­
        // que funcionaba por casualidad. Con 3+ niveles (Puppy:Dog:Animal)
        // deja de coincidir a partir del segundo hop: dentro de
        // Dog.__init__ (ejecutÃ¡ndose porque Puppy.__init__ ya llamÃ³ a
        // base.__init__ una vez), un SEGUNDO base.__init__() volvÃ­a a leer
        // el mismo `inst->attrs["__base__"]` = Dog (no Animal), encontraba
        // Dog.__init__ de nuevo y se volvÃ­a a llamar a sÃ­ mismo -- recursiÃ³n
        // infinita. `frame.base_lookup_class` (seteado por el CALL de
        // instanciaciÃ³n con la clase hoja, y propagado mÃ¡s abajo en este
        // mismo opcode con la clase base reciÃ©n resuelta) es la clase que
        // estÃ¡ corriendo ESTE frame, asÃ­ que buscar el `__base__` DE ESA
        // CLASE (no el de la instancia) avanza un nivel real por cada
        // base.xxx() en vez de quedarse pegado en el mismo. Fallback (frame
        // sin tag, ej. invocado desde un mÃ©todo normal vÃ­a BoundMethod en
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
                        call_args[i] = frame.registers[in.a + 1 + i];
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
                    args[i + 1] = frame.registers[in.a + 1 + i];
                }
                CallFrame callee_frame;
                callee_frame.proto = method_it->second;
                callee_frame.registers.resize(avastd::max<size_t>(method_it->second->num_registers, args.size()));
                for (size_t i = 0; i < args.size() && i < callee_frame.registers.size(); ++i) {
                    callee_frame.registers[i] = args[i];
                }
                callee_frame.argc = static_cast<uint32_t>(in.c);
                callee_frame.ret_slot = static_cast<int>(in.a);
                // Propaga la clase dueÃ±a de ESTE mÃ©todo (base_cls) al frame
                // que estÃ¡ por correr, para que si ese mÃ©todo a su vez hace
                // otro base.xxx(), el prÃ³ximo hop se resuelva desde acÃ¡ y
                // no vuelva a caer en el mismo nivel -- ver comentario del
                // bug #14 mÃ¡s arriba.
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
