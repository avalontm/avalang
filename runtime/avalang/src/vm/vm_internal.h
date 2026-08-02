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

namespace ava {

// Opcode implementations
void OpAdd(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
void OpSub(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
void OpMul(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
void OpDiv(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
void OpIdiv(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
void OpMod(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
void OpPow(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
void OpNeg(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
void OpNot(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
void OpInc(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
void OpDec(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);

void OpEq(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
void OpEqK(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
void OpNeK(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
void OpNe(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
void OpLt(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
void OpLe(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
void OpGt(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
void OpGe(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);

void OpNewList(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
void OpListAppend(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
void OpNewDict(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
void OpGetIndex(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
void OpSetIndex(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);

void OpNewClass(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
void OpNewInstance(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
void OpGetAttr(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
void OpSetAttr(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);

void OpCall(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
void OpReturn(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
void OpClosure(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
void OpGetUpval(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
void OpSetUpval(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
Value OpBaseCall(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);

void OpSlice(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);

void OpTry(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
void OpTryEnd(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
void OpCatch(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
void OpRaise(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
void OpArgc(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);

void OpYield(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);
void OpResume(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm);

void HandleFrameError(VM& vm, size_t frame_idx, const std::exception& e);

} // namespace ava

#endif // AVA_VM_VM_INTERNAL_H