#include <cstdio>
#include <fstream>
#include "vm/vm.h"
#include "vm/proto.h"
#include "builtins/builtin.h"
#include "compiler/proto_io.h"

using namespace ava;

namespace {

avastd::shared_ptr<Proto> BuildHelloProto() {
    auto proto = avastd::make_shared<Proto>();
    proto->num_registers = 2;
    proto->num_params = 0;
    proto->is_vararg = false;
    proto->is_method = false;
    proto->constants.push_back(Value::String("print"));
    proto->constants.push_back(Value::String("Hello from BareKernel!"));
    proto->instructions.push_back(Instr{OpCode::GETGLOBAL, 0, {0, 0}});
    proto->instructions.push_back(Instr{OpCode::LOADK, 1, {1, 0}});
    proto->instructions.push_back(Instr{OpCode::CALL, 0, {1, 0}});
    proto->instructions.push_back(Instr{OpCode::RETURN, 0, {0, 0}});
    proto->debug_lines = {1, 1, 1, 1};
    proto->debug_name = "main";
    proto->source_name = "barekernel_sample.ava";
    return proto;
}

}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "uso: gen_barekernel_sample <archivo_salida.avb>\n");
        return 1;
    }

    auto proto = BuildHelloProto();

    VM vm;
    RegisterBuiltinMethods(reinterpret_cast<AvaVM*>(&vm));
    RegisterBuiltinGlobals(reinterpret_cast<AvaVM*>(&vm));
    vm.Run(proto);

    ProtoIoOptions options;
    options.strip_debug_info = false;
    avastd::vector<uint8_t> bytes = SerializeProto(*proto, options);

    avastd::string error_out;
    auto roundtrip = DeserializeProto(bytes, &error_out);
    if (!roundtrip) {
        std::fprintf(stderr, "round-trip fallo: %s\n", error_out.c_str());
        return 1;
    }
    VM vm2;
    RegisterBuiltinMethods(reinterpret_cast<AvaVM*>(&vm2));
    RegisterBuiltinGlobals(reinterpret_cast<AvaVM*>(&vm2));
    vm2.Run(roundtrip);

    std::ofstream out(argv[1], std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    out.close();

    std::fprintf(stderr, "OK: %zu bytes escritos en %s\n", bytes.size(), argv[1]);
    return 0;
}
