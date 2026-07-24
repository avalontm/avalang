#include "vm.h"
#include "module.h"
#include "coroutine.h"
#include "../frontend/frontend.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <iomanip>
#include <cmath>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define PATH_SEPARATOR "\\"
#define PATH_SEPARATOR_CHAR '\\'
#define GET_CURRENT_DIR _getcwd
#else
#include <unistd.h>
#include <sys/stat.h>
#define PATH_SEPARATOR "/"
#define PATH_SEPARATOR_CHAR '/'
#define GET_CURRENT_DIR getcwd
#endif

namespace {

static std::string GetFileDir(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    if (pos == std::string::npos) return ".";
    return path.substr(0, pos);
}

static std::string NumberToString(double n) {
    if (std::abs(n - std::round(n)) < 0.0000001)
        return std::to_string(static_cast<long long>(std::round(n)));
    std::ostringstream oss;
    oss << std::setprecision(15) << n;
    std::string s = oss.str();
    size_t dot = s.find('.');
    if (dot != std::string::npos) {
        size_t last_not_zero = s.find_last_not_of('0');
        if (last_not_zero == dot)
            s.erase(dot);
        else
            s.erase(last_not_zero + 1);
    }
    return s;
}

static std::string JoinPath(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    char sep = PATH_SEPARATOR_CHAR;
    if (a.back() == sep || a.back() == '/') return a + b;
    return a + PATH_SEPARATOR + b;
}

#ifdef _WIN32
static bool FileExists(const std::string& path) {
    DWORD attrs = GetFileAttributesA(path.c_str());
    return (attrs != INVALID_FILE_ATTRIBUTES);
}
#else
static bool FileExists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}
#endif

static std::string GetCurrentWorkingDir() {
    char buff[4096];
    GET_CURRENT_DIR(buff, sizeof(buff));
    return std::string(buff);
}

}

namespace ava {

VM::VM() = default;

VM::~VM() {
    for (auto& kv : globals_) {
        Release(kv.second);
    }
    for (auto* co : created_coroutines_) {
        delete co;
    }
}

void VM::RegisterNative(const std::string& name, AvaNativeFn fn, void* user_data) {
    auto* native = new NativeObj();
    native->fn = fn;
    native->user_data = user_data;
    Value v;
    v.type = ValueType::Native;
    v.obj = native;
    SetGlobal(name, v);
}

void VM::RegisterBuiltinMethod(const std::string& name, AvaNativeFn fn, void* user_data) {
    builtin_methods_[name] = {fn, user_data};
}

bool VM::HasBuiltinMethod(const std::string& name) const {
    return builtin_methods_.find(name) != builtin_methods_.end();
}

Value VM::GetBuiltinMethod(const std::string& name) const {
    auto it = builtin_methods_.find(name);
    if (it == builtin_methods_.end()) return Value::Nil();
    
    auto* native = new NativeObj();
    native->fn = it->second.first;
    native->user_data = it->second.second;
    Value v;
    v.type = ValueType::Native;
    v.obj = native;
    return v;
}

Value VM::GetGlobal(const std::string& name) const {
    auto it = globals_.find(name);
    if (it == globals_.end()) {
        return Value::Nil();
    }
    return it->second;
}

void VM::SetGlobal(const std::string& name, Value value) {
    Retain(value);
    auto it = globals_.find(name);
    if (it != globals_.end()) {
        Release(it->second);
        it->second = value;
    } else {
        globals_.emplace(name, value);
    }
}

Value VM::Run(const std::shared_ptr<Proto>& main) {
    CallFrame frame;
    frame.proto = main;
    frame.registers.resize(main->num_registers);
    frames_.push_back(std::move(frame));
    Value result = ExecuteFrame(0);
    frames_.pop_back();
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
        frames_.push_back(frame);
        Value result = ExecuteFrame(frames_.size() - 1);
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
        frames_.push_back(frame);
        Value result = ExecuteFrame(frames_.size() - 1);
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
            frames_.push_back(resume_frame);
        } else {
            for (size_t i = 0; i < args.size() && i < frames_[0].registers.size(); ++i) {
                frames_[0].registers[i] = args[i];
            }
        }

        is_coroutine_suspended_ = false;
        Value result = ExecuteFrame(0);

        current_coroutine_ = coroutine_resumers_.back();
        coroutine_resumers_.pop_back();

        co->frames = frames_;
        co->status = is_coroutine_suspended_ ? CoStatus::Suspended : CoStatus::Dead;
        frames_ = saved_frames_;

        return result;
    }

    throw std::runtime_error("attempt to call a non-callable value");
}

Value VM::ExecuteFrame(size_t frame_idx) {
    auto& code = frames_[frame_idx].proto->instructions;
    auto& K = frames_[frame_idx].proto->constants;

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

            case OpCode::ADD: {
                auto& Ra = frames_[frame_idx].registers[in.a];
                auto& Rb = frames_[frame_idx].registers[in.b];
                auto& Rc = frames_[frame_idx].registers[in.c];
                if (Rb.type == ValueType::String || Rc.type == ValueType::String) {
                    std::string s1 = Rb.type == ValueType::String 
                        ? static_cast<StringObj*>(Rb.obj)->data : NumberToString(Rb.n);
                    std::string s2 = Rc.type == ValueType::String 
                        ? static_cast<StringObj*>(Rc.obj)->data : NumberToString(Rc.n);
                    Value v; v.type = ValueType::String; v.obj = new StringObj(s1 + s2);
                    Ra = v;
                } else if (Rb.type == ValueType::List && Rc.type == ValueType::List) {
                    auto* list1 = static_cast<ListObj*>(Rb.obj);
                    auto* list2 = static_cast<ListObj*>(Rc.obj);
                    auto* result = new ListObj();
                    result->items.reserve(list1->items.size() + list2->items.size());
                    for (auto& item : list1->items) {
                        Retain(item);
                        result->items.push_back(item);
                    }
                    for (auto& item : list2->items) {
                        Retain(item);
                        result->items.push_back(item);
                    }
                    Value v; v.type = ValueType::List; v.obj = result;
                    Ra = v;
                } else {
                    Ra = Value::Number(Rb.n + Rc.n);
                }
                break;
            }
            case OpCode::SUB: 
                frames_[frame_idx].registers[in.a] = Value::Number(
                    frames_[frame_idx].registers[in.b].n - frames_[frame_idx].registers[in.c].n); 
                break;
            case OpCode::MUL: 
                frames_[frame_idx].registers[in.a] = Value::Number(
                    frames_[frame_idx].registers[in.b].n * frames_[frame_idx].registers[in.c].n); 
                break;
            case OpCode::DIV: 
                frames_[frame_idx].registers[in.a] = Value::Number(
                    frames_[frame_idx].registers[in.b].n / frames_[frame_idx].registers[in.c].n); 
                break;
            case OpCode::MOD: 
                frames_[frame_idx].registers[in.a] = Value::Number(
                    std::fmod(frames_[frame_idx].registers[in.b].n, frames_[frame_idx].registers[in.c].n)); 
                break;
            case OpCode::POW: 
                frames_[frame_idx].registers[in.a] = Value::Number(
                    std::pow(frames_[frame_idx].registers[in.b].n, frames_[frame_idx].registers[in.c].n)); 
                break;
            case OpCode::NEG: 
                frames_[frame_idx].registers[in.a] = Value::Number(
                    -frames_[frame_idx].registers[in.b].n); 
                break;
            case OpCode::NOT: 
                frames_[frame_idx].registers[in.a] = Value::Bool(
                    !frames_[frame_idx].registers[in.b].IsTruthy()); 
                break;
            case OpCode::INC:
                frames_[frame_idx].registers[in.a] = Value::Number(
                    frames_[frame_idx].registers[in.b].n + 1);
                break;
            case OpCode::DEC:
                frames_[frame_idx].registers[in.a] = Value::Number(
                    frames_[frame_idx].registers[in.b].n - 1);
                break;

            case OpCode::EQ: {
                auto& Ra = frames_[frame_idx].registers[in.a];
                auto& Rb = frames_[frame_idx].registers[in.b];
                auto& Rc = frames_[frame_idx].registers[in.c];
                if (Rb.type == ValueType::String && Rc.type == ValueType::String) {
                    auto* s1 = static_cast<StringObj*>(Rb.obj);
                    auto* s2 = static_cast<StringObj*>(Rc.obj);
                    Ra = Value::Bool(s1->data == s2->data);
                } else if (Rb.type == ValueType::Number && Rc.type == ValueType::Number) {
                    Ra = Value::Bool(Rb.n == Rc.n);
                } else if (Rb.type == ValueType::Bool && Rc.type == ValueType::Bool) {
                    Ra = Value::Bool(Rb.b == Rc.b);
                } else if (Rb.type == ValueType::Nil && Rc.type == ValueType::Nil) {
                    Ra = Value::Bool(true);
                } else {
                    Ra = Value::Bool(Rb.type == Rc.type && Rb.n == Rc.n);
                }
                break;
            }
            case OpCode::EQK: {
                auto& Ra = frames_[frame_idx].registers[in.a];
                auto& Rb = frames_[frame_idx].registers[in.b];
                const Value& Kc = K[in.c];
                if (Rb.type == ValueType::String && Kc.type == ValueType::String) {
                    auto* s1 = static_cast<StringObj*>(Rb.obj);
                    auto* s2 = static_cast<StringObj*>(Kc.obj);
                    Ra = Value::Bool(s1->data == s2->data);
                } else if (Rb.type == ValueType::Number && Kc.type == ValueType::Number) {
                    Ra = Value::Bool(Rb.n == Kc.n);
                } else if (Rb.type == ValueType::Bool && Kc.type == ValueType::Bool) {
                    Ra = Value::Bool(Rb.b == Kc.b);
                } else if (Rb.type == ValueType::Nil && Kc.type == ValueType::Nil) {
                    Ra = Value::Bool(true);
                } else {
                    Ra = Value::Bool(Rb.type == Kc.type && Rb.n == Kc.n);
                }
                break;
            }
            case OpCode::NEK: {
                auto& Ra = frames_[frame_idx].registers[in.a];
                auto& Rb = frames_[frame_idx].registers[in.b];
                const Value& Kc = K[in.c];
                if (Rb.type == ValueType::Bool && Kc.type == ValueType::Number && Kc.n == 0) {
                    Ra = Value::Bool(!Rb.b);
                } else if (Rb.type == ValueType::Number && Kc.type == ValueType::Number) {
                    Ra = Value::Bool(Rb.n != Kc.n);
                } else if (Rb.type == ValueType::Bool && Kc.type == ValueType::Bool) {
                    Ra = Value::Bool(Rb.b != Kc.b);
                } else if (Rb.type == ValueType::Nil && Kc.type == ValueType::Nil) {
                    Ra = Value::Bool(false);
                } else if (Rb.type == ValueType::Nil && Kc.type == ValueType::Number && Kc.n == 0) {
                    Ra = Value::Bool(true);
                } else {
                    Ra = Value::Bool(!(Rb.type == Kc.type && Rb.n == Kc.n));
                }
                break;
            }
            case OpCode::LT: {
                auto& Rb = frames_[frame_idx].registers[in.b];
                auto& Rc = frames_[frame_idx].registers[in.c];
                if (Rb.type == ValueType::String && Rc.type == ValueType::String) {
                    auto* s1 = static_cast<StringObj*>(Rb.obj);
                    auto* s2 = static_cast<StringObj*>(Rc.obj);
                    frames_[frame_idx].registers[in.a] = Value::Bool(s1->data < s2->data);
                } else {
                    frames_[frame_idx].registers[in.a] = Value::Bool(Rb.n < Rc.n);
                }
                break;
            }
            case OpCode::LE: {
                auto& Rb = frames_[frame_idx].registers[in.b];
                auto& Rc = frames_[frame_idx].registers[in.c];
                if (Rb.type == ValueType::String && Rc.type == ValueType::String) {
                    auto* s1 = static_cast<StringObj*>(Rb.obj);
                    auto* s2 = static_cast<StringObj*>(Rc.obj);
                    frames_[frame_idx].registers[in.a] = Value::Bool(s1->data <= s2->data);
                } else {
                    frames_[frame_idx].registers[in.a] = Value::Bool(Rb.n <= Rc.n);
                }
                break;
            }

            case OpCode::JMP: 
                frames_[frame_idx].pc = static_cast<uint32_t>(
                    static_cast<int32_t>(frames_[frame_idx].pc) + static_cast<int16_t>(in.b)); 
                break;
            case OpCode::TEST:
                if (frames_[frame_idx].registers[in.a].IsTruthy() == (in.c == 0)) 
                    frames_[frame_idx].pc++;
                break;

            case OpCode::NE: {
                auto& Ra = frames_[frame_idx].registers[in.a];
                auto& Rb = frames_[frame_idx].registers[in.b];
                auto& Rc = frames_[frame_idx].registers[in.c];
                if (Rb.type == ValueType::String && Rc.type == ValueType::String) {
                    auto* s1 = static_cast<StringObj*>(Rb.obj);
                    auto* s2 = static_cast<StringObj*>(Rc.obj);
                    Ra = Value::Bool(s1->data != s2->data);
                } else if (Rb.type == ValueType::Number && Rc.type == ValueType::Number) {
                    Ra = Value::Bool(Rb.n != Rc.n);
                } else if (Rb.type == ValueType::Bool && Rc.type == ValueType::Bool) {
                    Ra = Value::Bool(Rb.b != Rc.b);
                } else if (Rb.type == ValueType::Nil && Rc.type == ValueType::Nil) {
                    Ra = Value::Bool(false);
                } else {
                    Ra = Value::Bool(!(Rb.type == Rc.type && Rb.n == Rc.n));
                }
                break;
            }
            case OpCode::GT: {
                auto& Rb = frames_[frame_idx].registers[in.b];
                auto& Rc = frames_[frame_idx].registers[in.c];
                if (Rb.type == ValueType::String && Rc.type == ValueType::String) {
                    auto* s1 = static_cast<StringObj*>(Rb.obj);
                    auto* s2 = static_cast<StringObj*>(Rc.obj);
                    frames_[frame_idx].registers[in.a] = Value::Bool(s1->data > s2->data);
                } else {
                    frames_[frame_idx].registers[in.a] = Value::Bool(Rb.n > Rc.n);
                }
                break;
            }
            case OpCode::GE: {
                auto& Rb = frames_[frame_idx].registers[in.b];
                auto& Rc = frames_[frame_idx].registers[in.c];
                if (Rb.type == ValueType::String && Rc.type == ValueType::String) {
                    auto* s1 = static_cast<StringObj*>(Rb.obj);
                    auto* s2 = static_cast<StringObj*>(Rc.obj);
                    frames_[frame_idx].registers[in.a] = Value::Bool(s1->data >= s2->data);
                } else {
                    frames_[frame_idx].registers[in.a] = Value::Bool(Rb.n >= Rc.n);
                }
                break;
            }

            case OpCode::NEWLIST: {
                auto* list = new ListObj();
                Value v; v.type = ValueType::List; v.obj = list;
                frames_[frame_idx].registers[in.a] = v;
                break;
            }
            case OpCode::LISTAPPEND: {
                static_cast<ListObj*>(frames_[frame_idx].registers[in.a].obj)->items.push_back(
                    frames_[frame_idx].registers[in.b]);
                break;
            }

            case OpCode::NEWDICT: {
                auto* dict = new DictObj();
                Value v; v.type = ValueType::Dict; v.obj = dict;
                frames_[frame_idx].registers[in.a] = v;
                break;
            }

            case OpCode::GETINDEX: {
                auto& obj = frames_[frame_idx].registers[in.b];
                auto& idx = frames_[frame_idx].registers[in.c];
                if (obj.type == ValueType::List) {
                    auto* list = static_cast<ListObj*>(obj.obj);
                    size_t pos = static_cast<size_t>(idx.n);
                    if (pos < list->items.size()) {
                        frames_[frame_idx].registers[in.a] = list->items[pos];
                    } else {
                        frames_[frame_idx].registers[in.a] = Value::Nil();
                    }
                } else if (obj.type == ValueType::Dict) {
                    auto* dict = static_cast<DictObj*>(obj.obj);
                    if (idx.type == ValueType::Number) {
                        size_t pos = static_cast<size_t>(idx.n);
                        if (pos < dict->entries.size()) {
                            frames_[frame_idx].registers[in.a] = dict->entries[pos].second;
                        } else {
                            frames_[frame_idx].registers[in.a] = Value::Nil();
                        }
                    } else if (idx.type == ValueType::String) {
                        auto* key = static_cast<StringObj*>(idx.obj);
                        auto it = dict->index.find(key->data);
                        if (it != dict->index.end()) {
                            frames_[frame_idx].registers[in.a] = dict->entries[it->second].second;
                        } else {
                            frames_[frame_idx].registers[in.a] = Value::Nil();
                        }
                    } else {
                        frames_[frame_idx].registers[in.a] = Value::Nil();
                    }
                } else {
                    frames_[frame_idx].registers[in.a] = Value::Nil();
                }
                break;
            }

            case OpCode::SETINDEX: {
                auto& obj = frames_[frame_idx].registers[in.a];
                auto& idx = frames_[frame_idx].registers[in.b];
                auto& val = frames_[frame_idx].registers[in.c];
                if (obj.type == ValueType::List) {
                    auto* list = static_cast<ListObj*>(obj.obj);
                    size_t pos = static_cast<size_t>(idx.n);
                    if (pos < list->items.size()) {
                        list->items[pos] = val;
                    }
                } else if (obj.type == ValueType::Dict) {
                    auto* dict = static_cast<DictObj*>(obj.obj);
                    if (idx.type == ValueType::String) {
                        auto* key = static_cast<StringObj*>(idx.obj);
                        auto it = dict->index.find(key->data);
                        if (it != dict->index.end()) {
                            dict->entries[it->second].second = val;
                        } else {
                            size_t pos = dict->entries.size();
                            dict->entries.push_back({key->data, val});
                            dict->index[key->data] = pos;
                        }
                    }
                }
                break;
            }

            case OpCode::NEWCLASS: {
                auto* cls = new ClassObj();
                auto* class_proto = static_cast<ClassObj*>(K[in.b].obj);
                cls->name = class_proto->name;
                cls->attrs = class_proto->attrs;
                cls->methods = class_proto->methods;
                cls->param_names = class_proto->param_names;
                Value v; v.type = ValueType::Class; v.obj = cls;
                Retain(v);
                frames_[frame_idx].registers[in.a] = v;
                break;
            }

            case OpCode::NEWINSTANCE: {
                auto& cls_val = frames_[frame_idx].registers[in.b];
                auto* cls = static_cast<ClassObj*>(cls_val.obj);
                auto* inst = new InstanceObj();
                inst->cls = cls;
                inst->attrs = cls->attrs;
                
                Value cls_val_copy;
                cls_val_copy.type = ValueType::Class;
                cls_val_copy.obj = cls;
                inst->attrs["__class__"] = cls_val_copy;
                
                auto base_it = cls->attrs.find("__base__");
                if (base_it != cls->attrs.end()) {
                    inst->attrs["__base__"] = base_it->second;
                }
                Value v; v.type = ValueType::Instance; v.obj = inst;
                Retain(v);
                frames_[frame_idx].registers[in.a] = v;
                break;
            }

            case OpCode::GETATTR: {
                auto& obj = frames_[frame_idx].registers[in.b];
                auto* attr_name = static_cast<StringObj*>(K[in.c].obj);
                
                if (obj.type == ValueType::String || obj.type == ValueType::List || obj.type == ValueType::Dict) {
                    std::string method_name = attr_name->data;
                    
                    std::string prefixed_name;
                    if (obj.type == ValueType::String) {
                        prefixed_name = "str_" + method_name;
                    } else if (obj.type == ValueType::List) {
                        prefixed_name = "list_" + method_name;
                    } else if (obj.type == ValueType::Dict) {
                        prefixed_name = "dict_" + method_name;
                    }
                    
                    if (HasBuiltinMethod(prefixed_name)) {
                        auto* native = new NativeObj();
                        native->fn = builtin_methods_[prefixed_name].first;
                        native->user_data = builtin_methods_[prefixed_name].second;
                        native->is_primitive_method = true;
                        native->primitive_this = ToC(obj);
                        Value v; v.type = ValueType::Native; v.obj = native;
                        Retain(v);
                        frames_[frame_idx].registers[in.a] = v;
                        break;
                    }
                }
                
                if (obj.type == ValueType::Dict) {
                    auto* dict = static_cast<DictObj*>(obj.obj);
                    auto it = dict->index.find(attr_name->data);
                    if (it != dict->index.end()) {
                        frames_[frame_idx].registers[in.a] = dict->entries[it->second].second;
                    } else {
                        frames_[frame_idx].registers[in.a] = Value::Nil();
                    }
                } else if (obj.type == ValueType::Instance) {
                    auto* inst = static_cast<InstanceObj*>(obj.obj);
                    
                    auto class_it = inst->attrs.find("__class__");
                    ClassObj* lookup_cls = inst->cls;
                    if (class_it != inst->attrs.end() && class_it->second.type == ValueType::Class) {
                        lookup_cls = static_cast<ClassObj*>(class_it->second.obj);
                    }
                    
                    auto it = inst->attrs.find(attr_name->data);
                    if (it != inst->attrs.end()) {
                        frames_[frame_idx].registers[in.a] = it->second;
                    } else {
                        auto method_it = lookup_cls->methods.find(attr_name->data);
                        if (method_it != lookup_cls->methods.end()) {
                            auto* bound = new BoundMethod();
                            bound->proto = method_it->second;
                            bound->instance = obj;
                            Retain(obj);
                            Value v; v.type = ValueType::Bound; v.obj = bound;
                            Retain(v);
                            frames_[frame_idx].registers[in.a] = v;
                        } else {
                            frames_[frame_idx].registers[in.a] = Value::Nil();
                        }
                    }
                } else if (obj.type == ValueType::Class) {
                    auto* cls = static_cast<ClassObj*>(obj.obj);
                    auto method_it = cls->methods.find(attr_name->data);
                    if (method_it != cls->methods.end()) {
                        auto* bound = new BoundMethod();
                        bound->proto = method_it->second;
                        bound->instance = Value::Nil();
                        Value v; v.type = ValueType::Bound; v.obj = bound;
                        Retain(v);
                        frames_[frame_idx].registers[in.a] = v;
                    } else {
                        auto it = cls->attrs.find(attr_name->data);
                        if (it != cls->attrs.end()) {
                            frames_[frame_idx].registers[in.a] = it->second;
                        } else {
                            frames_[frame_idx].registers[in.a] = Value::Nil();
                        }
                    }
                } else {
                    frames_[frame_idx].registers[in.a] = Value::Nil();
                }
                break;
            }

            case OpCode::SETATTR: {
                auto& obj = frames_[frame_idx].registers[in.a];
                auto* attr_name = static_cast<StringObj*>(K[in.b].obj);
                auto& val = frames_[frame_idx].registers[in.c];
                if (obj.type == ValueType::Instance) {
                    auto* inst = static_cast<InstanceObj*>(obj.obj);
                    auto it = inst->attrs.find(attr_name->data);
                    if (it != inst->attrs.end()) {
                        Release(it->second);
                        it->second = val;
                        Retain(val);
                    } else {
                        inst->attrs[attr_name->data] = val;
                        Retain(val);
                    }
                } else if (obj.type == ValueType::Class) {
                    auto* cls = static_cast<ClassObj*>(obj.obj);
                    cls->attrs[attr_name->data] = val;
                    Retain(val);
                }
                break;
            }

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
                    for (size_t i = 0; i < all_args.size() && i + 1 < callee_frame.registers.size(); ++i) {
                        callee_frame.registers[i + 1] = all_args[i];
                    }
                    frames_.push_back(callee_frame);
                    Value result = ExecuteFrame(frames_.size() - 1);
                    frames_.pop_back();
                    frames_[frame_idx].registers[save_a] = result;
                } else if (callee.type == ValueType::Class) {
                    auto* cls = static_cast<ClassObj*>(callee.obj);
                    auto* inst = new InstanceObj();
                    inst->cls = cls;
                    inst->attrs = cls->attrs;
                    
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
                        frames_.push_back(init_frame);
                        ExecuteFrame(frames_.size() - 1);
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
                    frames_.push_back(callee_frame);
                    Value result = ExecuteFrame(frames_.size() - 1);
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
                auto& this_val = frames_[frame_idx].registers[0];
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
                                // Args are always placed starting at register 1
                                // by the compiler (see BaseExpr codegen), never
                                // relative to in.a (which is just the base()
                                // expression's own result register).
                                args[i + 1] = frames_[frame_idx].registers[1 + i];
                            }
                            CallFrame callee_frame;
                            callee_frame.proto = method_it->second;
                            callee_frame.registers.resize(std::max<size_t>(method_it->second->num_registers, args.size()));
                            for (size_t i = 0; i < args.size() && i < callee_frame.registers.size(); ++i) {
                                callee_frame.registers[i] = args[i];
                            }
                            frames_.push_back(callee_frame);
                            Value result = ExecuteFrame(frames_.size() - 1);
                            frames_.pop_back();
                            // NOTE: in.a is the base() expression's own result
                            // register, NOT register 0 ("this"). Writing to
                            // registers[0] here previously clobbered `this`
                            // with the (always nil) return value of the base
                            // class's __init__, breaking every attribute
                            // assignment made after base(...) in a derived
                            // constructor.
                            frames_[frame_idx].registers[in.a] = result;
                            break;
                        }
                    }
                }
                throw std::runtime_error("base() call failed - __base__ not found");
            }

            case OpCode::SLICE: {
                auto& obj = frames_[frame_idx].registers[in.b];
                if (obj.type == ValueType::List) {
                    auto* list = static_cast<ListObj*>(obj.obj);
                    double start_val = frames_[frame_idx].registers[in.c].n;
                    double end_val = (in.a < frames_[frame_idx].registers.size()) 
                        ? frames_[frame_idx].registers[in.a].n : 0;
                    size_t len = list->items.size();
                    size_t start_idx = 0, end_idx = len;
                    int step = 1;
                    if (start_val < 0) start_idx = static_cast<size_t>((std::max)(0, static_cast<int>(len) + static_cast<int>(start_val)));
                    else start_idx = (std::min)(static_cast<size_t>(start_val), len);
                    if (end_val < 0) end_idx = static_cast<size_t>((std::max)(0, static_cast<int>(len) + static_cast<int>(end_val)));
                    else end_idx = (std::min)(static_cast<size_t>(end_val), len);
                    auto* result = new ListObj();
                    for (size_t i = start_idx; i < end_idx && i < len; i += step) {
                        result->items.push_back(list->items[i]);
                    }
                    Value v; v.type = ValueType::List; v.obj = result;
                    frames_[frame_idx].registers[in.a] = v;
                } else if (obj.type == ValueType::String) {
                    auto* str = static_cast<StringObj*>(obj.obj);
                    double start_val = frames_[frame_idx].registers[in.c].n;
                    double end_val = (in.a < frames_[frame_idx].registers.size()) 
                        ? frames_[frame_idx].registers[in.a].n : 0;
                    size_t len = str->data.size();
                    size_t start_idx = 0, end_idx = len;
                    int step = 1;
                    if (start_val < 0) start_idx = static_cast<size_t>((std::max)(0, static_cast<int>(len) + static_cast<int>(start_val)));
                    else start_idx = (std::min)(static_cast<size_t>(start_val), len);
                    if (end_val < 0) end_idx = static_cast<size_t>((std::max)(0, static_cast<int>(len) + static_cast<int>(end_val)));
                    else end_idx = (std::min)(static_cast<size_t>(end_val), len);
                    std::string result = str->data.substr(start_idx, end_idx - start_idx);
                    Value v; v.type = ValueType::String; v.obj = new StringObj(result);
                    frames_[frame_idx].registers[in.a] = v;
                } else {
                    frames_[frame_idx].registers[in.a] = Value::Nil();
                }
                break;
            }

            case OpCode::TRY: {
                ExceptionHandler handler;
                handler.catch_pc = frames_[frame_idx].pc + static_cast<int16_t>(in.b);
                exception_handlers_.push_back(handler);
                break;
            }

            case OpCode::TRY_END: {
                if (!exception_handlers_.empty()) {
                    exception_handlers_.pop_back();
                }
                break;
            }

            case OpCode::CATCH: {
                if (HasException()) {
                    frames_[frame_idx].pc = static_cast<uint32_t>(
                        static_cast<int32_t>(frames_[frame_idx].pc) + static_cast<int16_t>(in.b));
                }
                break;
            }

            case OpCode::RAISE: {
                if (!exception_handlers_.empty()) {
                    auto& handler = exception_handlers_.back();
                    frames_[frame_idx].pc = static_cast<uint32_t>(handler.catch_pc);
                }
                break;
            }

            default:
                throw std::runtime_error("opcode not yet implemented: " + std::to_string(static_cast<int>(in.op)));

            case OpCode::YIELD: {
                if (coroutine_resumers_.empty()) {
                    throw std::runtime_error("attempt to yield outside of coroutine");
                }

                Value result = Value::Nil();
                if (in.b > 0 && in.a < frames_[frame_idx].registers.size()) {
                    std::vector<Value> yielded;
                    for (uint8_t i = 0; i < in.b && (in.a + i) < frames_[frame_idx].registers.size(); ++i) {
                        yielded.push_back(frames_[frame_idx].registers[in.a + i]);
                    }
                    auto* list = new ListObj();
                    list->items = std::move(yielded);
                    result.type = ValueType::List;
                    result.obj = list;
                }

                is_coroutine_suspended_ = true;
                return result;
            }

            case OpCode::RESUME: {
                auto& co_val = frames_[frame_idx].registers[in.b];
                if (co_val.type != ValueType::Coroutine) {
                    throw std::runtime_error("attempt to resume a non-coroutine");
                }
                auto* co = reinterpret_cast<Coroutine*>(co_val.obj);
                if (co->status == CoStatus::Dead) {
                    throw std::runtime_error("attempt to resume a dead coroutine");
                }
                if (co->status == CoStatus::Running) {
                    throw std::runtime_error("attempt to resume a running coroutine");
                }

                std::vector<Value> args(
                    frames_[frame_idx].registers.begin() + in.b + 1,
                    frames_[frame_idx].registers.begin() + in.b + 1 + in.c);

                co->status = CoStatus::Running;
                coroutine_resumers_.push_back(current_coroutine_);

                saved_frames_ = frames_;
                std::iter_swap(frames_.begin(), co->frames.begin());
                frames_ = co->frames;
                current_coroutine_ = co;

                if (frames_.empty() || !frames_[0].proto) {
                    auto* closure = static_cast<Closure*>(co->entry.obj);
                    CallFrame resume_frame;
                    resume_frame.proto = closure->proto;
                    resume_frame.closure = std::shared_ptr<Closure>(closure, [](Closure*) {});
                    resume_frame.registers.resize(closure->proto->num_registers);
                    frames_.push_back(resume_frame);
                }
                if (frames_[0].registers.size() < args.size()) {
                    frames_[0].registers.resize(args.size());
                }
                for (size_t i = 0; i < args.size() && i < frames_[0].registers.size(); ++i) {
                    frames_[0].registers[i] = args[i];
                }

                Value result = ExecuteFrame(0);

                current_coroutine_ = coroutine_resumers_.back();
                coroutine_resumers_.pop_back();

                co->frames = frames_;
                co->status = result.type == ValueType::Nil ? CoStatus::Suspended : CoStatus::Dead;
                frames_ = saved_frames_;

                frames_[frame_idx].registers[in.a] = result;
                break;
            }
        }
    }

    return Value::Nil();
}

std::string VM::GetCurrentDir() const {
    if (!current_dir_.empty()) return current_dir_;
    return GetCurrentWorkingDir();
}

void VM::SetCurrentDir(const std::string& dir) {
    current_dir_ = dir;
}

Value VM::RunFile(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        throw std::runtime_error("could not open file: " + file_path);
    }
    
    std::stringstream ss;
    ss << file.rdbuf();
    std::string source = ss.str();
    file.close();
    
    std::string dir = GetFileDir(file_path);
    std::string prev_dir = GetCurrentDir();
    SetCurrentDir(dir);
    
    current_module_ = file_path;
    
    try {
        auto proto = CompileSource(source, file_path);
        auto result = Run(proto);
        SetCurrentDir(prev_dir);
        return result;
    } catch (...) {
        SetCurrentDir(prev_dir);
        throw;
    }
}

static void SetNestedNamespace(
    std::unordered_map<std::string, Value>& globals,
    const std::vector<std::string>& parts,
    size_t part_idx,
    Value module_dict) {
    if (part_idx == parts.size() - 1) {
        Retain(module_dict);
        auto it = globals.find(parts[part_idx]);
        if (it != globals.end()) {
            Release(it->second);
            it->second = module_dict;
        } else {
            globals.emplace(parts[part_idx], module_dict);
        }
        Release(module_dict);
    } else {
        Value ns_val;
        ns_val.type = ValueType::Dict;
        ns_val.obj = new DictObj();
        Retain(ns_val);
        
        Value existing = globals.find(parts[part_idx]) != globals.end() 
            ? globals.at(parts[part_idx]) : Value::Nil();
        if (existing.type == ValueType::Dict) {
            auto* existing_dict = static_cast<DictObj*>(existing.obj);
            std::string next_key = parts[part_idx + 1];
            auto next_it = existing_dict->index.find(next_key);
            if (next_it != existing_dict->index.end()) {
                Release(existing_dict->entries[next_it->second].second);
                existing_dict->entries[next_it->second].second = ns_val;
            } else {
                existing_dict->index[next_key] = existing_dict->entries.size();
                existing_dict->entries.emplace_back(next_key, ns_val);
            }
            Retain(ns_val);
        } else {
            globals.emplace(parts[part_idx], ns_val);
        }
        
        SetNestedNamespace(globals, parts, part_idx + 1, module_dict);
        Release(ns_val);
    }
}

Value VM::DoImport(const std::string& module_path, const std::string& alias) {
    std::string current_dir = GetCurrentDir();
    
    std::string resolved_path = module_resolver_.ResolveModulePath(module_path, current_dir);
    if (resolved_path.empty()) {
        throw std::runtime_error("could not find module: " + module_path);
    }
    
    if (!module_cache_.Exists(module_path)) {
        module_cache_.BeginLoading(module_path);
        
        std::ifstream file(resolved_path);
        if (!file.is_open()) {
            module_cache_.EndLoading(module_path);
            throw std::runtime_error("could not open module file: " + resolved_path);
        }
        
        std::stringstream ss;
        ss << file.rdbuf();
        std::string source = ss.str();
        file.close();
        
        std::string prev_dir = GetCurrentDir();
        std::string prev_module = current_module_;
        SetCurrentDir(GetFileDir(resolved_path));
        current_module_ = resolved_path;
        
        try {
            auto proto = CompileSource(source, resolved_path);
            module_cache_.Add(module_path, proto, resolved_path);
            SetCurrentDir(prev_dir);
            current_module_ = prev_module;
            module_cache_.EndLoading(module_path);
        } catch (...) {
            SetCurrentDir(prev_dir);
            current_module_ = prev_module;
            module_cache_.EndLoading(module_path);
            module_cache_.Remove(module_path);
            throw;
        }
    }
    
    auto proto = module_cache_.Get(module_path);
    
    std::unordered_map<std::string, Value> outer_globals = std::move(globals_);
    globals_.clear();
    
    auto import_fn = outer_globals.find("__import__");
    if (import_fn != outer_globals.end()) {
        SetGlobal("__import__", import_fn->second);
    }
    
    CallFrame frame;
    frame.proto = proto;
    frame.registers.resize(proto->num_registers);
    frames_.push_back(std::move(frame));
    
    ExecuteFrame(frames_.size() - 1);
    
    frames_.pop_back();
    
    Value module_dict;
    module_dict.type = ValueType::Dict;
    module_dict.obj = new DictObj();
    Retain(module_dict);
    
    auto* dict = static_cast<DictObj*>(module_dict.obj);
    for (auto& entry : globals_) {
        if (entry.first == "__import__") continue;
        dict->index[entry.first] = dict->entries.size();
        dict->entries.emplace_back(entry.first, entry.second);
        Retain(entry.second);
    }
    
    if (alias.empty()) {
        std::vector<std::string> parts;
        std::string temp = module_path;
        size_t start = 0;
        while ((start = temp.find('.')) != std::string::npos) {
            parts.push_back(temp.substr(0, start));
            temp = temp.substr(start + 1);
        }
        parts.push_back(temp);
        
        if (parts.size() > 1) {
            std::vector<std::string> ns_parts(parts.begin(), parts.end() - 1);
            globals_ = std::move(outer_globals);
            SetNestedNamespace(globals_, ns_parts, 0, module_dict);
        } else {
            globals_ = std::move(outer_globals);
            SetGlobal(module_path, module_dict);
            Release(module_dict);
        }
    } else {
        globals_ = std::move(outer_globals);
        SetGlobal(alias, module_dict);
        Release(module_dict);
    }
    
    return Value::Nil();
}

Coroutine* VM::CreateCoroutine(const Value& func) {
    auto* co = new Coroutine();
    co->entry = func;
    co->status = CoStatus::Suspended;
    created_coroutines_.push_back(co);
    return co;
}

void VM::RaiseException(const Value& exc) {
    Retain(exc);
    Release(pending_exception_);
    pending_exception_ = exc;
}

Value VM::GetAndClearException() {
    Value exc = pending_exception_;
    pending_exception_ = Value::Nil();
    return exc;
}

bool VM::HasException() const {
    return pending_exception_.type != ValueType::Nil;
}

} // namespace ava