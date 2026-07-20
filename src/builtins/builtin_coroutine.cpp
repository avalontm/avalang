#include "builtin.h"
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
    
    std::vector<ava::Value> vargs;
    for (size_t i = 1; i < count; ++i) {
        vargs.push_back(ava::FromC(args[i]));
    }
    
    auto result = raw_vm->Call(co_val, vargs);
    return ava::ToC(result);
}