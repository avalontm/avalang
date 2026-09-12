// Implementacion de avapack::RunPackagedProgram -- ver packaged_runtime.h.
// Extraida palabra por palabra de src/main.cpp (Fases 1-8: temp dir + hooks
// de Fase 4, verificacion de integridad de Fase 5, entry-como-bytecode de
// Fase 6) al moverla a una funcion parametrizada por avapack::PackagedManifest
// en vez de leer los symbols extern (kEmbeddedFiles, kEntryFile, ...)
// directo del namespace -- el comportamiento para src/main.cpp no cambia
// (ver el nuevo main() ahi, que arma el manifest desde esos mismos symbols
// y llama para aca).

#include "packaged_runtime.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "avalang.h"
#include "vm/vm.h"
#include "embedded_crypto.h" // DecryptWith/VerifyIntegrityWith/BuildFileMapFrom (Fase 9)

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace {

fs::path MakeTempDir() {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dist;

    fs::path base = fs::temp_directory_path();
    for (int attempt = 0; attempt < 8; ++attempt) {
        std::ostringstream name;
        name << "avapack_" << std::hex << dist(gen);
        fs::path candidate = base / name.str();
        std::error_code ec;
        if (fs::create_directory(candidate, ec)) {
            return candidate;
        }
    }
    return {};
}

struct TempDirGuard {
    fs::path dir;
    ~TempDirGuard() {
        if (!dir.empty()) {
            std::error_code ec;
            fs::remove_all(dir, ec);
        }
    }
};

#ifdef _WIN32
void MarkTemporary(const fs::path& path) {
    SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_TEMPORARY);
}
#else
void MarkTemporary(const fs::path&) {
    // No implementado fuera de Windows -- ver Fase 8 (multiplataforma),
    // hoy bloqueada por platform/linux|macos siendo stub.
}
#endif

void ZeroAndRemove(const fs::path& path) {
    std::error_code ec;
    auto size = fs::file_size(path, ec);
    if (!ec && size > 0) {
        std::ofstream out(path, std::ios::binary | std::ios::in | std::ios::out);
        if (out) {
            std::vector<char> zeros(static_cast<std::size_t>(size), 0);
            out.write(zeros.data(), zeros.size());
        }
    }
    fs::remove(path, ec);
}

std::string ToRelativePosix(const fs::path& temp_dir, const std::string& resolved_path) {
    std::error_code ec;
    fs::path rel = fs::relative(fs::path(resolved_path), temp_dir, ec);
    if (ec) return "";
    return rel.generic_string();
}

// Descifra un archivo embebido y lo escribe en disco real bajo `out_path`,
// exactamente la misma logica que ya usaba el lambda de
// SetBeforeModuleReadHook mas abajo (factorizada aca para poder llamarla
// tambien antes de compilar el entry -- ver comentario en
// RunPackagedProgram sobre por que hace falta).
void DecryptAndWriteFile(const avapack::EmbeddedFile& file, unsigned char key[32],
                          bool debug_build, const fs::path& out_path) {
    std::vector<unsigned char> plaintext = avapack::DecryptWith(file, key, debug_build);

    std::error_code ec;
    fs::create_directories(out_path.parent_path(), ec);

    std::ofstream out(out_path, std::ios::binary);
    if (out && !plaintext.empty()) {
        out.write(reinterpret_cast<const char*>(plaintext.data()),
                   static_cast<std::streamsize>(plaintext.size()));
    }
    out.close();
    if (!plaintext.empty()) {
        std::memset(plaintext.data(), 0, plaintext.size());
    }
    MarkTemporary(out_path);
}

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

namespace avapack {

int RunPackagedProgram(int argc, char** argv, const PackagedManifest& manifest,
                        unsigned char key[32]) {
    fs::path temp_dir = MakeTempDir();
    if (temp_dir.empty()) {
        std::fprintf(stderr, "error: no se pudo crear directorio temporal\n");
        return 1;
    }
    TempDirGuard temp_guard{temp_dir};

    if (!VerifyIntegrityWith(manifest.files, manifest.file_count, manifest.integrity_mac, key)) {
        std::fprintf(stderr,
                      "error: verificacion de integridad fallida -- el contenido embebido "
                      "no coincide con el esperado (binario posiblemente modificado)\n");
        std::memset(key, 0, 32);
        return 1;
    }

    FileMap file_map = BuildFileMapFrom(manifest.files, manifest.file_count);

    auto entry_it = file_map.find(manifest.entry_file);
    if (entry_it == file_map.end()) {
        std::fprintf(stderr, "error: entry file no encontrado entre los archivos embebidos: %s\n",
                     manifest.entry_file.c_str());
        std::memset(key, 0, 32);
        return 1;
    }
    std::vector<unsigned char> entry_plain = DecryptWith(*entry_it->second, key, manifest.debug_build);

    AvaVM* vm = ava_vm_create();
    ava::VM* raw_vm = reinterpret_cast<ava::VM*>(vm);
    raw_vm->GetModuleResolver().AddSearchPath(temp_dir.string());
    SetScriptArgsGlobal(raw_vm, argc, argv);

    raw_vm->SetBeforeModuleReadHook([&file_map, &key, &temp_dir, &manifest](
                                         const std::string& resolved_path) {
        std::string rel = ToRelativePosix(temp_dir, resolved_path);
        auto it = file_map.find(rel);
        if (it == file_map.end()) {
            return;
        }
        DecryptAndWriteFile(*it->second, key, manifest.debug_build, fs::path(resolved_path));
    });

    raw_vm->SetAfterModuleReadHook([](const std::string& resolved_path) {
        ZeroAndRemove(fs::path(resolved_path));
    });

    char* error = nullptr;
    AvaModule* module = nullptr;
    if (manifest.entry_is_bytecode) {
        module = ava_module_deserialize(vm, entry_plain.data(), entry_plain.size(), &error);
        if (!entry_plain.empty()) std::memset(entry_plain.data(), 0, entry_plain.size());
        if (module && manifest.entry_strings_obfuscated) {
            ava_module_deobfuscate_strings(module, manifest.entry_obfuscate_seed);
        }
        if (!module) {
            std::fprintf(stderr, "error: entry .avbc invalido: %s\n", error ? error : "unknown error");
            if (error) ava_string_free(error);
            std::memset(key, 0, 32);
            ava_vm_destroy(vm);
            return 1;
        }
    } else {
        // Bug real (encontrado esta sesion): Compiler::RegisterImportedClasses
        // (compiler.cpp) -- el harvesting estatico de clases para el chequeo
        // de 'new' -- corre DURANTE ava_compile() del entry, es decir ANTES
        // de que el entry ejecute ningun `import` en runtime. Pero los
        // siblings (ej. app.ava) solo se descifran y escriben a disco real
        // de forma perezosa, en SetBeforeModuleReadHook, disparado recien
        // cuando el VM resuelve ESE import durante la ejecucion -- que
        // todavia no paso en este punto. Entonces ResolveSiblingImportFile
        // (via VmPlatformAccessor, disco real en este target -- este
        // ejecutable NO usa MemoryOverridePlatform, eso es exclusivo de
        // --zero-disk/main_zerodisk.cpp) siempre encontraba "no existe" y
        // `new app()` fallaba con "'app' is not a class" pese a que el
        // import se resuelva perfecto despues.
        //
        // Ademas -- segundo bug independiente que este solo no alcanzaba
        // para arreglar -- el `source_name` pasado a ava_compile era
        // `manifest.entry_file` a secas ("main.ava", sin ruta), asi que
        // Compiler::current_file_dir_ quedaba vacio y
        // ResolveSiblingImportFile buscaba el sibling relativo al cwd real
        // del proceso empacado (donde el usuario corrio el .exe), no
        // relativo a `temp_dir` (donde los siblings realmente se
        // materializan). Aunque los siblings ya hubiesen existido en disco,
        // se los buscaba en el lugar equivocado.
        //
        // Fix: materializar TODOS los archivos embebidos en temp_dir de
        // una sola vez, antes de compilar el entry (no cambia ninguna
        // garantia de este modo -- a diferencia de --zero-disk, este
        // camino siempre escribio plano a disco real para cada import; solo
        // se adelanta el momento). Y pasar como source_name la ruta
        // absoluta del entry YA DENTRO de temp_dir, para que
        // current_file_dir_ apunte al lugar correcto. El hook de arriba
        // sigue re-escribiendo/borrando cada archivo al importarlo de
        // verdad en runtime (ver DecryptAndWriteFile/ZeroAndRemove) -- este
        // adelanto no cambia ese comportamiento, solo el timing para el
        // chequeo estatico de clases.
        for (const auto& [rel, file] : file_map) {
            DecryptAndWriteFile(*file, key, manifest.debug_build, temp_dir / rel);
        }

        std::string entry_source(entry_plain.begin(), entry_plain.end());
        if (!entry_plain.empty()) std::memset(entry_plain.data(), 0, entry_plain.size());
        std::string entry_source_name = (temp_dir / manifest.entry_file).string();
        module = ava_compile(vm, entry_source.c_str(), entry_source_name.c_str(), &error);
        entry_source.assign(entry_source.size(), '\0');
        if (!module) {
            std::fprintf(stderr, "compile error: %s\n", error ? error : "unknown error");
            if (error) ava_string_free(error);
            std::memset(key, 0, 32);
            ava_vm_destroy(vm);
            return 1;
        }
    }

    ava_value_t result{};
    ava_run(vm, module, &result, &error);
    if (error) {
        std::fprintf(stderr, "runtime error: %s\n", error);
        ava_string_free(error);
        std::memset(key, 0, 32);
        ava_vm_destroy(vm);
        ava_module_destroy(module);
        return 1;
    }

    {
        while (raw_vm->HasPendingAsyncWork()) {
            raw_vm->PumpAsyncEvents();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    std::memset(key, 0, 32);

    ava_vm_destroy(vm);
    ava_module_destroy(module);
    return 0;
}

} // namespace avapack
