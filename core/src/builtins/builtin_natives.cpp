#include "builtin_natives.h"

#include "vm/value.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

using namespace ava;

namespace {

Value MakeNilV() { return Value::Nil(); }

// Mirrors NumberToString in core/src/vm/vm.cpp (kept file-local there),
// duplicated here so str()/print() format numbers identically to the
// VM's own ADD-with-string-coercion path.
std::string NumberToString(double n) {
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

std::string TypeName(const Value& v) {
    switch (v.type) {
        case ValueType::Nil:       return "nil";
        case ValueType::Bool:      return "bool";
        case ValueType::Number:    return "number";
        case ValueType::String:    return "string";
        case ValueType::List:      return "list";
        case ValueType::Dict:      return "dict";
        case ValueType::Function:  return "function";
        case ValueType::Instance:  return "instance";
        case ValueType::Class:     return "class";
        case ValueType::Coroutine: return "coroutine";
        case ValueType::Native:    return "native";
        case ValueType::Bound:     return "bound";
        case ValueType::Exception: return "exception";
        default:                   return "unknown";
    }
}

std::string ToDisplayString(const Value& v) {
    switch (v.type) {
        case ValueType::Nil:    return "nil";
        case ValueType::Bool:   return v.b ? "true" : "false";
        case ValueType::Number: return NumberToString(v.n);
        case ValueType::String: return static_cast<StringObj*>(v.obj)->data;
        case ValueType::List: {
            auto* list = static_cast<ListObj*>(v.obj);
            std::string out = "[";
            for (size_t i = 0; i < list->items.size(); ++i) {
                if (i > 0) out += ", ";
                if (list->items[i].type == ValueType::String) {
                    out += "\"" + static_cast<StringObj*>(list->items[i].obj)->data + "\"";
                } else {
                    out += ToDisplayString(list->items[i]);
                }
            }
            out += "]";
            return out;
        }
        case ValueType::Dict: {
            auto* dict = static_cast<DictObj*>(v.obj);
            std::string out = "{";
            for (size_t i = 0; i < dict->entries.size(); ++i) {
                if (i > 0) out += ", ";
                out += "\"" + dict->entries[i].first + "\": ";
                const Value& ev = dict->entries[i].second;
                if (ev.type == ValueType::String) {
                    out += "\"" + static_cast<StringObj*>(ev.obj)->data + "\"";
                } else {
                    out += ToDisplayString(ev);
                }
            }
            out += "}";
            return out;
        }
        case ValueType::Instance: {
            auto* inst = static_cast<InstanceObj*>(v.obj);
            return "<" + (inst->cls ? inst->cls->name : std::string("instance")) + ">";
        }
        case ValueType::Class: {
            auto* cls = static_cast<ClassObj*>(v.obj);
            return "<class " + cls->name + ">";
        }
        default:
            return "<" + TypeName(v) + ">";
    }
}

// Best-effort numeric coercion, used by math builtins (abs/round/floor/...).
double AsNumber(const Value& v) {
    switch (v.type) {
        case ValueType::Number: return v.n;
        case ValueType::Bool:   return v.b ? 1.0 : 0.0;
        case ValueType::String: {
            try { return std::stod(static_cast<StringObj*>(v.obj)->data); }
            catch (...) { return 0.0; }
        }
        default: return 0.0;
    }
}

// Collects the values to operate over for variadic-or-single-list builtins
// like min/max/sum/any/all/sorted/reversed: if called with exactly one
// list argument, iterate its items; otherwise treat every arg as an item.
std::vector<Value> CollectItems(const std::vector<Value>& args) {
    if (args.size() == 1 && args[0].type == ValueType::List) {
        return static_cast<ListObj*>(args[0].obj)->items;
    }
    return args;
}

std::vector<Value> ArgsToValues(const ava_value_t* args, size_t count) {
    std::vector<Value> out;
    out.reserve(count);
    for (size_t i = 0; i < count; ++i) out.push_back(FromC(args[i]));
    return out;
}

bool LessThan(const Value& a, const Value& b) {
    if (a.type == ValueType::String && b.type == ValueType::String) {
        return static_cast<StringObj*>(a.obj)->data < static_cast<StringObj*>(b.obj)->data;
    }
    return AsNumber(a) < AsNumber(b);
}

} // namespace

extern "C" {

ava_value_t builtin_type(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 1) return ToC(Value::String("nil"));
    return ToC(Value::String(TypeName(FromC(args[0]))));
}

ava_value_t builtin_str(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 1) return ToC(Value::String(""));
    return ToC(Value::String(ToDisplayString(FromC(args[0]))));
}

ava_value_t builtin_int(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 1) return ToC(Value::Number(0));
    return ToC(Value::Number(std::trunc(AsNumber(FromC(args[0])))));
}

ava_value_t builtin_float(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 1) return ToC(Value::Number(0));
    return ToC(Value::Number(AsNumber(FromC(args[0]))));
}

ava_value_t builtin_print(AvaVM*, const ava_value_t* args, size_t count, void*) {
    for (size_t i = 0; i < count; ++i) {
        if (i > 0) std::fputc(' ', stdout);
        std::fputs(ToDisplayString(FromC(args[i])).c_str(), stdout);
    }
    std::fputc('\n', stdout);
    return ToC(MakeNilV());
}

ava_value_t builtin_abs(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 1) return ToC(Value::Number(0));
    return ToC(Value::Number(std::fabs(AsNumber(FromC(args[0])))));
}

ava_value_t builtin_round(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 1) return ToC(Value::Number(0));
    return ToC(Value::Number(std::round(AsNumber(FromC(args[0])))));
}

ava_value_t builtin_floor(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 1) return ToC(Value::Number(0));
    return ToC(Value::Number(std::floor(AsNumber(FromC(args[0])))));
}

ava_value_t builtin_ceil(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 1) return ToC(Value::Number(0));
    return ToC(Value::Number(std::ceil(AsNumber(FromC(args[0])))));
}

ava_value_t builtin_pow(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 2) return ToC(Value::Number(0));
    return ToC(Value::Number(std::pow(AsNumber(FromC(args[0])), AsNumber(FromC(args[1])))));
}

ava_value_t builtin_sqrt(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 1) return ToC(Value::Number(0));
    return ToC(Value::Number(std::sqrt(AsNumber(FromC(args[0])))));
}

ava_value_t builtin_min(AvaVM*, const ava_value_t* args, size_t count, void*) {
    auto items = CollectItems(ArgsToValues(args, count));
    if (items.empty()) return ToC(MakeNilV());
    Value best = items[0];
    for (size_t i = 1; i < items.size(); ++i) {
        if (LessThan(items[i], best)) best = items[i];
    }
    return ToC(best);
}

ava_value_t builtin_max(AvaVM*, const ava_value_t* args, size_t count, void*) {
    auto items = CollectItems(ArgsToValues(args, count));
    if (items.empty()) return ToC(MakeNilV());
    Value best = items[0];
    for (size_t i = 1; i < items.size(); ++i) {
        if (LessThan(best, items[i])) best = items[i];
    }
    return ToC(best);
}

ava_value_t builtin_sum(AvaVM*, const ava_value_t* args, size_t count, void*) {
    auto items = CollectItems(ArgsToValues(args, count));
    double total = 0;
    for (const auto& v : items) total += AsNumber(v);
    return ToC(Value::Number(total));
}

ava_value_t builtin_sorted(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 1 || args[0].type != AVA_LIST) return ava_list_create(nullptr);
    auto items = static_cast<ListObj*>(FromC(args[0]).obj)->items;
    std::sort(items.begin(), items.end(), LessThan);
    Value out; out.type = ValueType::List; out.obj = new ListObj();
    static_cast<ListObj*>(out.obj)->items = items;
    return ToC(out);
}

ava_value_t builtin_reversed(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 1 || args[0].type != AVA_LIST) return ava_list_create(nullptr);
    auto items = static_cast<ListObj*>(FromC(args[0]).obj)->items;
    std::reverse(items.begin(), items.end());
    Value out; out.type = ValueType::List; out.obj = new ListObj();
    static_cast<ListObj*>(out.obj)->items = items;
    return ToC(out);
}

ava_value_t builtin_any(AvaVM*, const ava_value_t* args, size_t count, void*) {
    auto items = CollectItems(ArgsToValues(args, count));
    for (const auto& v : items) {
        if (v.IsTruthy()) return ToC(Value::Bool(true));
    }
    return ToC(Value::Bool(false));
}

ava_value_t builtin_all(AvaVM*, const ava_value_t* args, size_t count, void*) {
    auto items = CollectItems(ArgsToValues(args, count));
    for (const auto& v : items) {
        if (!v.IsTruthy()) return ToC(Value::Bool(false));
    }
    return ToC(Value::Bool(true));
}

ava_value_t builtin_len(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 1) return ToC(Value::Number(0));
    Value v = FromC(args[0]);
    switch (v.type) {
        case ValueType::String: return ToC(Value::Number(static_cast<double>(static_cast<StringObj*>(v.obj)->data.size())));
        case ValueType::List:   return ToC(Value::Number(static_cast<double>(static_cast<ListObj*>(v.obj)->items.size())));
        case ValueType::Dict:   return ToC(Value::Number(static_cast<double>(static_cast<DictObj*>(v.obj)->entries.size())));
        default:                return ToC(Value::Number(0));
    }
}

ava_value_t builtin_range(AvaVM*, const ava_value_t* args, size_t count, void*) {
    double start = 0, end = 0, step = 1;
    if (count == 1) {
        end = AsNumber(FromC(args[0]));
    } else if (count == 2) {
        start = AsNumber(FromC(args[0]));
        end = AsNumber(FromC(args[1]));
    } else if (count >= 3) {
        start = AsNumber(FromC(args[0]));
        end = AsNumber(FromC(args[1]));
        step = AsNumber(FromC(args[2]));
    } else {
        return ava_list_create(nullptr);
    }

    Value out; out.type = ValueType::List; out.obj = new ListObj();
    auto* list = static_cast<ListObj*>(out.obj);
    if (step > 0) {
        for (double i = start; i < end; i += step) list->items.push_back(Value::Number(i));
    } else if (step < 0) {
        for (double i = start; i > end; i += step) list->items.push_back(Value::Number(i));
    }
    return ToC(out);
}

} // extern "C"
