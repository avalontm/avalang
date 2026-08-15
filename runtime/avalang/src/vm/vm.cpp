#include "vm.h"
#include "vm_internal.h"
#include "value.h"
#include <stdexcept>
#include <algorithm>

namespace ava {

Value VM::ExecuteFrame(size_t frame_idx) {
    auto& code = frames_[frame_idx].proto->instructions;
    auto& K = frames_[frame_idx].proto->constants;

    bool need_restart = false;
    do {
    need_restart = false;
    try {
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
                std::string name = std::string(static_cast<StringObj*>(K[in.b].obj)->data);
                frames_[frame_idx].registers[in.a] = GetGlobal(name); 
                break;
            }
            case OpCode::SETGLOBAL: 
                SetGlobal(std::string(
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
            case OpCode::NEG:
                OpNeg(frames_[frame_idx], in, K, *this);
                break;
            case OpCode::NOT:
                OpNot(frames_[frame_idx], in, K, *this);
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
                std::vector<Value> args(
                    frames_[frame_idx].registers.begin() + in.a + 1,
                    frames_[frame_idx].registers.begin() + in.a + 1 + in.b);
                const Value& callee = frames_[frame_idx].registers[in.a];

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
                    callee_frame.ret_slot = static_cast<int>(save_a);
                    frames_.push_back(callee_frame);
                    Value result = ExecuteFrame(frames_.size() - 1);
                    
                    if (is_coroutine_suspended_) {
                        return result;
                    }
                    
                    frames_.pop_back();
                    frames_[frame_idx].registers[save_a] = result;
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
                    frames_[frame_idx].registers[save_a] = v;
                    
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
                        init_frame.ret_slot = -1;
                        frames_.push_back(init_frame);
                        ExecuteFrame(frames_.size() - 1);
                        
                        if (is_coroutine_suspended_) {
                            return v;
                        }
                        
                        frames_.pop_back();
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
                        reinterpret_cast<AvaVM*>(this),
                        c_args.empty() ? nullptr : c_args.data(),
                        c_args.size(),
                        native->user_data
                    );
                    frames_[frame_idx].registers[save_a] = FromC(c_result);
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
                    frames_.push_back(callee_frame);
                    Value result = ExecuteFrame(frames_.size() - 1);
                    
                    if (is_coroutine_suspended_) {
                        return result;
                    }
                    
                    frames_.pop_back();
                    frames_[frame_idx].registers[save_a] = result;
                } else {
                    throw std::runtime_error("attempt to call a non-callable value");
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
                        auto upval = std::make_shared<Upvalue>();
                        upval->location = &frames_[frame_idx].registers[uvd.index];
                        upval->value = frames_[frame_idx].registers[uvd.index];
                        Retain(upval->value);
                        closure->upvalues.push_back(upval);
                    }
                }
                Value v; v.type = ValueType::Function; v.obj = closure;
                Retain(v);
                frames_[frame_idx].registers[in.a] = v;
                break;
            }

            case OpCode::GETUPVAL: {
                auto* closure = frames_[frame_idx].closure.get();
                if (closure && in.b < closure->upvalues.size()) {
                    frames_[frame_idx].registers[in.a] = closure->upvalues[in.b]->value;
                } else {
                    frames_[frame_idx].registers[in.a] = Value::Nil();
                }
                break;
            }

            case OpCode::SETUPVAL: {
                auto* closure = frames_[frame_idx].closure.get();
                if (closure && in.b < closure->upvalues.size()) {
                    Release(closure->upvalues[in.b]->value);
                    closure->upvalues[in.b]->value = frames_[frame_idx].registers[in.a];
                    closure->upvalues[in.b]->location = &closure->upvalues[in.b]->value;
                    Retain(closure->upvalues[in.b]->value);
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

            default:
                throw std::runtime_error("unknown opcode: " + std::to_string(static_cast<int>(in.op)));
        }
    }
    } catch (const AvaRaiseException& e) {
        if (!exception_handlers_.empty()) {
            auto handler = exception_handlers_.back();
            if (handler.frame_idx == frame_idx) {
                exception_handlers_.pop_back();
                // Calls that were aborted mid-propagation (OpCall/OpBaseCall/
                // constructor calls) never reached their normal-path
                // frames_.pop_back(), so any frames pushed above the frame
                // that's catching this exception are now stale garbage.
                // Trim them so frames_.size()-1 (used by OpTry to compute a
                // NEW handler's frame_idx) reflects reality again -- otherwise
                // a later try/catch registers a handler with an inflated
                // frame_idx that never matches, and its exceptions escape
                // uncaught.
                if (frames_.size() > frame_idx + 1) {
                    frames_.resize(frame_idx + 1);
                }
                frames_[frame_idx].pc = handler.catch_pc;
                need_restart = true;
            } else {
                throw;
            }
        } else {
            throw;
        }
    } catch (const std::exception& e) {
        HandleFrameError(*this, frame_idx, e);
        if (!exception_handlers_.empty()) {
            auto handler = exception_handlers_.back();
            if (handler.frame_idx == frame_idx) {
                Value msg;
                msg.type = ValueType::String;
                msg.obj = new StringObj(e.what());
                RaiseException(msg);
                exception_handlers_.pop_back();
                // Same stale-frame cleanup as above (see comment there).
                if (frames_.size() > frame_idx + 1) {
                    frames_.resize(frame_idx + 1);
                }
                frames_[frame_idx].pc = handler.catch_pc;
                need_restart = true;
            } else {
                throw;
            }
        } else {
            throw;
        }
    }
    } while (need_restart);

    return Value::Nil();
}

} // namespace ava