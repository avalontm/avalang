#include "builtin_natives.h"

#include "vm/value.h"
#include "vm/vm.h"
#include "vm/vm_platform_accessor.h"
#include "builtin_shared.h"

#include "../../platform/barekernel/stdcompat/ava_stdcompat.h"

using namespace ava;


extern "C" {

ava_value_t builtin_type(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 1) return ToCNew(Value::String("nil"));
    return ToCNew(Value::String(TypeName(FromC(args[0]))));
}

ava_value_t builtin_str(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 1) return ToCNew(Value::String(""));
    return ToCNew(Value::String(ToDisplayString(FromC(args[0]))));
}

ava_value_t builtin_int(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 1) return ToC(Value::Number(0));
    return ToC(Value::Number(avastd::trunc(AsNumber(FromC(args[0])))));
}

ava_value_t builtin_float(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 1) return ToC(Value::Number(0));
    return ToC(Value::Number(AsNumber(FromC(args[0]))));
}

ava_value_t builtin_print(AvaVM* vm, const ava_value_t* args, size_t count, void*) {

    avastd::string line;
    for (size_t i = 0; i < count; ++i) {
        if (i > 0) line += ' ';
        line += ToDisplayString(FromC(args[i]));
    }
    line += '\n';
    ava::VM* raw_vm = reinterpret_cast<ava::VM*>(vm);
    if (raw_vm) {
        raw_vm->Print(line);
    } else {
        // raw_vm nulo no deberia pasar en la ruta real del VM (siempre
        // llama con un AvaVM* valido) -- pero este builtin igual tiene
        // que compilar en el kernel sin libc. Antes: std::fputs(...,
        // stdout). Mismo motivo que VM::Print (vm_core.cpp): va por
        // IConsole en vez de FILE*.
        VmPlatformAccessor::Get().Console().Write(line);
    }
    return ToC(MakeNilV());
}

ava_value_t builtin_input(AvaVM* vm, const ava_value_t* args, size_t count, void*) {
    avastd::string prompt;
    if (count >= 1) {
        prompt = ToDisplayString(FromC(args[0]));
    }
    ava::VM* raw_vm = reinterpret_cast<ava::VM*>(vm);
    avastd::string line;
    if (raw_vm) {
        line = raw_vm->ReadLine(prompt);
    } else {
        // Mismo caso de raw_vm nulo que builtin_print de arriba -- antes
        // std::fputs/std::fflush/std::fgets (FILE* de libc, no existe en
        // el kernel), ahora IConsole (mismo patron que VM::ReadLine).
        if (!prompt.empty()) {
            VmPlatformAccessor::Get().Console().Write(prompt);
        }
        VmPlatformAccessor::Get().Console().ReadLine(line);
    }
    return ToCNew(Value::String(line));
}

ava_value_t builtin_abs(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 1) return ToC(Value::Number(0));
    return ToC(Value::Number(avastd::fabs(AsNumber(FromC(args[0])))));
}

ava_value_t builtin_round(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 1) return ToC(Value::Number(0));
    return ToC(Value::Number(avastd::round(AsNumber(FromC(args[0])))));
}

ava_value_t builtin_floor(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 1) return ToC(Value::Number(0));
    return ToC(Value::Number(avastd::floor(AsNumber(FromC(args[0])))));
}

ava_value_t builtin_ceil(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 1) return ToC(Value::Number(0));
    return ToC(Value::Number(avastd::ceil(AsNumber(FromC(args[0])))));
}

ava_value_t builtin_pow(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 2) return ToC(Value::Number(0));
    return ToC(Value::Number(avastd::pow(AsNumber(FromC(args[0])), AsNumber(FromC(args[1])))));
}

ava_value_t builtin_sqrt(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 1) return ToC(Value::Number(0));
    return ToC(Value::Number(avastd::sqrt(AsNumber(FromC(args[0])))));
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
    avastd::sort(items.begin(), items.end(), LessThan);
    Value out; out.type = ValueType::List; out.obj = new ListObj();
    static_cast<ListObj*>(out.obj)->items = items;
    return ToCNew(out);
}

ava_value_t builtin_reversed(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 1 || args[0].type != AVA_LIST) return ava_list_create(nullptr);
    auto items = static_cast<ListObj*>(FromC(args[0]).obj)->items;
    avastd::reverse(items.begin(), items.end());
    Value out; out.type = ValueType::List; out.obj = new ListObj();
    static_cast<ListObj*>(out.obj)->items = items;
    return ToCNew(out);
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

ava_value_t builtin_import(AvaVM* vm, const ava_value_t* args, size_t count, void*) {
    avastd::string module_path = count >= 1 ? ToDisplayString(FromC(args[0])) : "";
    avastd::string alias = (count >= 2) ? ToDisplayString(FromC(args[1])) : "";

    auto* raw_vm = reinterpret_cast<VM*>(vm);
    Value result = raw_vm->DoImport(module_path, alias);
    return ToC(result);
}

ava_value_t builtin_slice(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count < 1) return ToC(Value::Nil());
    Value obj = FromC(args[0]);
    if (obj.type != ValueType::List && obj.type != ValueType::String) return ToC(Value::Nil());

    auto has_arg = [&](size_t i) -> bool {
        if (i >= count) return false;
        return FromC(args[i]).type != ValueType::Nil;
    };
    auto get_num = [&](size_t i, double def) -> double {
        if (i >= count) return def;
        Value v = FromC(args[i]);
        if (v.type == ValueType::Nil) return def;
        if (v.type == ValueType::Number) return v.n;
        if (v.type == ValueType::Bool) return v.b ? 1.0 : 0.0;
        return def;
    };

    bool has_start = has_arg(1);
    bool has_end = has_arg(2);
    bool has_step = has_arg(3);

    double step_d = has_step ? get_num(3, 1) : 1;
    if (step_d == 0) step_d = 1;
    long long step = static_cast<long long>(step_d);

    auto compute_bounds = [&](size_t len, long long& start, long long& end) {
        if (step > 0) {
            start = has_start ? static_cast<long long>(get_num(1, 0)) : 0;
            end = has_end ? static_cast<long long>(get_num(2, static_cast<double>(len))) : static_cast<long long>(len);
            if (start < 0) start += static_cast<long long>(len);
            if (end < 0) end += static_cast<long long>(len);
            if (start < 0) start = 0;
            if (end > static_cast<long long>(len)) end = static_cast<long long>(len);
            if (end < start) end = start;
        } else {
            start = has_start ? static_cast<long long>(get_num(1, static_cast<double>(len) - 1)) : static_cast<long long>(len) - 1;
            end = has_end ? static_cast<long long>(get_num(2, -1)) : -1;
            if (start < 0) start += static_cast<long long>(len);
            if (has_end && end < 0) end += static_cast<long long>(len);
            if (start >= static_cast<long long>(len)) start = static_cast<long long>(len) - 1;
            if (start < -1) start = -1;
            if (end < -1) end = -1;
            if (end > start) end = start;
        }
    };

    if (obj.type == ValueType::List) {
        auto* list = static_cast<ListObj*>(obj.obj);
        size_t len = list->items.size();
        long long start, end;
        compute_bounds(len, start, end);

        auto* result = new ListObj();
        if (step > 0) {
            for (long long i = start; i < end; i += step) {
                if (i >= 0 && i < static_cast<long long>(len)) {
                    result->items.push_back(list->items[i]);
                }
            }
        } else {
            for (long long i = start; i > end; i += step) {
                if (i >= 0 && i < static_cast<long long>(len)) {
                    result->items.push_back(list->items[i]);
                }
            }
        }
        Value out; out.type = ValueType::List; out.obj = result;
        return ToCNew(out);
    }

    auto* str = static_cast<StringObj*>(obj.obj);
    size_t len = str->data.size();
    long long start, end;
    compute_bounds(len, start, end);

    avastd::string result;
    if (step > 0) {
        for (long long i = start; i < end; i += step) {
            if (i >= 0 && i < static_cast<long long>(len)) result += str->data[i];
        }
    } else {
        for (long long i = start; i > end; i += step) {
            if (i >= 0 && i < static_cast<long long>(len)) result += str->data[i];
        }
    }
    return ToCNew(Value::String(result));
}

ava_value_t builtin_setglobal(AvaVM* vm, const ava_value_t* args, size_t count, void*) {
    if (count < 2 || !args) return ToC(MakeNilV());

    Value nameVal = FromC(args[0]);
    if (nameVal.type != ValueType::String) return ToC(MakeNilV());

    avastd::string name = static_cast<StringObj*>(nameVal.obj)->data;
    Value value = FromC(args[1]);

    auto* raw_vm = reinterpret_cast<ava::VM*>(vm);
    if (raw_vm) {
        raw_vm->SetGlobal(name, value);
    }
    return ToC(value);
}

} // extern "C"