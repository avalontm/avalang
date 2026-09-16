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
#include "embedded_crypto.h"

#ifdef _WIN32
#include <windows.h>
#endif

#if defined(_WIN32) && defined(AVAPACK_HAS_DESKTOP_UI_HOST)
#include "native/native_app_host.h"
#include "platform/windows/WinBackendEntry.h"
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

#ifdef _WIN32
void MarkTemporary(const fs::path& path) {
    SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_TEMPORARY);
}
#else
void MarkTemporary(const fs::path&) {}
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

struct PreparedModule {
    AvaVM* vm = nullptr;
    ava::VM* raw_vm = nullptr;
    AvaModule* module = nullptr;
    bool ok = false;
    std::string error;
};

PreparedModule PrepareAndCompile(int argc, char** argv, const avapack::PackagedManifest& manifest,
                                  unsigned char key[32], const fs::path& temp_dir,
                                  avapack::FileMap& file_map) {
    PreparedModule result;

    if (!avapack::VerifyIntegrityWith(manifest.files, manifest.file_count, manifest.integrity_mac, key)) {
        result.error = "verificacion de integridad fallida -- el contenido embebido no coincide "
                        "con el esperado (binario posiblemente modificado)";
        return result;
    }

    file_map = avapack::BuildFileMapFrom(manifest.files, manifest.file_count);

    auto entry_it = file_map.find(manifest.entry_file);
    if (entry_it == file_map.end()) {
        result.error = "entry file no encontrado entre los archivos embebidos: " + manifest.entry_file;
        return result;
    }
    std::vector<unsigned char> entry_plain = avapack::DecryptWith(*entry_it->second, key, manifest.debug_build);

    AvaVM* vm = ava_vm_create();
    ava::VM* raw_vm = reinterpret_cast<ava::VM*>(vm);
    raw_vm->GetModuleResolver().AddSearchPath(temp_dir.string());
    SetScriptArgsGlobal(raw_vm, argc, argv);

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
            result.error = std::string("entry .avbc invalido: ") + (error ? error : "unknown error");
            if (error) ava_string_free(error);
            ava_vm_destroy(vm);
            return result;
        }
    } else {
        for (const auto& [rel, file] : file_map) {
            DecryptAndWriteFile(*file, key, manifest.debug_build, temp_dir / rel);
        }

        std::string entry_source(entry_plain.begin(), entry_plain.end());
        if (!entry_plain.empty()) std::memset(entry_plain.data(), 0, entry_plain.size());
        std::string entry_source_name = (temp_dir / manifest.entry_file).string();
        module = ava_compile(vm, entry_source.c_str(), entry_source_name.c_str(), &error);
        entry_source.assign(entry_source.size(), '\0');
        if (!module) {
            result.error = std::string("compile error: ") + (error ? error : "unknown error");
            if (error) ava_string_free(error);
            ava_vm_destroy(vm);
            return result;
        }
    }

    result.vm = vm;
    result.raw_vm = raw_vm;
    result.module = module;
    result.ok = true;
    return result;
}

} // namespace

namespace avapack {

struct PackagedInstance {
    fs::path temp_dir;
    FileMap file_map;
    std::string entry_file;
    bool debug_build = false;
    unsigned char key[32] = {0};
    AvaVM* vm = nullptr;
    ava::VM* raw_vm = nullptr;
    AvaModule* module = nullptr;
};

int RunPackagedProgram(int argc, char** argv, const PackagedManifest& manifest,
                        unsigned char key[32]) {
    fs::path temp_dir = MakeTempDir();
    if (temp_dir.empty()) {
        std::fprintf(stderr, "error: no se pudo crear directorio temporal\n");
        return 1;
    }

    FileMap file_map;
    PreparedModule prepared = PrepareAndCompile(argc, argv, manifest, key, temp_dir, file_map);
    if (!prepared.ok) {
        std::fprintf(stderr, "error: %s\n", prepared.error.c_str());
        std::memset(key, 0, 32);
        std::error_code ec;
        fs::remove_all(temp_dir, ec);
        return 1;
    }

    prepared.raw_vm->SetBeforeModuleReadHook(
        [&file_map, &key, &temp_dir, &manifest](const std::string& resolved_path) {
            std::string rel = ToRelativePosix(temp_dir, resolved_path);
            auto it = file_map.find(rel);
            if (it == file_map.end()) return;
            DecryptAndWriteFile(*it->second, key, manifest.debug_build, fs::path(resolved_path));
        });

    ava_value_t result{};
    char* error = nullptr;
    ava_run(prepared.vm, prepared.module, &result, &error);
    int exit_code = 0;
    if (error) {
        std::fprintf(stderr, "runtime error: %s\n", error);
        ava_string_free(error);
        exit_code = 1;
    } else {
        while (prepared.raw_vm->HasPendingAsyncWork()) {
            prepared.raw_vm->PumpAsyncEvents();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    std::memset(key, 0, 32);
    ava_vm_destroy(prepared.vm);
    ava_module_destroy(prepared.module);
    std::error_code ec;
    fs::remove_all(temp_dir, ec);
    return exit_code;
}

PackagedInstance* LoadPackagedProgram(int argc, char** argv, const PackagedManifest& manifest,
                                       unsigned char key[32], std::string* out_error) {
    fs::path temp_dir = MakeTempDir();
    if (temp_dir.empty()) {
        if (out_error) *out_error = "no se pudo crear directorio temporal";
        return nullptr;
    }

    auto* instance = new PackagedInstance();
    instance->temp_dir = temp_dir;
    instance->entry_file = manifest.entry_file;
    instance->debug_build = manifest.debug_build;
    std::memcpy(instance->key, key, 32);

    PreparedModule prepared = PrepareAndCompile(argc, argv, manifest, key, temp_dir, instance->file_map);
    if (!prepared.ok) {
        if (out_error) *out_error = prepared.error;
        std::error_code ec;
        fs::remove_all(temp_dir, ec);
        delete instance;
        return nullptr;
    }

    instance->vm = prepared.vm;
    instance->raw_vm = prepared.raw_vm;
    instance->module = prepared.module;

    instance->raw_vm->SetBeforeModuleReadHook(
        [instance](const std::string& resolved_path) {
            std::string rel = ToRelativePosix(instance->temp_dir, resolved_path);
            auto it = instance->file_map.find(rel);
            if (it == instance->file_map.end()) return;
            DecryptAndWriteFile(*it->second, instance->key, instance->debug_build, fs::path(resolved_path));
        });

    ava_value_t result{};
    char* error = nullptr;
    ava_run(instance->vm, instance->module, &result, &error);
    if (error) {
        if (out_error) *out_error = std::string("runtime error: ") + error;
        ava_string_free(error);
        std::memset(instance->key, 0, 32);
        ava_vm_destroy(instance->vm);
        ava_module_destroy(instance->module);
        std::error_code ec;
        fs::remove_all(instance->temp_dir, ec);
        delete instance;
        return nullptr;
    }

    while (instance->raw_vm->HasPendingAsyncWork()) {
        instance->raw_vm->PumpAsyncEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    return instance;
}

AvaVM* PackagedInstanceVM(PackagedInstance* instance) {
    return instance ? instance->vm : nullptr;
}

void UnloadPackagedProgram(PackagedInstance* instance) {
    if (!instance) return;

    while (instance->raw_vm->HasPendingAsyncWork()) {
        instance->raw_vm->PumpAsyncEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    std::memset(instance->key, 0, 32);
    ava_vm_destroy(instance->vm);
    ava_module_destroy(instance->module);
    std::error_code ec;
    fs::remove_all(instance->temp_dir, ec);
    delete instance;
}

#if defined(_WIN32) && defined(AVAPACK_HAS_DESKTOP_UI_HOST)

int RunPackagedNativeApp(int argc, char** argv, const PackagedManifest& manifest,
                          unsigned char key[32]) {
    if (manifest.entry_is_bytecode) {

        std::memset(key, 0, 32);
        MessageBoxA(nullptr,
                     "avapack: --obfuscate no esta soportado todavia para apps Desktop UI "
                     "(--with-ui --target desktop sin --output-kind library) -- empaqueta "
                     "sin --obfuscate mientras tanto.",
                     "Avalang", MB_OK | MB_ICONERROR);
        return 1;
    }

    if (!avapack::VerifyIntegrityWith(manifest.files, manifest.file_count, manifest.integrity_mac, key)) {
        std::memset(key, 0, 32);
        MessageBoxA(nullptr,
                     "avapack: verificacion de integridad fallida -- el contenido embebido no "
                     "coincide con el esperado (binario posiblemente modificado)",
                     "Avalang", MB_OK | MB_ICONERROR);
        return 1;
    }

    fs::path temp_dir = MakeTempDir();
    if (temp_dir.empty()) {
        std::memset(key, 0, 32);
        MessageBoxA(nullptr, "avapack: no se pudo crear directorio temporal", "Avalang",
                     MB_OK | MB_ICONERROR);
        return 1;
    }

    avapack::FileMap file_map = avapack::BuildFileMapFrom(manifest.files, manifest.file_count);
    for (const auto& [rel, file] : file_map) {
        DecryptAndWriteFile(*file, key, manifest.debug_build, temp_dir / rel);
    }
    std::memset(key, 0, 32);

    avalang::ui::platform::windows::LinkBackend();

    int width = 1024;
    int height = 720;
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::string(argv[i]) == "--width") width = std::atoi(argv[i + 1]);
        else if (std::string(argv[i]) == "--height") height = std::atoi(argv[i + 1]);
    }

    std::string error;
    int code = avahost::native::RunNativeApp(temp_dir.string(), manifest.entry_file, width,
                                              height, error);
    if (code != 0 && !error.empty()) {
        std::string message = "avapack: " + error;
        MessageBoxA(nullptr, message.c_str(), "Avalang", MB_OK | MB_ICONERROR);
    }

    std::error_code ec;
    fs::remove_all(temp_dir, ec);
    return code;
}

#endif // defined(_WIN32) && defined(AVAPACK_HAS_DESKTOP_UI_HOST)

} // namespace avapack