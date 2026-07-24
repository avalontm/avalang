#include <cstdio>
#include <fstream>
#include <sstream>
#include "avalang.h"
#include "vm/vm.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <script.ava>\n", argv[0]);
        return 1;
    }

    std::ifstream file(argv[1]);
    if (!file) {
        std::fprintf(stderr, "error: could not open %s\n", argv[1]);
        return 1;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();

    AvaVM* vm = ava_vm_create();
    
    {
        std::string script_dir = argv[1];
        size_t sep = script_dir.find_last_of("/\\");
        if (sep != std::string::npos) {
            script_dir = script_dir.substr(0, sep);
            ava::VM* raw_vm = reinterpret_cast<ava::VM*>(vm);
            raw_vm->GetModuleResolver().AddSearchPath(script_dir);
        }
    }

    char* error = nullptr;
    AvaModule* module = ava_compile(vm, buffer.str().c_str(), argv[1], &error);
    if (!module) {
        std::fprintf(stderr, "compile error: %s\n", error ? error : "unknown error");
        if (error) ava_string_free(error);
        ava_vm_destroy(vm);
        return 1;
    }

    ava_value_t result{};
    ava_run(vm, module, &result, &error);
    if (error) {
        std::fprintf(stderr, "runtime error: %s\n", error);
        ava_string_free(error);
        ava_module_destroy(module);
        ava_vm_destroy(vm);
        return 1;
    }

    ava_module_destroy(module);
    ava_vm_destroy(vm);
    return 0;
}