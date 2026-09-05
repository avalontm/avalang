#include "builtin.h"
#include "builtin_shared.h"
#include "vm/value.h"
#include "vm/vm_helpers.h"
#include <algorithm>

using namespace ava;

namespace {

ava_value_t MakeNil() {
    ava_value_t v{};
    v.type = AVA_NIL;
    return v;
}

ava_value_t MakeBool(bool b) {
    ava_value_t v{};
    v.type = AVA_BOOL;
    v.as.b = b;
    return v;
}

ava_value_t MakeNumber(double n) {
    ava_value_t v{};
    v.type = AVA_NUMBER;
    v.as.n = n;
    return v;
}

ava_value_t MakeList() {
    return ava_list_create(nullptr);
}

}

extern "C" {

ava_value_t builtin_list_append(AvaVM* vm, const ava_value_t* args, size_t count, void*) {
    if (!args || count < 2) return MakeNil();
    if (args[0].type != AVA_LIST) return MakeNil();
    ava_list_append(vm, args[0], args[1]);
    return args[0];
}

ava_value_t builtin_list_pop(AvaVM* vm, const ava_value_t* args, size_t count, void*) {
    if (!args) return MakeNil();
    if (args[0].type != AVA_LIST) return MakeNil();
    
    size_t len = ava_list_length(vm, args[0]);
    if (len == 0) return MakeNil();
    
    ava_value_t last = ava_list_get(vm, args[0], len - 1);
    ava_value_t result;
    result.type = last.type;
    result.as = last.as;
    
    ava_list_remove(vm, args[0], len - 1);
    return result;
}

ava_value_t builtin_list_push(AvaVM* vm, const ava_value_t* args, size_t count, void*) {
    if (!args || count < 2) return MakeNil();
    if (args[0].type != AVA_LIST) return MakeNil();
    ava_list_append(vm, args[0], args[1]);
    return args[0];
}

ava_value_t builtin_list_insert(AvaVM* vm, const ava_value_t* args, size_t count, void*) {
    if (!args || count < 3) return MakeNil();
    if (args[0].type != AVA_LIST) return MakeNil();
    
    size_t pos = static_cast<size_t>(args[1].as.n);
    ava_list_insert(vm, args[0], pos, args[2]);
    return args[0];
}

ava_value_t builtin_list_remove(AvaVM* vm, const ava_value_t* args, size_t count, void*) {
    if (!args || count < 2) return MakeNil();
    if (args[0].type != AVA_LIST) return MakeNil();

    // Antes comparaba item.as.n == args[1].as.n -- el campo numerico
    // crudo del union ava_value_t.as, sin mirar el tipo real. Para
    // strings/instancias eso compara el puntero/bits internos, no el
    // contenido, asi que dos strings iguales pero con distinta identidad
    // de objeto (ej. uno literal y otro construido en runtime con `+`)
    // no coincidian aunque `==` si los considerara iguales. Fix: usar
    // ValueEquals (la misma comparacion estructural que ya usa el
    // operador == del lenguaje, vm_helpers.cpp) sobre los Value internos.
    Value needle = FromC(args[1]);
    size_t len = ava_list_length(vm, args[0]);
    for (size_t i = 0; i < len; i++) {
        Value item = FromC(ava_list_get(vm, args[0], i));
        if (ValueEquals(item, needle)) {
            ava_list_remove(vm, args[0], i);
            return args[0];
        }
    }
    return args[0];
}

ava_value_t builtin_list_removeAt(AvaVM* vm, const ava_value_t* args, size_t count, void*) {
    if (!args || count < 2) return MakeNil();
    if (args[0].type != AVA_LIST) return MakeNil();
    if (args[1].type != AVA_NUMBER) return MakeNil();

    size_t len = ava_list_length(vm, args[0]);
    long idx = static_cast<long>(args[1].as.n);
    if (idx < 0 || static_cast<size_t>(idx) >= len) return MakeNil();

    ava_value_t removed = ava_list_get(vm, args[0], static_cast<size_t>(idx));
    ava_value_t result;
    result.type = removed.type;
    result.as = removed.as;
    ava_list_remove(vm, args[0], static_cast<size_t>(idx));
    return result;
}

ava_value_t builtin_list_length(AvaVM* vm, const ava_value_t* args, size_t, void*) {
    if (!args) return MakeNil();
    if (args[0].type != AVA_LIST) return MakeNil();
    return MakeNumber(static_cast<double>(ava_list_length(vm, args[0])));
}

ava_value_t builtin_list_contains(AvaVM* vm, const ava_value_t* args, size_t count, void*) {
    if (!args || count < 2) return MakeNil();
    if (args[0].type != AVA_LIST) return MakeNil();
    
    // Mismo fix que builtin_list_remove: ValueEquals en vez de comparar
    // el union crudo, para que contains() coincida con lo que dice ==.
    Value needle = FromC(args[1]);
    size_t len = ava_list_length(vm, args[0]);
    for (size_t i = 0; i < len; i++) {
        Value item = FromC(ava_list_get(vm, args[0], i));
        if (ValueEquals(item, needle)) {
            return MakeBool(true);
        }
    }
    return MakeBool(false);
}

// ---------------------------------------------------------------------
// Superficie de API asimetrica en `list` (anotada como pendiente de
// prioridad baja en BUGS_ENCONTRADOS.md, notas finales): `str`
// ya tenia `indexOf`, y existian `sorted()`/`reversed()` globales
// (funcionales, devuelven una lista NUEVA, no mutan -- ver
// builtin_natives.cpp) pero `list` no tenia el equivalente dotted
// in-place (`lst.sort()`/`lst.reverse()`), ni `indexOf`/`clear`/`join`/
// `copy`. Se agregan los 6 metodos que faltaban para emparejar la
// superficie de `str`/las funciones globales, sin tocar nada de lo ya
// existente (mismos criterios: `ValueEquals` para comparar, mismo
// `LessThan` que ya usa `sorted()` para el orden por defecto).
// ---------------------------------------------------------------------

ava_value_t builtin_list_indexOf(AvaVM* vm, const ava_value_t* args, size_t count, void*) {
    if (!args || count < 2) return MakeNumber(-1);
    if (args[0].type != AVA_LIST) return MakeNumber(-1);

    Value needle = FromC(args[1]);
    size_t len = ava_list_length(vm, args[0]);
    for (size_t i = 0; i < len; i++) {
        Value item = FromC(ava_list_get(vm, args[0], i));
        if (ValueEquals(item, needle)) {
            return MakeNumber(static_cast<double>(i));
        }
    }
    return MakeNumber(-1);
}

ava_value_t builtin_list_sort(AvaVM* vm, const ava_value_t* args, size_t count, void*) {
    if (!args || count < 1) return MakeNil();
    if (args[0].type != AVA_LIST) return MakeNil();

    // In-place: a diferencia de sorted() (global, devuelve copia), este
    // ordena la MISMA lista y la retorna (igual convencion que
    // append/insert/remove, que tambien devuelven args[0]).
    Value listVal = FromC(args[0]);
    auto* list = static_cast<ListObj*>(listVal.obj);
    avastd::sort(list->items.begin(), list->items.end(), LessThan);
    return args[0];
}

ava_value_t builtin_list_reverse(AvaVM* vm, const ava_value_t* args, size_t count, void*) {
    if (!args || count < 1) return MakeNil();
    if (args[0].type != AVA_LIST) return MakeNil();

    Value listVal = FromC(args[0]);
    auto* list = static_cast<ListObj*>(listVal.obj);
    avastd::reverse(list->items.begin(), list->items.end());
    return args[0];
}

ava_value_t builtin_list_clear(AvaVM* vm, const ava_value_t* args, size_t count, void*) {
    if (!args || count < 1) return MakeNil();
    if (args[0].type != AVA_LIST) return MakeNil();

    Value listVal = FromC(args[0]);
    auto* list = static_cast<ListObj*>(listVal.obj);
    list->items.clear();
    return args[0];
}

ava_value_t builtin_list_copy(AvaVM* vm, const ava_value_t* args, size_t count, void*) {
    if (!args || count < 1) return MakeList();
    if (args[0].type != AVA_LIST) return MakeList();

    // Shallow copy: mismo criterio que sorted()/reversed() en
    // builtin_natives.cpp (copian el vector de items, no clonan
    // profundamente objetos referenciados adentro).
    Value src = FromC(args[0]);
    Value out; out.type = ValueType::List; out.obj = new ListObj();
    static_cast<ListObj*>(out.obj)->items = static_cast<ListObj*>(src.obj)->items;
    return ToCNew(out);
}

ava_value_t builtin_list_join(AvaVM* vm, const ava_value_t* args, size_t count, void*) {
    if (!args || count < 1) return MakeNil();
    if (args[0].type != AVA_LIST) return MakeNil();

    avastd::string sep = ", ";
    if (count >= 2 && args[1].type == AVA_STRING) {
        sep = FromC(args[1]).obj ? static_cast<StringObj*>(FromC(args[1]).obj)->data : "";
    }

    Value listVal = FromC(args[0]);
    auto* list = static_cast<ListObj*>(listVal.obj);
    avastd::string out;
    for (size_t i = 0; i < list->items.size(); i++) {
        if (i > 0) out += sep;
        // ToDisplayString (no ToC/quote-wrapped): join() de un string
        // no debe agregar comillas alrededor, a diferencia de como
        // ToDisplayString imprime una LISTA de strings (con comillas,
        // ver builtin_shared.cpp) -- acá cada item es el elemento en
        // si, no una lista anidada, así que ToDisplayString ya da el
        // texto plano correcto para String/Number/Bool/Nil.
        out += ToDisplayString(list->items[i]);
    }

    Value result = Value::String(out);
    return ToCNew(result);
}

}