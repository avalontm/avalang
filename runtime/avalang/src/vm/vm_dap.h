#ifndef AVA_VM_VM_DAP_H
#define AVA_VM_VM_DAP_H

#include "../../platform/barekernel/stdcompat/ava_stdcompat.h"

namespace ava {

enum class DapStopReason {
    Breakpoint,
    Step,
    Pause,
    Entry,
};

struct DapStopEvent {
    DapStopReason reason;
    size_t frame_index;
    avastd::string source;
    int line;
};

struct DapFrameInfo {
    size_t frame_index;
    avastd::string function_name;
    avastd::string source;
    int line;
    int column;
};

struct DapVariable {
    avastd::string name;
    avastd::string value;
    avastd::string type;
};

}  // namespace ava

#endif  // AVA_VM_VM_DAP_H
