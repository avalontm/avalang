#ifndef AVA_VM_VM_INTERNAL_H
#define AVA_VM_VM_INTERNAL_H

#include "vm.h"
#include "opcodes.h"
#include "value.h"
#include "proto.h"
#include "closure.h"
#include "coroutine.h"
#include "module.h"
#include "vm_helpers.h"
#include "../common/ava_error.h"

namespace ava {

struct AvaRaiseException : public avastd::exception {
    const char* what() const noexcept override { return "ava raise"; }
#if !AVA_HAVE_EXCEPTIONS
    int ava_type_tag() const noexcept override { return 1; }
#endif
};

void OpAdd(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
void OpSub(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
void OpMul(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
void OpDiv(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
void OpIdiv(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
void OpMod(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
void OpPow(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
void OpBand(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
void OpBor(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
void OpBxor(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
void OpShl(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
void OpShr(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
void OpNeg(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
void OpNot(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
void OpBnot(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
void OpInc(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
void OpDec(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);

void OpEq(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
void OpEqK(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
void OpNeK(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
void OpNe(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
void OpLt(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
void OpLe(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
void OpGt(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
void OpGe(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);

void OpNewList(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
void OpListAppend(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
void OpNewDict(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
void OpGetIndex(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
void OpSetIndex(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);

void OpNewClass(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
void OpNewInstance(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
void OpGetAttr(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
void OpSetAttr(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);

Value OpBaseCall(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);

void OpSlice(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);

void OpTry(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
void OpTryEnd(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
void OpCatch(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
void OpRaise(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
void OpArgc(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);

void OpYield(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
void OpResume(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);
void OpAwait(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm);

void HandleFrameError(VM& vm, size_t frame_idx, const avastd::exception& e);

AvaError MakeFrameError(const CallFrame& frame, const avastd::string& message);

// Construye el AvaError para un CALL/BASECALL cuyo callee no es
// invocable. Si el callee se puede rastrear hasta un identificador
// directo (una variable global, opcionalmente con un `.attr`, como
// `Console` o `Console.WriteLine`) el mensaje lo nombra explicitamente;
// si ademas ese identificador raiz coincide con lo que expone un modulo
// nativo registrado (ver VM::FindNativeModuleExporting), sugiere el
// `import` que falta. Sin rastro reconstruible, cae al mensaje generico
// de siempre ("non-callable value (type=N)").
AvaError MakeNonCallableError(VM& vm, const CallFrame& frame, const Value& callee);

} // namespace ava

#endif // AVA_VM_VM_INTERNAL_H