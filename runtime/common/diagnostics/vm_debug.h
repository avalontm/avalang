#pragma once

#include <cstdio>
#include <string>

#include "avalang.h"
#include "diagnostics/debug_mode.h"

namespace ava {
namespace diag {

inline void ApplyDebugMode(AvaVM* vm) {
    ava_vm_set_debug_mode(vm, DebugRuntimeEnabled() ? 1 : 0);
}

inline std::string CaptureDebugStack(AvaVM* vm) {
    if (!DebugRuntimeEnabled()) return std::string();
    char* raw = ava_last_error_stack(vm);
    std::string stack = raw ? raw : "";
    if (raw) ava_string_free(raw);
    return stack;
}

inline void PrintStackTrace(const std::string& stack) {
    if (stack.empty()) return;
    std::fprintf(stderr, "stack traceback:\n%s\n", stack.c_str());
}

}  // namespace diag
}  // namespace ava
