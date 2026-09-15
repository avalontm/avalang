#include <cstdlib>
#include <cstring>
#include <mutex>

#include "avalang.h"
#include "embedded_project.h"
#include "packaged_runtime.h"
#include "lib_api.h"

namespace {

std::mutex g_mutex;
avapack::PackagedInstance* g_instance = nullptr;

avapack::PackagedManifest BuildManifest() {
    avapack::PackagedManifest manifest;
    manifest.files = avapack::kEmbeddedFiles;
    manifest.file_count = avapack::kEmbeddedFileCount;
    manifest.entry_file = avapack::kEntryFile;
    manifest.integrity_mac = avapack::kIntegrityMac;
    manifest.debug_build = avapack::kDebugBuild;
    manifest.entry_is_bytecode = avapack::kEntryIsBytecode;
    manifest.entry_strings_obfuscated = avapack::kEntryStringsObfuscated;
    manifest.entry_obfuscate_seed = avapack::kEntryObfuscateSeed;
    return manifest;
}

char* DupError(const std::string& text) {
    char* out = static_cast<char*>(std::malloc(text.size() + 1));
    if (!out) return nullptr;
    std::memcpy(out, text.c_str(), text.size() + 1);
    return out;
}

} // namespace

AVAPACK_LIB_API int avapack_run(int argc, char** argv) {
    unsigned char key[32];
    avapack::GetEmbeddedKey(key);
    avapack::PackagedManifest manifest = BuildManifest();
    return avapack::RunPackagedProgram(argc, argv, manifest, key);
}

AVAPACK_LIB_API const char* avapack_abi_version(void) { return "2"; }

AVAPACK_LIB_API int avapack_init(int argc, char** argv, char** out_error) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_instance) {
        if (out_error) *out_error = DupError("avapack_init ya fue llamado (avapack_shutdown primero)");
        return 0;
    }

    unsigned char key[32];
    avapack::GetEmbeddedKey(key);
    avapack::PackagedManifest manifest = BuildManifest();

    std::string error;
    avapack::PackagedInstance* instance = avapack::LoadPackagedProgram(argc, argv, manifest, key, &error);
    if (!instance) {
        if (out_error) *out_error = DupError(error);
        return 0;
    }

    g_instance = instance;
    return 1;
}

AVAPACK_LIB_API int avapack_is_loaded(void) {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_instance != nullptr;
}

AVAPACK_LIB_API int avapack_call(
    const char* fn_name,
    const ava_value_t* args,
    size_t arg_count,
    ava_value_t* out_result,
    char** out_error
) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_instance) {
        if (out_error) *out_error = DupError("avapack_init no fue llamado todavia");
        return 0;
    }

    AvaVM* vm = avapack::PackagedInstanceVM(g_instance);
    ava_value_t callable = ava_get_global(vm, fn_name);
    if (callable.type != AVA_FUNCTION && callable.type != AVA_NATIVE && callable.type != AVA_BOUND) {
        if (out_error) *out_error = DupError(std::string("global no invocable: ") + fn_name);
        return 0;
    }

    char* error = nullptr;
    ava_value_t result{};
    ava_call(vm, callable, args, arg_count, &result, &error);
    if (error) {
        if (out_error) *out_error = DupError(error);
        ava_string_free(error);
        return 0;
    }

    if (out_result) *out_result = result;
    return 1;
}

AVAPACK_LIB_API ava_value_t avapack_get_global(const char* name) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_instance) {
        ava_value_t nil{};
        nil.type = AVA_NIL;
        return nil;
    }
    return ava_get_global(avapack::PackagedInstanceVM(g_instance), name);
}

AVAPACK_LIB_API void avapack_set_global(const char* name, ava_value_t value) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_instance) return;
    ava_set_global(avapack::PackagedInstanceVM(g_instance), name, value);
}

AVAPACK_LIB_API AvaVM* avapack_vm(void) {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_instance ? avapack::PackagedInstanceVM(g_instance) : nullptr;
}

AVAPACK_LIB_API void avapack_free_error(char* error) {
    std::free(error);
}

AVAPACK_LIB_API void avapack_shutdown(void) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_instance) return;
    avapack::UnloadPackagedProgram(g_instance);
    g_instance = nullptr;
}
