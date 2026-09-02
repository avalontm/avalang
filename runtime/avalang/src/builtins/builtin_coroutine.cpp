#include "builtin.h"
#include "builtin_shared.h"
#include "../vm/vm.h"
#include "../vm/coroutine.h"

ava_value_t builtin_coroutine(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data) {
    (void)user_data;
    if (count < 1) {
        ava_value_t result;
        result.type = AVA_NIL;
        return result;
    }
    auto* raw_vm = reinterpret_cast<ava::VM*>(vm);
    auto ava_val = ava::FromC(args[0]);

    if (ava_val.type != ava::ValueType::Function) {
        ava_value_t result;
        result.type = AVA_NIL;
        return result;
    }
    
    auto* co = raw_vm->CreateCoroutine(ava_val);
    
    ava_value_t result;
    result.type = AVA_COROUTINE;
    result.as.ref.id = reinterpret_cast<uint64_t>(co);
    return result;
}

ava_value_t builtin_resume(AvaVM* vm, const ava_value_t* args, size_t count, void* user_data) {
    (void)user_data;
    if (count < 1 || args[0].type != AVA_COROUTINE) {
        ava_value_t result;
        result.type = AVA_NIL;
        return result;
    }
    auto* raw_vm = reinterpret_cast<ava::VM*>(vm);
    auto co_val = ava::FromC(args[0]);
    
    avastd::vector<ava::Value> vargs;
    for (size_t i = 1; i < count; ++i) {
        vargs.push_back(ava::FromC(args[i]));
    }
    
    auto result = raw_vm->Call(co_val, vargs);
    // Bug preexistente, mismo patron documentado en builtin_natives.cpp
    // (range()) y en el comentario de ToCNew() en builtin_shared.h: esto
    // usaba ToC(result) en vez de ToCNew(result). `result` es un Value
    // RAII local -- si el coroutine devuelve/yieldea un objeto RECIEN
    // CREADO sin otro dueno (ej. un string armado con `+` dentro del
    // cuerpo de la coroutine DESPUES de reanudar un yield, o cualquier
    // List/Dict/Instance fresca), ToC() (cast plano, no retiene) deja el
    // ava_value_t ya devuelto apuntando al mismo objeto que la
    // destruccion de `result` libera (refcount 1->0, delete) apenas
    // termina esta funcion -- use-after-free confirmado con
    // AddressSanitizer (heap-use-after-free en Retain(), llamado desde
    // FromC() sobre el handle ya colgante, en el primer sitio que
    // consume el valor de retorno de resume()). No se manifestaba en la
    // bateria anterior porque todos los valores devueltos por resume()
    // hasta ahora o bien no eran refcounted (Number) o seguian teniendo
    // otro dueno vivo en otro lado (una constante de string literal en
    // la tabla K[], o una lista armada en vm_call.cpp mismo -- ver nota
    // de diseno mas abajo). Fix: ToCNew() en vez de ToC(), mismo patron
    // ya usado en builtin_sorted/builtin_reversed.
    return ava::ToCNew(result);
}