// Plantilla de runtime empacado -- Fases 1-6 (ver README.md). Este archivo
// se compila junto con el embedded_project.cpp que genera avapack_gen para
// producir el .exe final via CMake (flujo "con repo" -- ver
// runtime/avacli/src/build_command.cpp).
//
// Fase 9: la logica real (temp dir + hooks de Fase 4, verificacion de
// integridad de Fase 5, entry-como-bytecode de Fase 6) se movio a
// avapack::RunPackagedProgram (packaged_runtime.h/.cpp) para poder
// compartirla con src/stub_main.cpp -- el stub precompilado que lee su
// proyecto de un payload apendeado en runtime en vez de estos symbols
// extern compilados. Este main() solo arma el avapack::PackagedManifest a
// partir de kEmbeddedFiles/kEntryFile/kIntegrityMac/etc. (embedded_project.h,
// definidos por el embedded_project.cpp generado) y delega -- el
// comportamiento es exactamente el mismo que antes de este refactor.
//
// main_zerodisk.cpp (Fase 7) sigue siendo un archivo aparte, sin cambios --
// ver su propio comentario de cabecera y CMakeLists.txt sobre por que.

#include "avalang.h"
#include "embedded_project.h"
#include "packaged_runtime.h"

int main(int argc, char** argv) {
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
