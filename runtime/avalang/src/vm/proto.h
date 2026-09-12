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
    // Module-scoped global table for protos that belong to an imported
    // module. When non-null, GETGLOBAL/SETGLOBAL read/write this map
    // instead of the VM's shared globals_ (falling back to globals_ for
    // names not present), so one function in a module can see another
    // symbol defined at the module's top level no matter when the calling
    // closure runs (module top-level executes in its own env; see
    // DoImport in vm_import.cpp). Null for the main script / builtins.
    // Shared by a module's top proto and all its child protos via the
    // same shared_ptr, so every closure of the module resolves together.
    avastd::shared_ptr<avastd::unordered_map<avastd::string, Value>> module_globals;

    // Plan de anotaciones (AvaLang_Plan_Anotaciones.md), Fase 2: nombre de
    // la función/método marcado `[entry]` en el programa compilado, o
    // vacío si no hay ninguno (comportamiento actual -- correr el chunk
    // top-level de punta a punta, sin invocar nada extra). Solo se
    // completa en el Proto top-level que devuelve Compiler::Compile();
    // los child_protos (funciones, métodos, lambdas) no llevan copia
    // propia de esto. Compiler::ValidateEntryAttributes (compiler.cpp) ya
    // garantizó que, si esto no está vacío, es la única `[entry]` del
    // programa compilado.
    avastd::string entry_func_name;
    // Vacío si entry_func_name es una función suelta (top-level). Si
    // entry_func_name es un método static, acá va el nombre de la clase
    // dueña -- así la Fase 3 (VM) sabe que tiene que resolverlo via
    // class_obj->attrs en vez de buscarlo como global suelto.
    avastd::string entry_class_name;
};

} // namespace ava

#endif // AVA_VM_PROTO_H
