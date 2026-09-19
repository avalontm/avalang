#include "vm.h"
#include "vm_internal.h"
#include "vm_helpers.h"

namespace ava {

avastd::string VM::DapNormalizeFileKey(const avastd::string& path) {
    avastd::string normalized = path;
    for (size_t i = 0; i < normalized.size(); ++i) {
        if (normalized[i] == '\\') normalized[i] = '/';
    }
    size_t slash = normalized.find_last_of('/');
    return slash == avastd::string::npos ? normalized : normalized.substr(slash + 1);
}

void VM::DapSetBreakpoints(const avastd::string& file, const avastd::vector<int>& lines) {
    avastd::lock_guard<avastd::mutex> lock(dap_mutex_);
    dap_breakpoints_[DapNormalizeFileKey(file)] = lines;
}

bool VM::DapHasBreakpointAt(const avastd::string& source, int line) const {
    auto it = dap_breakpoints_.find(DapNormalizeFileKey(source));
    if (it == dap_breakpoints_.end()) return false;
    for (int bp_line : it->second) {
        if (bp_line == line) return true;
    }
    return false;
}

void VM::DapContinue() {
    avastd::lock_guard<avastd::mutex> lock(dap_mutex_);
    dap_step_kind_ = DapStepKind::None;
    dap_paused_ = false;
    dap_cv_.notify_all();
}

void VM::DapStepOver() {
    avastd::lock_guard<avastd::mutex> lock(dap_mutex_);
    dap_step_kind_ = DapStepKind::Over;
    dap_step_start_depth_ = dap_stopped_depth_;
    dap_step_start_line_ = dap_stopped_line_;
    dap_paused_ = false;
    dap_cv_.notify_all();
}

void VM::DapStepInto() {
    avastd::lock_guard<avastd::mutex> lock(dap_mutex_);
    dap_step_kind_ = DapStepKind::Into;
    dap_step_start_depth_ = dap_stopped_depth_;
    dap_step_start_line_ = dap_stopped_line_;
    dap_paused_ = false;
    dap_cv_.notify_all();
}

void VM::DapStepOut() {
    avastd::lock_guard<avastd::mutex> lock(dap_mutex_);
    dap_step_kind_ = DapStepKind::Out;
    dap_step_start_depth_ = dap_stopped_depth_;
    dap_step_start_line_ = dap_stopped_line_;
    dap_paused_ = false;
    dap_cv_.notify_all();
}

void VM::DapRequestPause() {
    avastd::lock_guard<avastd::mutex> lock(dap_mutex_);
    dap_pause_requested_ = true;
}

void VM::DapOnBeforeInstruction(size_t frame_idx) {
    if (!dap_hook_enabled_) return;
    if (frame_idx >= frames_.size()) return;

    CallFrame& frame = frames_[frame_idx];
    if (!frame.proto) return;

    size_t pc = frame.pc;
    if (pc >= frame.proto->debug_lines.size()) return;
    int line = static_cast<int>(frame.proto->debug_lines[pc]);
    size_t depth = frames_.size();

    avastd::unique_lock<avastd::mutex> lock(dap_mutex_);

    bool should_stop = false;
    DapStopReason reason = DapStopReason::Step;

    if (dap_pause_requested_) {
        dap_pause_requested_ = false;
        should_stop = true;
        reason = DapStopReason::Pause;
    } else if (dap_step_kind_ == DapStepKind::Over) {
        if (depth <= dap_step_start_depth_ &&
            (depth < dap_step_start_depth_ || line != dap_step_start_line_)) {
            should_stop = true;
        }
    } else if (dap_step_kind_ == DapStepKind::Into) {
        if (depth != dap_step_start_depth_ || line != dap_step_start_line_) {
            should_stop = true;
        }
    } else if (dap_step_kind_ == DapStepKind::Out) {
        if (depth < dap_step_start_depth_) {
            should_stop = true;
        }
    }

    if (!should_stop && DapHasBreakpointAt(frame.proto->source_name, line)) {
        should_stop = true;
        reason = DapStopReason::Breakpoint;
    }

    if (!should_stop) return;

    dap_step_kind_ = DapStepKind::None;
    dap_stopped_line_ = line;
    dap_stopped_depth_ = depth;
    dap_paused_ = true;

    DapStopEvent event;
    event.reason = reason;
    event.frame_index = frame_idx;
    event.source = frame.proto->source_name;
    event.line = line;

    DapStoppedSink sink = dap_stopped_sink_;

    lock.unlock();
    if (sink) sink(event);
    lock.lock();

    dap_cv_.wait(lock, [this] { return !dap_paused_; });
}

avastd::vector<DapFrameInfo> VM::DapCaptureFrames() const {
    avastd::vector<DapFrameInfo> out;
    for (size_t i = frames_.size(); i-- > 0; ) {
        const CallFrame& frame = frames_[i];
        DapFrameInfo info;
        info.frame_index = i;
        info.function_name = frame.proto ? frame.proto->debug_name : avastd::string();
        info.source = frame.proto ? frame.proto->source_name : avastd::string();

        size_t instr_idx = (i + 1 == frames_.size()) ? frame.pc
            : (frame.pc > 0 ? frame.pc - 1 : 0);
        if (frame.proto && instr_idx < frame.proto->debug_lines.size()) {
            info.line = static_cast<int>(frame.proto->debug_lines[instr_idx]);
            info.column = instr_idx < frame.proto->debug_columns.size()
                ? static_cast<int>(frame.proto->debug_columns[instr_idx]) : 0;
        } else {
            info.line = 0;
            info.column = 0;
        }
        out.push_back(info);
    }
    return out;
}

avastd::vector<DapVariable> VM::DapCaptureLocals(size_t frame_index) const {
    avastd::vector<DapVariable> out;
    if (frame_index >= frames_.size()) return out;

    const CallFrame& frame = frames_[frame_index];
    for (size_t r = 0; r < frame.registers.size(); ++r) {
        DapVariable var;
        var.name = (frame.proto && frame.proto->is_method && r == 0)
            ? avastd::string("this")
            : avastd::string("r") + avastd::to_string(static_cast<long long>(r));
        var.value = ValueToString(frame.registers[r]);
        var.type = ValueTypeName(frame.registers[r].type);
        out.push_back(var);
    }
    return out;
}

}  // namespace ava
