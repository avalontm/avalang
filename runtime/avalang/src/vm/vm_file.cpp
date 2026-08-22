#include "vm.h"
#include "vm_internal.h"
#include "vm_platform_accessor.h"
#include "../frontend/frontend.h"
#include "../../platform/barekernel/stdcompat/ava_stdcompat.h"

namespace ava {

avastd::string VM::GetCurrentDir() const {
    if (!current_dir_.empty()) return current_dir_;
    return GetCurrentWorkingDir();
}

void VM::SetCurrentDir(const avastd::string& dir) {
    current_dir_ = dir;
}

// Antes usaba std::ifstream + std::stringstream para leer el archivo
// entero a memoria (patron "abrir, volcar a stringstream, .str()").
// std::ifstream es I/O real de sistema operativo -- en este kernel eso
// tiene que pasar por la PAL (VmPlatformAccessor::Get().FileSystem(),
// implementada por BareKernelFileSystem para este target), no por
// libstdc++ directo, que ademas ni esta disponible aca. El resto de esta
// funcion (compilar + correr el modulo, manejo de current_dir_) no
// cambia -- solo la lectura del archivo.
Value VM::RunFile(const avastd::string& file_path) {
    avastd::string source;
    if (!VmPlatformAccessor::Get().FileSystem().ReadFile(file_path, source)) {
        AVA_THROW(avastd::runtime_error("could not open file: " + file_path));
    }

    avastd::string dir = GetFileDir(file_path);
    avastd::string prev_dir = GetCurrentDir();
    SetCurrentDir(dir);

    current_module_ = file_path;

    AVA_TRY {
        auto proto = CompileSource(source, file_path);
        auto result = Run(proto);
        SetCurrentDir(prev_dir);
        return result;
    } AVA_CATCH(avastd::exception, e) {
        (void)e;
        SetCurrentDir(prev_dir);
        AVA_RETHROW();
    }
}

} // namespace ava
