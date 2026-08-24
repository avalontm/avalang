#include "builtin_shared.h"
// NumberToString ya vive en vm/vm_helpers.cpp (implementacion manual,
// segura en freestanding/barekernel) -- este archivo tenia una segunda
// definicion de "ava::NumberToString(double)" con el mismo nombre
// calificado y sin `static`, lo que rompia el link real (duplicate
// symbol) apenas se intentaba enlazar la biblioteca completa en vez de
// solo `-fsyntax-only` por archivo (encontrado en la Fase 7 de
// AVALANG_IMPORT_SYSTEM_PLAN.md). Se elimina la copia local y se
// reusa la version canonica de vm/, en vez de renombrar y mantener dos
// formateadores de numero que puedan divergir en el mismo binario.
#include "vm/vm_helpers.h"

namespace ava {

Value MakeNilV() { return Value::Nil(); }

avastd::string TypeName(const Value& v) {
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
        case ValueType::Module:    return "module";
        default:                   return "unknown";
    }
}

avastd::string ToDisplayString(const Value& v) {
    switch (v.type) {
        case ValueType::Nil:    return "nil";
        case ValueType::Bool:   return v.b ? "true" : "false";
        case ValueType::Number: return NumberToString(v.n);
        case ValueType::String: return static_cast<StringObj*>(v.obj)->data;
        case ValueType::List: {
            auto* list = static_cast<ListObj*>(v.obj);
            avastd::string out = "[";
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
            avastd::string out = "{";
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
            return "<" + (inst->cls ? inst->cls->name : avastd::string("instance")) + ">";
        }
        case ValueType::Class: {
            auto* cls = static_cast<ClassObj*>(v.obj);
            return "<class " + cls->name + ">";
        }
        case ValueType::Module: {
            auto* mod = static_cast<ModuleObj*>(v.obj);
            return "<extern " + mod->name + ">";
        }
        default:
            return "<" + TypeName(v) + ">";
    }
}

double AsNumber(const Value& v) {
    switch (v.type) {
        case ValueType::Number: return v.n;
        case ValueType::Bool:   return v.b ? 1.0 : 0.0;
        case ValueType::String: {
#if AVA_HAVE_STD_LIBRARY
            // avastd::stod == std::stod here (hosted alias) and DOES
            // throw (invalid_argument/out_of_range) on non-numeric
            // input -- caught to return 0.0 like before. In
            // freestanding builds (branch below) avastd::stod is a
            // hand-written parser that never throws (see
            // ava_string.h), so a try/catch there would be dead code
            // (and might not even compile if the kernel is built with
            // -fno-exceptions).
            try { return avastd::stod(static_cast<StringObj*>(v.obj)->data); }
            catch (...) { return 0.0; }
#else
            return avastd::stod(static_cast<StringObj*>(v.obj)->data);
#endif
        }
        default: return 0.0;
    }
}

avastd::vector<Value> CollectItems(const avastd::vector<Value>& args) {
    if (args.size() == 1 && args[0].type == ValueType::List) {
        return static_cast<ListObj*>(args[0].obj)->items;
    }
    return args;
}

avastd::vector<Value> ArgsToValues(const ava_value_t* args, size_t count) {
    avastd::vector<Value> out;
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

} // namespace ava
