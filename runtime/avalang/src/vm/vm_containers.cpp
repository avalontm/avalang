#include "vm.h"
#include "vm_internal.h"

namespace ava {

void OpNewList(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm) {
    auto* list = new ListObj();
    Value v; v.type = ValueType::List; v.obj = list;
    frame.registers[in.a] = v;
}

void OpListAppend(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm) {
    static_cast<ListObj*>(frame.registers[in.a].obj)->items.push_back(
        frame.registers[in.b]);
}

void OpNewDict(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm) {
    auto* dict = new DictObj();
    Value v; v.type = ValueType::Dict; v.obj = dict;
    frame.registers[in.a] = v;
}

void OpGetIndex(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm) {
    auto& obj = frame.registers[in.b];
    auto& idx = frame.registers[in.c];
    if (obj.type == ValueType::List) {
        if (idx.type != ValueType::Number) {
            frame.registers[in.a] = Value::Nil();
            return;
        }
        auto* list = static_cast<ListObj*>(obj.obj);
        size_t pos = ValidateIntegerIndex(idx.n, "list index");
        if (pos < list->items.size()) {
            frame.registers[in.a] = list->items[pos];
        } else {
            frame.registers[in.a] = Value::Nil();
        }
    } else if (obj.type == ValueType::Dict) {
        auto* dict = static_cast<DictObj*>(obj.obj);
        if (idx.type == ValueType::Number) {
            size_t pos = ValidateIntegerIndex(idx.n, "dict index");
            if (pos < dict->entries.size()) {
                auto sv = Value(); sv.type = ValueType::String; sv.obj = new StringObj(dict->entries[pos].first);
                frame.registers[in.a] = sv;
            } else {
                frame.registers[in.a] = Value::Nil();
            }
        } else if (idx.type == ValueType::String) {
            auto* key = static_cast<StringObj*>(idx.obj);
            auto it = dict->index.find(key->data);
            if (it != dict->index.end()) {
                frame.registers[in.a] = dict->entries[it->second].second;
            } else {
                frame.registers[in.a] = Value::Nil();
            }
        } else {
            frame.registers[in.a] = Value::Nil();
        }
    } else if (obj.type == ValueType::String) {
        if (idx.type == ValueType::Number) {
            auto* str = static_cast<StringObj*>(obj.obj);
            size_t pos = ValidateIntegerIndex(idx.n, "string index");
            if (pos < str->data.size()) {
                Value sv; sv.type = ValueType::String; sv.obj = new StringObj(std::string(1, str->data[pos]));
                frame.registers[in.a] = sv;
            } else {
                frame.registers[in.a] = Value::Nil();
            }
        } else {
            frame.registers[in.a] = Value::Nil();
        }
    } else {
        frame.registers[in.a] = Value::Nil();
    }
}

void OpSetIndex(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm) {
    auto& obj = frame.registers[in.a];
    auto& idx = frame.registers[in.b];
    auto& val = frame.registers[in.c];
    if (obj.type == ValueType::List) {
        if (idx.type != ValueType::Number) {
            return;
        }
        auto* list = static_cast<ListObj*>(obj.obj);
        size_t pos = ValidateIntegerIndex(idx.n, "list index");
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
}

} // namespace ava