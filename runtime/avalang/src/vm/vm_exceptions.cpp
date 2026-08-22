#include "vm.h"
#include "vm_internal.h"

namespace ava {

void OpTry(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm) {
    ExceptionHandler handler;
    handler.catch_pc = frame.pc + in.bx32;
    handler.frame_idx = vm.GetCurrentFrameIndex();
    vm.exception_handlers_.push_back(handler);
}

void OpTryEnd(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm) {
    if (!vm.exception_handlers_.empty()) {
        vm.exception_handlers_.pop_back();
    }
}

void OpCatch(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm) {
    if (vm.HasException()) {
        Value exc = vm.GetAndClearException();
        vm.SetGlobal("__exception__", exc);
    } else {
        frame.pc = static_cast<uint32_t>(static_cast<int32_t>(frame.pc) + in.bx32);
    }
}

void OpRaise(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm) {
    vm.RaiseException(frame.registers[in.a]);
    AVA_THROW(AvaRaiseException());
}

void OpArgc(CallFrame& frame, const Instr& in, const avastd::vector<Value>& K, VM& vm) {
    frame.registers[in.a] = Value::Number(static_cast<double>(frame.argc));
}

} // namespace ava
