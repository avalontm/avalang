#include "avalang.h"
#include "embedded_project.h"
#include "packaged_runtime.h"
#include "lib_api.h"

AVAPACK_LIB_API int avapack_run(int argc, char** argv) {
    unsigned char key[32];
    avapack::GetEmbeddedKey(key);

    avapack::PackagedManifest manifest;
    manifest.files = avapack::kEmbeddedFiles;
    manifest.file_count = avapack::kEmbeddedFileCount;
    manifest.entry_file = avapack::kEntryFile;
    manifest.integrity_mac = avapack::kIntegrityMac;
    manifest.debug_build = avapack::kDebugBuild;
    manifest.entry_is_bytecode = avapack::kEntryIsBytecode;
    manifest.entry_strings_obfuscated = avapack::kEntryStringsObfuscated;
    manifest.entry_obfuscate_seed = avapack::kEntryObfuscateSeed;

    return avapack::RunPackagedProgram(argc, argv, manifest, key);
}

AVAPACK_LIB_API const char* avapack_abi_version(void) { return "1"; }
