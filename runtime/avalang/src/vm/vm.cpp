#include "vm.h"
#include "vm_internal.h"
#include "value.h"
#include "../../platform/barekernel/stdcompat/ava_stdcompat.h"

namespace ava {

avastd::shared_ptr<Upvalue> VM::FindOrCreateUpvalue(CallFrame& frame, avastd::uint32_t reg_idx) {
    for (auto& existing : frame.open_upvalues) {
        if (existing->reg_index == static_cast<int>(reg_idx)) {
            return existing;
        }
    }
    auto upval = avastd::make_shared<Upvalue>();
    upval->location = &frame.registers[reg_idx];
    upval->reg_index = static_cast<int>(reg_idx);
    frame.open_upvalues.push_back(upval);
    return upval;
}

void VM::CloseUpvalues(CallFrame& frame) {
    for (auto& upval : frame.open_upvalues) {
        // Snapshot the live value before `location` (which points into
        // frame.registers, about to be destroyed) goes dangling, then
        // repoint at that snapshot so closures holding this Upvalue keep
        // working with self-contained storage from here on.
        upval->value = *upval->location;
        upval->location = &upval->value;
        upval->reg_index = -1;
    }
    frame.open_upvalues.clear();
}

void VM::RelocateUpvalues(CallFrame& from, CallFrame& to) {
    for (auto& upval : from.open_upvalues) {
        if (upval->reg_index >= 0 &&
            static_cast<size_t>(upval->reg_index) < to.registers.size()) {
            upval->location = &to.registers[static_cast<size_t>(upval->reg_index)];
        }
    }
    to.open_upvalues = avastd::move(from.open_upvalues);
    from.open_upvalues.clear();
}

Value VM::ExecuteFrame(size_t frame_idx) {
    auto& code = frames_[frame_idx].proto->instructions;
    auto& K = frames_[frame_idx].proto->constants;

    bool need_restart = false;

    auto handle_vm_exception = [&](bool is_raise, const avastd::exception& e) -> bool {
        if (!is_raise) {
            HandleFrameError(*this, frame_idx, e);
        }
        if (exception_handlers_.empty()) return false;
        auto handler = exception_handlers_.back();
        if (handler.frame_idx != frame_idx) return false;

        if (!is_raise) {
            Value msg;
            msg.type = ValueType::String;
            msg.obj = new StringObj(e.what());
            RaiseException(msg);
        }
        exception_handlers_.pop_back();
        if (frames_.size() > frame_idx + 1) {
            // Close upvalues opened by every frame this exception is about
            // to unwind past -- resize() below destroys their `registers`
            // buffers directly (no per-frame pop_back/CloseUpvalues call),
            // so any Upvalue still pointing into one would otherwise be
            // left dangling if a closure escaped from inside the try block
            // before the throw (e.g. stored into a list/global).
            for (size_t i = frame_idx + 1; i < frames_.size(); ++i) {
                CloseUpvalues(frames_[i]);
            }
            frames_.resize(frame_idx + 1);
        }
        frames_[frame_idx].pc = handler.catch_pc;
        need_restart = true;
        return true;
    };
    do {
    need_restart = false;
    AVA_TRY {
    if (frames_[frame_idx].pending_await_error) {
        frames_[frame_idx].pending_await_error = false;
        Value exc = frames_[frame_idx].pending_await_error_value;
        RaiseException(exc);
        AVA_THROW(AvaRaiseException());
    }
    while (frames_[frame_idx].pc < code.size()) {
        const Instr& in = code[frames_[frame_idx].pc++];
        switch (in.op) {
            case OpCode::LOADK:    
                frames_[frame_idx].registers[in.a] = K[in.b]; 
                break;
            case OpCode::LOADNIL:  
                frames_[frame_idx].registers[in.a] = Value::Nil(); 
                break;
            case OpCode::LOADBOOL: 
                frames_[frame_idx].registers[in.a] = Value::Bool(in.b != 0); 
                break;
            case OpCode::MOVE:     
                frames_[frame_idx].registers[in.a] = frames_[frame_idx].registers[in.b]; 
                break;

            case OpCode::GETGLOBAL: {
                avastd::string name = avastd::string(static_cast<StringObj*>(K[in.b].obj)->data);
                // Antes esto era `frames_[frame_idx].registers[in.a] =
                // GetGlobal(name);` sin chequear nada: un typo o un
                // identificador nunca declarado (ni funcion, ni variable
                // global) resolvia en silencio a Nil (ver
                // VM::GetGlobal), tanto si se leia como valor suelto
                // ("fdsf" solo, statement cuyo resultado se descarta)
                // como si se usaba en cualquier expresion -- ni el
                // compilador (CheckCallArgs solo mira callees de CallExpr,
                // ver compiler.cpp) ni la VM avisaban nada, ni en tiempo
                // de compilacion ni en runtime. Mismo mensaje/formato que
                // MakeNonCallableError ya usa para "'x' is not callable"
                // (incluyendo el hint de import si el nombre resulta ser
                // un simbolo de un modulo nativo no importado), para que
                // el usuario vea linea/columna reales en vez de un Nil
                // silencioso propagandose river abajo.
                if (!HasGlobal(name)) {
                    avastd::string module_path = FindNativeModuleExporting(name);
                    if (!module_path.empty()) {
                        AVA_THROW(MakeFrameError(frames_[frame_idx],
                            "'" + name + "' is not defined -- did you forget 'import " +
                            module_path + "'?"));
                    }
                    AVA_THROW(MakeFrameError(frames_[frame_idx], "'" + name + "' is not defined"));
                }
                frames_[frame_idx].registers[in.a] = GetGlobal(name);
                break;
            }
            case OpCode::SETGLOBAL: 
                SetGlobal(avastd::string(
                    static_cast<StringObj*>(K[in.b].obj)->data), frames_[frame_idx].registers[in.a]); 
                break;

            case OpCode::ADD:
                OpAdd(frames_[frame_idx], in, K, *this);
                break;
            case OpCode::SUB:
                OpSub(frames_[frame_idx], in, K, *this);
                break;
            case OpCode::MUL:
                OpMul(frames_[frame_idx], in, K, *this);
                break;
            case OpCode::DIV:
                OpDiv(frames_[frame_idx], in, K, *this);
                break;
            case OpCode::IDIV:
                OpIdiv(frames_[frame_idx], in, K, *this);
                break;
            case OpCode::MOD:
                OpMod(frames_[frame_idx], in, K, *this);
                break;
            case OpCode::POW:
                OpPow(frames_[frame_idx], in, K, *this);
                break;
            case OpCode::BAND:
                OpBand(frames_[frame_idx], in, K, *this);
                break;
            case OpCode::BOR:
                OpBor(frames_[frame_idx], in, K, *this);
                break;
            case OpCode::BXOR:
                OpBxor(frames_[frame_idx], in, K, *this);
                break;
            case OpCode::SHL:
                OpShl(frames_[frame_idx], in, K, *this);
                break;
            case OpCode::SHR:
                OpShr(frames_[frame_idx], in, K, *this);
                break;
            case OpCode::NEG:
                OpNeg(frames_[frame_idx], in, K, *this);
                break;
            case OpCode::NOT:
                OpNot(frames_[frame_idx], in, K, *this);
                break;
            case OpCode::BNOT:
                OpBnot(frames_[frame_idx], in, K, *this);
                break;
            case OpCode::INC:
                OpInc(frames_[frame_idx], in, K, *this);
                break;
            case OpCode::DEC:
                OpDec(frames_[frame_idx], in, K, *this);
                break;

            case OpCode::EQ:
                OpEq(frames_[frame_idx], in, K, *this);
                break;
            case OpCode::EQK:
                OpEqK(frames_[frame_idx], in, K, *this);
                break;
            case OpCode::NEK:
                OpNeK(frames_[frame_idx], in, K, *this);
                break;
            case OpCode::NE:
                OpNe(frames_[frame_idx], in, K, *this);
                break;
            case OpCode::LT:
                OpLt(frames_[frame_idx], in, K, *this);
                break;
            case OpCode::LE:
                OpLe(frames_[frame_idx], in, K, *this);
                break;
            case OpCode::GT:
                OpGt(frames_[frame_idx], in, K, *this);
                break;
            case OpCode::GE:
                OpGe(frames_[frame_idx], in, K, *this);
                break;

            case OpCode::JMP: 
                frames_[frame_idx].pc = static_cast<uint32_t>(
                    static_cast<int32_t>(frames_[frame_idx].pc) + in.bx32); 
                break;
            case OpCode::TEST:
                if (frames_[frame_idx].registers[in.a].IsTruthy() == (in.c == 0)) 
                    frames_[frame_idx].pc++;
                break;

            case OpCode::NEWLIST:
                OpNewList(frames_[frame_idx], in, K, *this);
                break;
            case OpCode::LISTAPPEND:
                OpListAppend(frames_[frame_idx], in, K, *this);
                break;
            case OpCode::NEWDICT:
                OpNewDict(frames_[frame_idx], in, K, *this);
                break;
            case OpCode::GETINDEX:
                OpGetIndex(frames_[frame_idx], in, K, *this);
                break;
            case OpCode::SETINDEX:
                OpSetIndex(frames_[frame_idx], in, K, *this);
                break;

            case OpCode::NEWCLASS:
                OpNewClass(frames_[frame_idx], in, K, *this);
                break;
            case OpCode::NEWINSTANCE:
                OpNewInstance(frames_[frame_idx], in, K, *this);
                break;
            case OpCode::GETATTR:
                OpGetAttr(frames_[frame_idx], in, K, *this);
                break;
            case OpCode::SETATTR:
                OpSetAttr(frames_[frame_idx], in, K, *this);
                break;

            case OpCode::CALL: {
                uint8_t save_a = in.a;
                avastd::vector<Value> args(
                    frames_[frame_idx].registers.begin() + in.a + 1,
                    frames_[frame_idx].registers.begin() + in.a + 1 + in.b);
                const Value& callee = frames_[frame_idx].registers[in.a];

                if (callee.type == ValueType::Bound) {
                    auto* bound = static_cast<BoundMethod*>(callee.obj);
                    if (bound->proto->is_async) {
                        // Bug #16: esta rama (CALL inline dentro de
                        // ExecuteFrame) es el unico despachador de CALL
                        // realmente usado en runtime -- OpCall en
                        // vm_call_op.cpp tiene el mismo chequeo pero es
                        // codigo muerto, nunca invocado. Sin este check,
                        // un metodo `async func` (via BoundMethod) caia
                        // siempre en la rama sincrona de abajo y devolvia
                        // el valor ya resuelto en vez de envolverlo en un
                        // Task, a diferencia de una funcion `async func`
                        // libre (ValueType::Function, ver mas abajo) que
                        // si pasaba por StartAsyncCall.
                        frames_[frame_idx].registers[save_a] = StartAsyncBoundCall(callee, args);
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
                    frames_.push_back(callee_frame);
                    Value result = ExecuteFrame(frames_.size() - 1);
                    
                    if (is_coroutine_suspended_) {
                        return result;
                    }
                    
                    CloseUpvalues(frames_.back());
                    frames_.pop_back();
                    frames_[frame_idx].registers[save_a] = result;
                    }
                } else if (callee.type == ValueType::Class) {
                    auto* cls = static_cast<ClassObj*>(callee.obj);
                    auto* inst = new InstanceObj();
                    inst->cls = cls;
                    inst->attrs = cls->instance_defaults;
                    
                    // Bug #13: `cls` es un objeto YA existente (viene de
                    // `callee`, no de un `new` recien hecho aca), asi que
                    // envolverlo con campos manuales (sin pasar por el
                    // constructor de copia) no retiene nada -- el
                    // ref_count=1 inicial de ClassObj ya esta "gastado" en
                    // el dueno original de `callee`. Copiar `callee`
                    // directamente retiene correctamente (Value::Value(const
                    // Value&)), evitando el Release() de mas que el
                    // destructor de una Value manual hubiera hecho al salir
                    // de este bloque.
                    Value cls_val = callee;
                    inst->attrs["__class__"] = cls_val;
                    
                    auto base_it = cls->attrs.find("__base__");
                    if (base_it != cls->attrs.end()) {
                        inst->attrs["__base__"] = base_it->second;
                    }
                    
                    Value v; v.type = ValueType::Instance; v.obj = inst;
                    frames_[frame_idx].registers[save_a] = v;
                    
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
                        // Bug #14: `cls` es la clase hoja que se está
                        // instanciando (ej. Puppy en `Puppy()`), dueña de
                        // este __init__ que va a correr. Sin esto,
                        // `base.__init__()` dentro de este método no tenía
                        // forma de saber desde qué nivel de la cadena
                        // arrancar a buscar el siguiente `__base__` y
                        // siempre usaba el de la instancia (fijo) -- ver
                        // OpBaseCall en vm_call_op.cpp para el resto de la
                        // cadena.
                        init_frame.base_lookup_class = cls;
                        frames_.push_back(init_frame);
                        ExecuteFrame(frames_.size() - 1);
                        
                        if (is_coroutine_suspended_) {
                            return v;
                        }
                        
                        CloseUpvalues(frames_.back());
                        frames_.pop_back();
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
                    
                    ava_value_t c_result = native->fn(
                        reinterpret_cast<AvaVM*>(this),
                        c_args.empty() ? nullptr : c_args.data(),
                        c_args.size(),
                        native->user_data
                    );
                    frames_[frame_idx].registers[save_a] = FromC(c_result);
                } else if (callee.type == ValueType::Function) {
                    auto* closure = static_cast<Closure*>(callee.obj);
                    if (closure->proto->is_async) {
                        frames_[frame_idx].registers[save_a] = StartAsyncCall(callee, args);
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
                        frames_.push_back(callee_frame);
                        Value result = ExecuteFrame(frames_.size() - 1);

                        if (is_coroutine_suspended_) {
                            return result;
                        }

                        CloseUpvalues(frames_.back());
                        frames_.pop_back();
                        frames_[frame_idx].registers[save_a] = result;
                    }
                } else {
                    AVA_THROW(MakeNonCallableError(*this, frames_[frame_idx], callee));
                }
                break;
            }

            case OpCode::RETURN:
                return in.b > 0 ? frames_[frame_idx].registers[in.a] : Value::Nil();

            case OpCode::CLOSURE: {
                auto& child_proto = frames_[frame_idx].proto->child_protos[in.b];
                auto* closure = new Closure();
                closure->proto = child_proto;
                for (size_t i = 0; i < child_proto->upvalue_descs.size(); ++i) {
                    auto& uvd = child_proto->upvalue_descs[i];
                    if (uvd.from_parent_local) {
                        // FindOrCreateUpvalue interns by register index, so a
                        // sibling closure created earlier in this same frame
                        // that captured this same local gets back that exact
                        // Upvalue -- not a fresh, disconnected one -- which is
                        // what lets two closures share mutations to it.
                        closure->upvalues.push_back(FindOrCreateUpvalue(frames_[frame_idx], uvd.index));
                    } else {
                        // Bug #3 (captura de closures a 2+ niveles de
                        // anidamiento): uvd.index acá NO es un registro de
                        // este frame, sino un índice dentro de las propias
                        // upvalues de la closure que está corriendo este
                        // frame (this frame's own closure), es decir "upvalue
                        // de upvalue" -- ver el nuevo loop de encadenamiento
                        // en compiler.cpp (CompileLambda/CompileFunctionDecl).
                        // Reusar el mismo shared_ptr<Upvalue> del padre (en
                        // vez de crear uno nuevo) es lo que hace que la
                        // closure hija comparta la MISMA caja que el padre
                        // ya comparte con el abuelo -- funciona sin importar
                        // si esa Upvalue del padre sigue abierta (todavía
                        // apunta a un registro vivo más arriba en la pila) o
                        // ya fue cerrada (snapshot autocontenido), porque en
                        // ambos casos location/value ya están resueltos
                        // correctamente dentro del propio Upvalue.
                        auto* parent_closure = frames_[frame_idx].closure.get();
                        if (parent_closure && uvd.index < parent_closure->upvalues.size()) {
                            closure->upvalues.push_back(parent_closure->upvalues[uvd.index]);
                        } else {
                            // No debería pasar (el compilador solo emite
                            // índices válidos), pero evita un puntero nulo /
                            // out-of-bounds en vez de crashear si algún .avac
                            // corrupto o desincronizado llega hasta acá.
                            closure->upvalues.push_back(avastd::make_shared<Upvalue>());
                        }
                    }
                }
                Value v; v.type = ValueType::Function; v.obj = closure;
                frames_[frame_idx].registers[in.a] = v;
                break;
            }

            case OpCode::GETUPVAL: {
                auto* closure = frames_[frame_idx].closure.get();
                if (closure && in.b < closure->upvalues.size()) {
                    // Read through `location`, not the `value` snapshot: while
                    // the upvalue is open, `location` points at the live
                    // register in the frame that created it, so this sees
                    // writes made after the closure was created (including by
                    // that frame's own code, e.g. a self-referential
                    // `fib = (n) => ... fib(n-1) ...` reassignment).
                    frames_[frame_idx].registers[in.a] = *closure->upvalues[in.b]->location;
                } else {
                    frames_[frame_idx].registers[in.a] = Value::Nil();
                }
                break;
            }

            case OpCode::SETUPVAL: {
                auto* closure = frames_[frame_idx].closure.get();
                if (closure && in.b < closure->upvalues.size()) {
                    // Write through `location` instead of overwriting it: the
                    // old code pointed `location` at this Upvalue's own
                    // `value` field on every write, which permanently
                    // severed the shared box on the first assignment -- any
                    // sibling closure (or the parent frame) still reading
                    // through the original register, or through their own
                    // copy of this Upvalue, would stop seeing further
                    // changes. Leave `location` alone; it only gets
                    // repointed once, in CloseUpvalues, when the owning
                    // frame is popped.
                    *closure->upvalues[in.b]->location = frames_[frame_idx].registers[in.a];
                }
                break;
            }

            case OpCode::BASECALL: {
                Value base_result = OpBaseCall(frames_[frame_idx], in, K, *this);
                if (is_coroutine_suspended_) {
                    return base_result;
                }
                break;
            }

            case OpCode::SLICE:
                OpSlice(frames_[frame_idx], in, K, *this);
                break;

            case OpCode::TRY:
                OpTry(frames_[frame_idx], in, K, *this);
                break;

            case OpCode::TRY_END:
                OpTryEnd(frames_[frame_idx], in, K, *this);
                break;

            case OpCode::CATCH:
                OpCatch(frames_[frame_idx], in, K, *this);
                break;

            case OpCode::RAISE:
                OpRaise(frames_[frame_idx], in, K, *this);
                break;

            case OpCode::ARGC:
                OpArgc(frames_[frame_idx], in, K, *this);
                break;

            case OpCode::YIELD:
                OpYield(frames_[frame_idx], in, K, *this);
                return frames_[frame_idx].registers[in.a];

            case OpCode::RESUME:
                OpResume(frames_[frame_idx], in, K, *this);
                break;

            case OpCode::AWAIT:
                OpAwait(frames_[frame_idx], in, K, *this);
                if (is_coroutine_suspended_) {
                    return frames_[frame_idx].registers[in.a];
                }
                break;

            default:
                AVA_THROW(MakeFrameError(frames_[frame_idx],
                    "unknown opcode: " + avastd::to_string(static_cast<int>(in.op))));
        }
    }
#if AVA_HAVE_EXCEPTIONS
    } catch (const AvaRaiseException& e) {
        if (!handle_vm_exception(/*is_raise=*/true, e)) throw;
    } catch (const avastd::exception& e) {
        if (!handle_vm_exception(/*is_raise=*/false, e)) throw;
    }
#else
    } AVA_CATCH(avastd::exception, e) {
        bool is_raise = (e.ava_type_tag() == 1);  // ver AvaRaiseException::ava_type_tag()
        if (!handle_vm_exception(is_raise, e)) AVA_RETHROW();
    }
#endif
    } while (need_restart);

    return Value::Nil();
}

} // namespace ava