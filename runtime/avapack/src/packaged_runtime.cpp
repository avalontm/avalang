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
        std::vector<unsigned char> plaintext = DecryptWith(*it->second, key, manifest.debug_build);

        fs::path out_path(resolved_path);
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
        std::string entry_source(entry_plain.begin(), entry_plain.end());
        if (!entry_plain.empty()) std::memset(entry_plain.data(), 0, entry_plain.size());
        module = ava_compile(vm, entry_source.c_str(), manifest.entry_file.c_str(), &error);
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
