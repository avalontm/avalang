// Plantilla de runtime empacado -- Fase 7 (filesystem virtual en memoria,
// cero disco. Ver runtime/avapack/README.md y plan_ava_pack.md).
//
// Reemplaza el mecanismo de hooks de Fase 4 (materializar cada import en
// un temp dir, uno a la vez, y borrarlo apenas se lee) por un
// MemoryFileSystem (runtime/avalang/platform/memory/) instalado como
// override del IPlatform activo ANTES de crear el AvaVM
// (VmPlatformAccessor::SetOverride, ver
// runtime/avalang/src/vm/vm_platform_accessor.h). Con eso, ModuleResolver
// (module.cpp) y DoImport (vm_import.cpp) -- que ya solo hablan con
// VmPlatformAccessor::Get().FileSystem(), nunca con la ruta de disco
// directamente -- terminan resolviendo/leyendo cada import contra RAM. En
// ningun momento de la ejecucion existe un .ava/.avbc del proyecto como
// archivo real en disco, ni siquiera por milisegundos.
//
// Flujo:
//   1) Verifica integridad (HMAC-SHA256, igual que Fase 5) y reconstruye
//      la clave AES-256 embebida -- exactamente igual que main.cpp.
//   2) Crea el IPlatform real (platform::Platform::Create()) y un
//      MemoryFileSystem que cae a ESE real como fallback (para que el
//      stdlib real, que sigue viviendo en disco al lado de avalang.dll,
//      se siga resolviendo con normalidad -- solo el proyecto empacado es
//      "zero disk").
//   3) Registra cada EmbeddedFile (salvo el entry, que se maneja aparte,
//      igual que en main.cpp) como un archivo virtual bajo un prefijo
//      sentinela (kVirtualRoot) que NO es una ruta real -- el
//      ContentProvider descifra bajo demanda en cada ReadFile() y nunca
//      cachea el plaintext en el mapa (ver MemoryFileSystem.h).
//   4) Instala el MemoryOverridePlatform via
//      ava::VmPlatformAccessor::SetOverride() -- ANTES de crear el AvaVM.
//   5) Descifra+compila/deserializa el entry en memoria exactamente igual
//      que main.cpp (kEntryIsBytecode / kEntryStringsObfuscated, Fase 6).
//   6) Corre. No hay temp dir, no hay TempDirGuard, no hay hooks
//      before/after de Fase 4 -- no aplican, no se instalan.
//
// Seleccion en build time: runtime/avapack/CMakeLists.txt compila esta
// plantilla en vez de main.cpp cuando AVAPACK_ZERO_DISK=ON (`ava_cli build
// --zero-disk`, ver runtime/avacli/src/build_command.cpp) -- son dos
// binarios mutuamente excluyentes, no coexisten en el mismo .exe.
//
// Limitacion conocida, documentada en README.md: esto protege el
// PROYECTO empacado (los .ava/.avbc embebidos), no cualquier otro archivo
// que el propio codigo del usuario decida leer/escribir explicitamente
// via las funciones de I/O del stdlib -- esas siguen tocando disco real
// (via el fallback del MemoryFileSystem) porque ese es justamente su
// proposito.

#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "avalang.h"
#include "vm/vm.h"
#include "vm/vm_platform_accessor.h"
#include "platform/Platform.h"
#include "platform/memory/MemoryFileSystem.h"
#include "platform/memory/MemoryOverridePlatform.h"

#include "embedded_project.h"
#include "embedded_crypto.h"

namespace {

// Prefijo sentinela usado como "search path" ante ModuleResolver -- NUNCA
// es una ruta real de disco, solo una clave bajo la que se registran los
// archivos virtuales (ver MemoryFileSystem::NormalizeKey). No hace falta
// que exista de verdad: MemoryFileSystem::Exists()/ReadFile() resuelven
// contra su mapa interno antes de siquiera considerar el fallback real.
constexpr const char* kVirtualRoot = "avapack:/vfs";

// Expone los argumentos de linea de comandos del .exe empacado como el
// global `args` (una List de strings) para que el entry .ava los pueda
// leer -- ver el mismo helper en main.cpp para el detalle de por que
// argv[0] queda afuera.
void SetScriptArgsGlobal(ava::VM* raw_vm, int argc, char** argv) {
    auto* list = new ava::ListObj();
    for (int i = 1; i < argc; ++i) {
        list->items.push_back(ava::Value::String(argv[i]));
    }
    ava::Value args_value;
    args_value.type = ava::ValueType::List;
    args_value.obj = list;
    raw_vm->SetGlobal("args", args_value);
}

} // namespace

int main(int argc, char** argv) {
    unsigned char key[32];
    avapack::GetEmbeddedKey(key);

    // Fase 5 (igual que main.cpp): verificacion de integridad antes de
    // descifrar o compilar cualquier cosa.
    if (!avapack::VerifyIntegrity(key)) {
        std::fprintf(stderr,
                      "error: verificacion de integridad fallida -- el contenido embebido "
                      "no coincide con el esperado (binario posiblemente modificado)\n");
        std::memset(key, 0, sizeof(key));
        return 1;
    }

    avapack::FileMap file_map = avapack::BuildFileMap();

    auto entry_it = file_map.find(avapack::kEntryFile);
    if (entry_it == file_map.end()) {
        std::fprintf(stderr, "error: entry file no encontrado entre los archivos embebidos: %s\n",
                     avapack::kEntryFile);
        std::memset(key, 0, sizeof(key));
        return 1;
    }

    // --- Fase 7: instalar el filesystem virtual ANTES de crear el AvaVM ---
    auto real_platform = ava::platform::Platform::Create();
    ava::platform::IFileSystem* real_fs = &real_platform->FileSystem();

    auto memory_fs = std::make_unique<ava::platform::MemoryFileSystem>(real_fs);

    // Cada import se registra con un ContentProvider que captura `key` y
    // el EmbeddedFile* por referencia/puntero -- ambos siguen vivos
    // durante toda la corrida del proceso (key vive en el stack de main,
    // los EmbeddedFile son datos estaticos de embedded_project.cpp). El
    // provider descifra en cada llamada a ReadFile(), nunca precalcula ni
    // cachea el plaintext -- ModuleCache ya se encarga de que cada import
    // solo se lea una vez de todas formas (ver module.cpp).
    for (std::size_t i = 0; i < avapack::kEmbeddedFileCount; ++i) {
        const avapack::EmbeddedFile* f = &avapack::kEmbeddedFiles[i];
        if (std::string(f->path) == avapack::kEntryFile) {
            // El entry se maneja aparte (mas abajo), igual que main.cpp --
            // no pasa por DoImport/ModuleResolver porque es el modulo
            // inicial, no un import.
            continue;
        }
        std::string virtual_path = std::string(kVirtualRoot) + "/" + f->path;
        memory_fs->RegisterFile(virtual_path, static_cast<int64_t>(f->content_len),
                                 [f, &key](std::string& out) -> bool {
                                     std::vector<unsigned char> plaintext = avapack::Decrypt(*f, key);
                                     out.assign(plaintext.begin(), plaintext.end());
                                     if (!plaintext.empty()) {
                                         std::memset(plaintext.data(), 0, plaintext.size());
                                     }
                                     return true;
                                 });
    }

    auto override_platform = std::make_unique<ava::platform::MemoryOverridePlatform>(
        std::move(real_platform), std::move(memory_fs));
    ava::VmPlatformAccessor::SetOverride(std::move(override_platform));

    // El entry file se descifra directo a memoria, igual que main.cpp --
    // nunca pasa por el MemoryFileSystem (no es un import).
    std::vector<unsigned char> entry_plain = avapack::Decrypt(*entry_it->second, key);

    AvaVM* vm = ava_vm_create();
    ava::VM* raw_vm = reinterpret_cast<ava::VM*>(vm);
    // "Search path" virtual -- ModuleResolver arma rutas
    // JoinPath(kVirtualRoot, ...) que MemoryFileSystem sabe resolver
    // contra su mapa (ver MemoryFileSystem::NormalizeKey). No es una ruta
    // real, nunca se crea en disco.
    raw_vm->GetModuleResolver().AddSearchPath(kVirtualRoot);
    SetScriptArgsGlobal(raw_vm, argc, argv);

    // Sin hooks de Fase 4: no hace falta antes/despues de leer un modulo
    // porque nunca se materializa nada en disco para empezar -- el
    // MemoryFileSystem ya resuelve todo en RAM.

    char* error = nullptr;
    AvaModule* module = nullptr;
    if (avapack::kEntryIsBytecode) {
        module = ava_module_deserialize(vm, entry_plain.data(), entry_plain.size(), &error);
        if (!entry_plain.empty()) std::memset(entry_plain.data(), 0, entry_plain.size());
        if (module && avapack::kEntryStringsObfuscated) {
            ava_module_deobfuscate_strings(module, avapack::kEntryObfuscateSeed);
        }
        if (!module) {
            std::fprintf(stderr, "error: entry .avbc invalido: %s\n", error ? error : "unknown error");
            if (error) ava_string_free(error);
            std::memset(key, 0, sizeof(key));
            ava_vm_destroy(vm);
            ava::VmPlatformAccessor::ClearOverride();
            return 1;
        }
    } else {
        std::string entry_source(entry_plain.begin(), entry_plain.end());
        if (!entry_plain.empty()) std::memset(entry_plain.data(), 0, entry_plain.size());
        module = ava_compile(vm, entry_source.c_str(), avapack::kEntryFile, &error);
        entry_source.assign(entry_source.size(), '\0');
        if (!module) {
            std::fprintf(stderr, "compile error: %s\n", error ? error : "unknown error");
            if (error) ava_string_free(error);
            std::memset(key, 0, sizeof(key));
            ava_vm_destroy(vm);
            ava::VmPlatformAccessor::ClearOverride();
            return 1;
        }
    }

    ava_value_t result{};
    ava_run(vm, module, &result, &error);
    if (error) {
        std::fprintf(stderr, "runtime error: %s\n", error);
        ava_string_free(error);
        ava_module_destroy(module);
        std::memset(key, 0, sizeof(key));
        ava_vm_destroy(vm);
        ava::VmPlatformAccessor::ClearOverride();
        return 1;
    }

    ava_module_destroy(module);

    {
        while (raw_vm->HasPendingAsyncWork()) {
            raw_vm->PumpAsyncEvents();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    // Igual que main.cpp: la clave capturada por referencia en los
    // ContentProvider (arriba) puede seguir siendo invocada hasta que la
    // VM termina de correr -- recien aca se borra.
    std::memset(key, 0, sizeof(key));

    ava_vm_destroy(vm);

    // Deshace el override antes de salir -- higiene, no estrictamente
    // necesario porque el proceso termina de todas formas, pero evita
    // dejar un MemoryFileSystem con providers que capturan `key` (ya
    // borrada) accesible si algo mas en el proceso volviera a llamar
    // VmPlatformAccessor::Get() despues de este punto.
    ava::VmPlatformAccessor::ClearOverride();

    return 0;
}
