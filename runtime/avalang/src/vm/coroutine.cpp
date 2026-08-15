#include "vm.h"
#include "vm_internal.h"
#include "coroutine.h"
#include <algorithm>

namespace ava {

void OpYield(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm) {
    if (vm.coroutine_resumers_.empty()) {
        throw std::runtime_error("attempt to yield outside of coroutine");
    }

    Value result = Value::Nil();
    if (in.b > 0 && in.a < frame.registers.size()) {
        std::vector<Value> yielded;
        for (uint8_t i = 0; i < in.b && (in.a + i) < frame.registers.size(); ++i) {
            yielded.push_back(frame.registers[in.a + i]);
        }
        auto* list = new ListObj();
        list->items = std::move(yielded);
        result.type = ValueType::List;
        result.obj = list;
    }

    vm.is_coroutine_suspended_ = true;
    frame.registers[in.a] = result;
}

void OpResume(CallFrame& frame, const Instr& in, const std::vector<Value>& K, VM& vm) {
    size_t frame_idx = static_cast<size_t>(&frame - vm.frames_.data());
    auto& co_val = frame.registers[in.b];
    if (co_val.type != ValueType::Coroutine) {
        throw std::runtime_error("attempt to resume a non-coroutine");
    }
    auto* co = reinterpret_cast<Coroutine*>(co_val.obj);
    if (co->status == CoStatus::Dead) {
        throw std::runtime_error("attempt to resume a dead coroutine");
    }
    if (co->status == CoStatus::Running) {
        throw std::runtime_error("attempt to resume a running coroutine");
    }

    std::vector<Value> args(
        frame.registers.begin() + in.b + 1,
        frame.registers.begin() + in.b + 1 + in.c);

    co->status = CoStatus::Running;
    vm.coroutine_resumers_.push_back(vm.current_coroutine_);

    vm.saved_frames_ = vm.frames_;
    std::swap(vm.frames_, co->frames);
    vm.current_coroutine_ = co;

    if (vm.frames_.empty() || !vm.frames_[0].proto) {
        auto* closure = static_cast<Closure*>(co->entry.obj);
        CallFrame resume_frame;
        resume_frame.proto = closure->proto;
        resume_frame.closure = std::shared_ptr<Closure>(closure, [](Closure*) {});
        resume_frame.registers.resize(closure->proto->num_registers);
        vm.frames_.push_back(resume_frame);
    }
    if (vm.frames_[0].registers.size() < args.size()) {
        vm.frames_[0].registers.resize(args.size());
    }
    for (size_t i = 0; i < args.size() && i < vm.frames_[0].registers.size(); ++i) {
        vm.frames_[0].registers[i] = args[i];
    }
    vm.frames_[0].argc = static_cast<uint32_t>(args.size());

    vm.is_coroutine_suspended_ = false;
    Value result = vm.ResumeFromTop();

    vm.current_coroutine_ = vm.coroutine_resumers_.back();
    vm.coroutine_resumers_.pop_back();

    co->frames = vm.frames_;
    co->status = vm.is_coroutine_suspended_ ? CoStatus::Suspended : CoStatus::Dead;
    vm.frames_ = vm.saved_frames_;

    vm.is_coroutine_suspended_ = false;

    vm.frames_[frame_idx].registers[in.a] = result;
}

} // namespace ava