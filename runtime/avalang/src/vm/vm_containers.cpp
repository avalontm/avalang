#include "vm.h"
#include "vm_internal.h"
#include "vm_helpers.h"

namespace ava {

static double NormalizeIndex(double n, size_t len) {
    if (n < 0) {
        n += static_cast<double>(len);
    }
    return n;
}

void OpNewList(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm) {
    auto* list = new ListObj();
    Value v; v.type = ValueType::List; v.obj = list;
    frame.registers[in.a] = v;
}

void OpListAppend(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm) {
    static_cast<ListObj*>(frame.registers[in.a].obj)->items.push_back(
        frame.registers[in.b]);
}

void OpNewDict(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm) {
    auto* dict = new DictObj();
    Value v; v.type = ValueType::Dict; v.obj = dict;
    frame.registers[in.a] = v;
}

void OpGetIndex(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm) {
    auto& obj = frame.registers[in.b];
    auto& idx = frame.registers[in.c];
    if (obj.type == ValueType::List) {
        if (idx.type != ValueType::Number) {
            frame.registers[in.a] = Value::Nil();
            return;
        }
        auto* list = static_cast<ListObj*>(obj.obj);
        size_t pos = ValidateIntegerIndex(NormalizeIndex(idx.n, list->items.size()), list->items.size(), "list index");
        frame.registers[in.a] = list->items[pos];
    } else if (obj.type == ValueType::Dict) {
        avastd::string key_str;
        bool have_key = false;
        if (idx.type == ValueType::Number) {
            key_str = NumberToString(idx.n);
            have_key = true;
        } else if (idx.type == ValueType::String) {
            key_str = static_cast<StringObj*>(idx.obj)->data;
            have_key = true;
        }
        if (have_key) {
            auto* dict = static_cast<DictObj*>(obj.obj);
            auto it = dict->index.find(key_str);
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
            size_t pos = ValidateIntegerIndex(NormalizeIndex(idx.n, str->data.size()), str->data.size(), "string index");
            Value sv; sv.type = ValueType::String; sv.obj = new StringObj(avastd::string(1, str->data[pos]));
            frame.registers[in.a] = sv;
        } else {
            frame.registers[in.a] = Value::Nil();
        }
    } else {
        frame.registers[in.a] = Value::Nil();
    }
}

void OpSetIndex(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm) {
    auto& obj = frame.registers[in.a];
    auto& idx = frame.registers[in.b];
    auto& val = frame.registers[in.c];
    if (obj.type == ValueType::List) {
        if (idx.type != ValueType::Number) {
            return;
        }
        auto* list = static_cast<ListObj*>(obj.obj);
        size_t pos = ValidateIntegerIndex(NormalizeIndex(idx.n, list->items.size()), list->items.size(), "list index");
        list->items[pos] = val;
    } else if (obj.type == ValueType::Dict) {

        avastd::string key_str;
        bool have_key = false;
        if (idx.type == ValueType::String) {
            key_str = static_cast<StringObj*>(idx.obj)->data;
            have_key = true;
        } else if (idx.type == ValueType::Number) {
            key_str = NumberToString(idx.n);
            have_key = true;
        }
        if (have_key) {
            auto* dict = static_cast<DictObj*>(obj.obj);
            auto it = dict->index.find(key_str);
            if (it != dict->index.end()) {
                dict->entries[it->second].second = val;
            } else {
                size_t pos = dict->entries.size();
                dict->entries.push_back({key_str, val});
                dict->index[key_str] = pos;
            }
        }
    }
}

} // namespace ava