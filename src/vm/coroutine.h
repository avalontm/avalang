#ifndef AVA_VM_COROUTINE_H
#define AVA_VM_COROUTINE_H

#include <vector>
#include <memory>
#include <string>

#include "value.h"

namespace ava {

struct Proto;
struct Closure;

struct CallFrame {
    std::shared_ptr<Proto> proto;
    std::shared_ptr<Closure> closure;
    std::vector<Value> registers;
    uint32_t pc = 0;
    std::string module_dir;
    int ret_slot = -1;  // Register in caller frame to write return value to; -1 = discard
};

enum class CoStatus { Suspended, Running, Dead };

struct Coroutine {
    Value entry;
    CoStatus status = CoStatus::Suspended;
    std::vector<CallFrame> frames;
    std::vector<Value> yielded_values;
    Coroutine() : status(CoStatus::Suspended) {}
};

} // namespace ava

#endif // AVA_VM_COROUTINE_H
