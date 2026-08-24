#ifndef AVA_VM_PROTO_H
#define AVA_VM_PROTO_H

#include "../../platform/barekernel/stdcompat/ava_stdcompat.h"
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
    avastd::uint16_t index;
};

struct AVA_PROTO_API Proto {
    avastd::uint16_t num_registers = 0;
    avastd::uint8_t  num_params = 0;
    bool     is_vararg = false;
    bool     is_method = false;
    bool     is_async = false;

    avastd::vector<Value>      constants;
    avastd::vector<UpvalDesc>  upvalue_descs;
    avastd::vector<Instr>      instructions;
    avastd::vector<avastd::shared_ptr<Proto>> child_protos;

    avastd::vector<avastd::uint32_t>   debug_lines;
    // Columna (1-based) paralela a debug_lines; 0 = desconocida. Igual
    // que debug_lines, cada Emit() empuja una entrada -- ver
    // Compiler::Emit (compiler.cpp) y Compiler::current_col_.
    avastd::vector<avastd::uint32_t>   debug_columns;
    avastd::string             debug_name;
    // Path of the source file this Proto was compiled from (top-level
    // script, or the module a function/method/lambda was defined in).
    // Empty for Protos compiled before this field existed. Used to
    // report the correct file when an error happens inside an imported
    // module, not just a line number (see vm.cpp).
    avastd::string             source_name;
};

} // namespace ava

#endif // AVA_VM_PROTO_H
