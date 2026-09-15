#ifndef AVAPACK_PACKAGED_RUNTIME_H
#define AVAPACK_PACKAGED_RUNTIME_H

#include <cstdint>
#include <string>

#include "avalang.h"
#include "embedded_project.h"

namespace avapack {

struct PackagedManifest {
    const EmbeddedFile* files = nullptr;
    std::size_t file_count = 0;
    std::string entry_file;
    const unsigned char* integrity_mac = nullptr;
    bool debug_build = false;
    bool entry_is_bytecode = false;
    bool entry_strings_obfuscated = false;
    std::uint64_t entry_obfuscate_seed = 0;
};

int RunPackagedProgram(int argc, char** argv, const PackagedManifest& manifest,
                        unsigned char key[32]);

struct PackagedInstance;

PackagedInstance* LoadPackagedProgram(int argc, char** argv, const PackagedManifest& manifest,
                                       unsigned char key[32], std::string* out_error);

AvaVM* PackagedInstanceVM(PackagedInstance* instance);

void UnloadPackagedProgram(PackagedInstance* instance);

} // namespace avapack

#endif // AVAPACK_PACKAGED_RUNTIME_H
