#include "vm.h"
#include "vm_internal.h"

namespace ava {

void OpNewClass(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm) {
    auto* cls = new ClassObj();
    auto* class_proto = static_cast<ClassObj*>(K[in.b].obj);
    cls->name = class_proto->name;
    cls->attrs = class_proto->attrs;
    cls->instance_defaults = class_proto->instance_defaults;
    cls->methods = class_proto->methods;
    cls->private_members = class_proto->private_members;
    cls->param_names = class_proto->param_names;
    Value v; v.type = ValueType::Class; v.obj = cls;
    Retain(v);
    frame.registers[in.a] = v;
}

void OpNewInstance(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm) {
    auto& cls_val = frame.registers[in.b];
    auto* cls = static_cast<ClassObj*>(cls_val.obj);
    auto* inst = new InstanceObj();
    inst->cls = cls;
    inst->attrs = cls->instance_defaults;
    
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
    frame.registers[in.a] = v;
}

void OpGetAttr(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm) {
    auto& obj = frame.registers[in.b];
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
        
        if (vm.HasBuiltinMethod(prefixed_name)) {
            auto* native = new NativeObj();
            native->fn = vm.builtin_methods_[prefixed_name].first;
            native->user_data = vm.builtin_methods_[prefixed_name].second;
            native->is_primitive_method = true;
            native->primitive_this = ToC(obj);
            Value v; v.type = ValueType::Native; v.obj = native;
            Retain(v);
            frame.registers[in.a] = v;
            return;
        }
    }
    
    if (obj.type == ValueType::Dict) {
        auto* dict = static_cast<DictObj*>(obj.obj);
        auto it = dict->index.find(attr_name->data);
        if (it != dict->index.end()) {
            frame.registers[in.a] = dict->entries[it->second].second;
        } else {
            frame.registers[in.a] = Value::Nil();
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
            frame.registers[in.a] = it->second;
        } else {
            auto method_it = lookup_cls->methods.find(attr_name->data);
            if (method_it != lookup_cls->methods.end()) {
                auto* bound = new BoundMethod();
                bound->proto = method_it->second;
                bound->instance = obj;
                Retain(obj);
                Value v; v.type = ValueType::Bound; v.obj = bound;
                Retain(v);
                frame.registers[in.a] = v;
            } else {
                frame.registers[in.a] = Value::Nil();
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
            frame.registers[in.a] = v;
        } else {
            auto* owner = FindClassOwningAttr(cls, attr_name->data);
            if (owner) {
                frame.registers[in.a] = owner->attrs.at(attr_name->data);
            } else {
                frame.registers[in.a] = Value::Nil();
            }
        }
    } else if (obj.type == ValueType::Module) {
        // Namespace de un bloque `extern` (ver EXTERN_FFI_DESIGN.md / Fase 2).
        // No hay herencia ni "this": es solo un mapa nombre -> Native.
        auto* mod = static_cast<ModuleObj*>(obj.obj);
        auto it = mod->attrs.find(attr_name->data);
        if (it != mod->attrs.end()) {
            frame.registers[in.a] = it->second;
        } else {
            frame.registers[in.a] = Value::Nil();
        }
    } else {
        frame.registers[in.a] = Value::Nil();
    }
}

void OpSetAttr(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm) {
    auto& obj = frame.registers[in.a];
    auto* attr_name = static_cast<StringObj*>(K[in.b].obj);
    auto& val = frame.registers[in.c];
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
        auto* owner = FindClassOwningAttr(cls, attr_name->data);
        auto& target_attrs = owner ? owner->attrs : cls->attrs;
        auto it = target_attrs.find(attr_name->data);
        if (it != target_attrs.end()) {
            Release(it->second);
            it->second = val;
            Retain(val);
        } else {
            target_attrs[attr_name->data] = val;
            Retain(val);
        }
    }
}

} // namespace ava