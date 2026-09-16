#include "vm.h"
#include "vm_internal.h"
#include "../../platform/barekernel/stdcompat/ava_stdcompat.h"

namespace ava {

Value VM::NewInstance(const Value& class_value, const avastd::vector<Value>& args) {
    if (class_value.type != ValueType::Class) {
        AVA_THROW(AvaError("cannot instantiate a value that is not a class"));
    }

    auto* cls = static_cast<ClassObj*>(class_value.obj);
    auto* inst = new InstanceObj();
    inst->cls = cls;
    inst->attrs = cls->instance_defaults;
    inst->attrs["__class__"] = class_value;

    auto base_it = cls->attrs.find("__base__");
    if (base_it != cls->attrs.end()) {
        inst->attrs["__base__"] = base_it->second;
    }

    Value instance_value;
    instance_value.type = ValueType::Instance;
    instance_value.obj = inst;

    auto init_it = cls->methods.find("__init__");
    if (init_it != cls->methods.end()) {
        CallFrame init_frame;
        init_frame.proto = init_it->second;
        init_frame.registers.resize(init_it->second->num_registers);
        init_frame.registers[0] = instance_value;
        for (size_t i = 0; i < args.size() && i + 1 < init_frame.registers.size(); ++i) {
            init_frame.registers[i + 1] = args[i];
        }
        init_frame.argc = static_cast<uint32_t>(args.size());
        init_frame.ret_slot = -1;
        init_frame.base_lookup_class = cls;
        frames_.push_back(init_frame);
        ExecuteFrame(frames_.size() - 1);
        if (is_coroutine_suspended_) {
            return instance_value;
        }
        CloseUpvalues(frames_.back());
        frames_.pop_back();
    }

    return instance_value;
}

Value VM::GetAttr(const Value& obj, const avastd::string& name) {
    if (obj.type == ValueType::Instance) {
        auto* inst = static_cast<InstanceObj*>(obj.obj);

        auto class_it = inst->attrs.find("__class__");
        ClassObj* lookup_cls = inst->cls;
        if (class_it != inst->attrs.end() && class_it->second.type == ValueType::Class) {
            lookup_cls = static_cast<ClassObj*>(class_it->second.obj);
        }

        auto it = inst->attrs.find(name);
        if (it != inst->attrs.end()) {
            return it->second;
        }

        auto method_it = lookup_cls->methods.find(name);
        if (method_it != lookup_cls->methods.end()) {
            auto* bound = new BoundMethod();
            bound->proto = method_it->second;
            bound->instance = obj;
            Value v;
            v.type = ValueType::Bound;
            v.obj = bound;
            return v;
        }

        auto* owner = FindClassOwningAttr(lookup_cls, name);
        if (owner) {
            return owner->attrs.at(name);
        }
        return Value::Nil();
    }

    if (obj.type == ValueType::Class) {
        auto* cls = static_cast<ClassObj*>(obj.obj);
        auto method_it = cls->methods.find(name);
        if (method_it != cls->methods.end()) {
            if (!cls->static_methods.count(name)) {
                AVA_THROW(AvaError("'" + name + "' is an instance method of class '" + cls->name +
                    "' -- '" + cls->name + "' has not been instantiated (use 'new " + cls->name +
                    "(...)' to create an instance first)"));
            }
            auto* bound = new BoundMethod();
            bound->proto = method_it->second;
            bound->instance = Value::Nil();
            Value v;
            v.type = ValueType::Bound;
            v.obj = bound;
            return v;
        }
        auto* owner = FindClassOwningAttr(cls, name);
        if (owner) {
            return owner->attrs.at(name);
        }
        if (cls->instance_defaults.count(name)) {
            AVA_THROW(AvaError("'" + name + "' is an instance member of class '" + cls->name +
                "' -- '" + cls->name + "' has not been instantiated (use 'new " + cls->name +
                "(...)' to create an instance first)"));
        }
        return Value::Nil();
    }

    if (obj.type == ValueType::Dict) {
        auto* dict = static_cast<DictObj*>(obj.obj);
        auto it = dict->index.find(name);
        if (it != dict->index.end()) {
            return dict->entries[it->second].second;
        }
        return Value::Nil();
    }

    return Value::Nil();
}

void VM::SetAttr(const Value& obj, const avastd::string& name, const Value& value) {
    if (obj.type == ValueType::Instance) {
        auto* inst = static_cast<InstanceObj*>(obj.obj);
        auto it = inst->attrs.find(name);
        if (it != inst->attrs.end()) {
            it->second = value;
            return;
        }

        auto class_it = inst->attrs.find("__class__");
        ClassObj* lookup_cls = inst->cls;
        if (class_it != inst->attrs.end() && class_it->second.type == ValueType::Class) {
            lookup_cls = static_cast<ClassObj*>(class_it->second.obj);
        }
        auto* owner = lookup_cls ? FindClassOwningAttr(lookup_cls, name) : nullptr;
        if (owner) {
            owner->attrs.at(name) = value;
        } else {
            inst->attrs[name] = value;
        }
        return;
    }

    if (obj.type == ValueType::Class) {
        auto* cls = static_cast<ClassObj*>(obj.obj);
        auto* owner = FindClassOwningAttr(cls, name);
        if (!owner && cls->instance_defaults.count(name)) {
            AVA_THROW(AvaError("'" + name + "' is an instance member of class '" + cls->name +
                "' -- '" + cls->name + "' has not been instantiated (use 'new " + cls->name +
                "(...)' to create an instance first)"));
        }
        auto& target_attrs = owner ? owner->attrs : cls->attrs;
        target_attrs[name] = value;
        return;
    }

    if (obj.type == ValueType::Dict) {
        auto* dict = static_cast<DictObj*>(obj.obj);
        auto it = dict->index.find(name);
        if (it != dict->index.end()) {
            dict->entries[it->second].second = value;
        } else {
            dict->index[name] = dict->entries.size();
            dict->entries.emplace_back(name, value);
        }
    }
}

Value VM::CallMethod(const Value& obj, const avastd::string& name, const avastd::vector<Value>& args) {
    Value method = GetAttr(obj, name);
    if (method.type != ValueType::Bound && method.type != ValueType::Function && method.type != ValueType::Native) {
        AVA_THROW(AvaError("'" + name + "' is not a callable method"));
    }
    return Call(method, args);
}

} // namespace ava
