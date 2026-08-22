#include "vm.h"
#include "vm_internal.h"

namespace ava {

void OpSlice(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm) {
    auto& obj = frame.registers[in.b];
    double start_val = 0;
    double end_val = 0;
    if (in.c < frame.registers.size() && frame.registers[in.c].type == ValueType::Number) {
        start_val = frame.registers[in.c].n;
    }
    if (in.a < frame.registers.size() && frame.registers[in.a].type == ValueType::Number) {
        end_val = frame.registers[in.a].n;
    }
    if (avastd::abs(start_val - avastd::round(start_val)) >= 0.0000001 || start_val < 0) {
        AVA_THROW(avastd::runtime_error(avastd::string("slice start must be a non-negative integer, got ") + NumberToString(start_val)));
    }
    if (avastd::abs(end_val - avastd::round(end_val)) >= 0.0000001 || end_val < 0) {
        AVA_THROW(avastd::runtime_error(avastd::string("slice end must be a non-negative integer, got ") + NumberToString(end_val)));
    }
    size_t s_start = static_cast<size_t>(avastd::round(start_val));
    size_t s_end = static_cast<size_t>(avastd::round(end_val));
    if (obj.type == ValueType::List) {
        auto* list = static_cast<ListObj*>(obj.obj);
        size_t len = list->items.size();
        size_t start_idx = s_start < len ? s_start : len;
        size_t end_idx = s_end < len ? s_end : len;
        auto* result = new ListObj();
        for (size_t i = start_idx; i < end_idx && i < len; ++i) {
            result->items.push_back(list->items[i]);
        }
        Value v; v.type = ValueType::List; v.obj = result;
        frame.registers[in.a] = v;
    } else if (obj.type == ValueType::String) {
        auto* str = static_cast<StringObj*>(obj.obj);
        size_t len = str->data.size();
        size_t start_idx = s_start < len ? s_start : len;
        size_t end_idx = s_end < len ? s_end : len;
        avastd::string result = str->data.substr(start_idx, end_idx - start_idx);
        Value v; v.type = ValueType::String; v.obj = new StringObj(result);
        frame.registers[in.a] = v;
    } else {
        frame.registers[in.a] = Value::Nil();
    }
}

} // namespace ava