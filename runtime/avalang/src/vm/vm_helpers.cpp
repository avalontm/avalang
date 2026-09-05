#include "vm.h"
#include "vm_internal.h"
#include "../../platform/barekernel/stdcompat/ava_math.h"
#include "vm_helpers.h"
#include "vm_platform_accessor.h"

#ifdef _WIN32
#define PATH_SEPARATOR_CHAR '\\'
#define PATH_SEPARATOR "\\"
#else
#define PATH_SEPARATOR_CHAR '/'
#define PATH_SEPARATOR "/"
#endif

namespace ava {

avastd::string GetFileDir(const avastd::string& path) {
    size_t pos = path.find_last_of("/\\");
    if (pos == avastd::string::npos) return ".";
    return path.substr(0, pos);
}

avastd::string NumberToString(double n) {
    if (avastd::abs(n - avastd::round(n)) < 0.0000001)
        return avastd::to_string(static_cast<long long>(avastd::round(n)));

    // Reemplaza el std::ostringstream + std::setprecision(15) original
    // (ambos de libstdc++, no disponibles sin CKM_CAP_LIBSTDCPP). Formatea
    // a mano hasta 15 digitos significativos totales, recortando ceros de
    // cola -- mismo resultado visible que el codigo original para los
    // rangos de numero que un script de AvaLang tipicamente produce.
    // LIMITACION CONOCIDA: no es una implementacion "shortest round-trip"
    // (tipo Ryu/Grisu) como la que usa una libstdc++ moderna por debajo;
    // para floats en los limites de precision de double (muy grandes, muy
    // chicos, o resultado de muchas operaciones acumuladas) puede diferir
    // en el ultimo digito. Si eso se vuelve un problema real (bug de
    // "0.1 + 0.2 se imprime distinto en barekernel vs Windows"), esta
    // funcion es el lugar a revisar primero.
    bool neg = n < 0;
    double v = neg ? -n : n;

    long long int_part = static_cast<long long>(v);
    double frac = v - static_cast<double>(int_part);

    avastd::string int_str = avastd::to_string(int_part);
    int max_frac_digits = 15 - static_cast<int>(int_str.size());

    avastd::string frac_str;
    if (max_frac_digits > 0 && frac > 0.0) {
        frac_str += '.';
        for (int i = 0; i < max_frac_digits; ++i) {
            frac *= 10.0;
            int digit = static_cast<int>(frac);
            if (digit > 9) digit = 9;  // guarda contra error de redondeo FP
            if (digit < 0) digit = 0;
            frac_str += static_cast<char>('0' + digit);
            frac -= digit;
        }
        avastd::size_t last_not_zero = frac_str.find_last_not_of('0');
        if (last_not_zero == 0)
            frac_str.erase(0);
        else
            frac_str.erase(last_not_zero + 1);
    }

    return (neg ? avastd::string("-") : avastd::string("")) + int_str + frac_str;
}

avastd::string ValueToString(const Value& v) {
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
                    out += ValueToString(list->items[i]);
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
                    out += ValueToString(ev);
                }
            }
            out += "}";
            return out;
        }
        default: return "<" + avastd::string("value") + ">";
    }
}

bool ValueEquals(const Value& a, const Value& b) {
    if (a.type != b.type) return false;
    switch (a.type) {
        case ValueType::Nil:    return true;
        case ValueType::Bool:   return a.b == b.b;
        case ValueType::Number: return a.n == b.n;
        case ValueType::String: {
            auto* sa = static_cast<StringObj*>(a.obj);
            auto* sb = static_cast<StringObj*>(b.obj);
            return sa->data == sb->data;
        }
        case ValueType::List: {
            auto* la = static_cast<ListObj*>(a.obj);
            auto* lb = static_cast<ListObj*>(b.obj);
            if (la->items.size() != lb->items.size()) return false;
            for (size_t i = 0; i < la->items.size(); ++i) {
                if (!ValueEquals(la->items[i], lb->items[i])) return false;
            }
            return true;
        }
        case ValueType::Dict: {
            auto* da = static_cast<DictObj*>(a.obj);
            auto* db = static_cast<DictObj*>(b.obj);
            if (da->entries.size() != db->entries.size()) return false;
            for (size_t i = 0; i < da->entries.size(); ++i) {
                auto it = db->index.find(da->entries[i].first);
                if (it == db->index.end()) return false;
                if (!ValueEquals(da->entries[i].second, db->entries[it->second].second)) return false;
            }
            return true;
        }
        default: return a.obj == b.obj;
    }
}

size_t ValidateIntegerIndex(double n, size_t len, const char* context) {
    if (avastd::abs(n - avastd::round(n)) >= 0.0000001) {
        AVA_THROW(avastd::runtime_error(avastd::string(context) + ": index must be an integer, got " + NumberToString(n)));
    }
    double rounded = avastd::round(n);
    if (rounded < 0) {
        AVA_THROW(avastd::runtime_error(avastd::string(context) + ": index must not be negative, got " + NumberToString(rounded)));
    }
    if (rounded > static_cast<double>(avastd::size_t_max)) {
        AVA_THROW(avastd::runtime_error(avastd::string(context) + ": index too large: " + NumberToString(rounded)));
    }
    size_t pos = static_cast<size_t>(rounded);
    // Bug #6 en BUGS_ENCONTRADOS.md ("escritura fuera de rango es
    // no-op silencioso"): antes esta funcion solo validaba tipo/signo y
    // dejaba que cada call site (OpGetIndex/OpSetIndex en
    // vm_containers.cpp) decidiera por su cuenta que hacer con un indice
    // positivo fuera de rango -- y todos decidian "nada" (silencio). Al
    // mover el chequeo de limite superior aca (unico lugar que ya conoce
    // el largo del contenedor via `len`), lectura y escritura quedan con
    // el MISMO comportamiento uniforme que ya tenian indice negativo e
    // indice no entero: excepcion dura, estilo VB6 "Subscript out of
    // range" pero con el detalle moderno de indice y largo real.
    if (pos >= len) {
        AVA_THROW(avastd::runtime_error(avastd::string(context) + ": index out of range: " +
            NumberToString(rounded) + " (length " + avastd::to_string(len) + ")"));
    }
    return pos;
}

avastd::string JoinPath(const avastd::string& a, const avastd::string& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    char sep = PATH_SEPARATOR_CHAR;
    if (a.back() == sep || a.back() == '/') return a + b;
    return a + PATH_SEPARATOR + b;
}

avastd::string GetCurrentWorkingDir() {
    return VmPlatformAccessor::Get().Environment().GetCurrentDirectory();
}

// Static (non-instance) attrs live only on the class that declared them.
// CompileClass no longer copies a subclass's base attrs into its own
// `attrs` map (that made `Sub.x` and `Base.x` two independent copies as
// soon as either was written to) -- instead a subclass just carries a
// `__base__` link, and lookups walk it here to find the ClassObj that
// actually owns `name`, so `Base.x` and `Sub.x` stay the same storage.
ClassObj* FindClassOwningAttr(ClassObj* cls, const avastd::string& name) {
    ClassObj* cur = cls;
    while (cur) {
        if (cur->attrs.find(name) != cur->attrs.end()) return cur;
        auto base_it = cur->attrs.find("__base__");
        if (base_it == cur->attrs.end() || base_it->second.type != ValueType::Class) break;
        cur = static_cast<ClassObj*>(base_it->second.obj);
    }
    return nullptr;
}

const char* ValueTypeName(ValueType t) {
    switch (t) {
        case ValueType::Nil:       return "Nil";
        case ValueType::Bool:      return "Bool";
        case ValueType::Number:    return "Number";
        case ValueType::String:    return "String";
        case ValueType::List:      return "List";
        case ValueType::Dict:      return "Dict";
        case ValueType::Function:  return "Function";
        case ValueType::Instance:  return "Instance";
        case ValueType::Class:     return "Class";
        case ValueType::Coroutine: return "Coroutine";
        case ValueType::Native:    return "Native";
        case ValueType::Bound:     return "Bound";
        case ValueType::Exception: return "Exception";
        case ValueType::Module:    return "Module";
        case ValueType::Task:      return "Task";
    }
    return "Unknown";
}

// Ver el comentario de la declaracion en vm_helpers.h. Vive ACA (no
// duplicada en vm_arith.cpp y vm_compare.cpp) a proposito: es el mismo
// tipo de decision que antes vivia repetida en dos lugares que tenian
// que estar de acuerdo a mano (CompileStmt/CompileExprToReg con
// current_line_, el fprintf duplicado de avacli) y terminaba
// divergiendo. Con un solo helper compartido, los operadores
// aritmeticos Y los relacionales usan la misma definicion de "esto
// coerciona a numero".
double CoerceToNumber(const Value& v, const char* op) {
    if (v.type == ValueType::Number) {
        return v.n;
    }
    if (v.type == ValueType::String) {
        const avastd::string& raw = static_cast<StringObj*>(v.obj)->data;
        avastd::size_t n = raw.size();

        avastd::size_t start = 0;
        while (start < n && (raw[start] == ' ' || raw[start] == '\t' ||
                              raw[start] == '\r' || raw[start] == '\n')) {
            ++start;
        }
        avastd::size_t end = n;
        while (end > start && (raw[end - 1] == ' ' || raw[end - 1] == '\t' ||
                                raw[end - 1] == '\r' || raw[end - 1] == '\n')) {
            --end;
        }
        avastd::string trimmed = raw.substr(start, end - start);

        bool looks_numeric = !trimmed.empty();
        if (looks_numeric) {
            char c0 = trimmed[0];
            looks_numeric = (c0 == '+' || c0 == '-' || c0 == '.' ||
                              (c0 >= '0' && c0 <= '9'));
        }
        if (looks_numeric) {
            avastd::size_t consumed = 0;
            double parsed = avastd::stod(trimmed, &consumed);
            if (consumed == trimmed.size()) {
                return parsed;
            }
        }
        AVA_THROW(avastd::runtime_error(
            avastd::string("type mismatch: '") + op +
            "' expects Number, got non-numeric String \"" + raw + "\""));
    }
    AVA_THROW(avastd::runtime_error(
        avastd::string("type mismatch: '") + op + "' expects Number, got " +
        ValueTypeName(v.type)));
}

} // namespace ava