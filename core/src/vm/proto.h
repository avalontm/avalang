#ifndef AVA_VM_PROTO_H
#define AVA_VM_PROTO_H

#include <string>
#include <vector>
#include <memory>
#include "opcodes.h"
#include "value.h"

#ifdef _WIN32
  #define AVA_PROTO_API __declspec(dllexport)
#else
  #define AVA_PROTO_API __attribute__((visibility("default")))
#endif

namespace ava {

struct UpvalDesc {
    bool from_parent_local;
    uint16_t index;
};

struct AVA_PROTO_API Proto {
    uint16_t num_registers = 0;
    uint8_t  num_params = 0;
    bool     is_vararg = false;
    bool     is_method = false;

    std::vector<Value>      constants;
    std::vector<UpvalDesc>  upvalue_descs;
    std::vector<Instr>      instructions;
    std::vector<std::shared_ptr<Proto>> child_protos;

    std::vector<uint32_t>   debug_lines;
    std::string             debug_name;
};

} // namespace ava

#endif // AVA_VM_PROTO_H
