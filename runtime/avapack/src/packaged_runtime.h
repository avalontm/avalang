#ifndef AVAPACK_PACKAGED_RUNTIME_H
#define AVAPACK_PACKAGED_RUNTIME_H

// Fase 9 -- logica de "arrancar el proyecto empacado" (temp dir + hooks de
// Fase 4, verificacion de integridad de Fase 5, entry-como-bytecode de
// Fase 6) extraida de src/main.cpp para poder reusarla desde
// src/stub_main.cpp SIN duplicarla. src/main.cpp (compilado contra un
// embedded_project.cpp generado, Fases 1-8) y src/stub_main.cpp (Fase 9,
// lee el payload apendeado en runtime) llegan los dos a un
// avapack::PackagedManifest -- desde symbols extern compilados uno, desde
// el blob decodificado el otro -- y llaman a la MISMA RunPackagedProgram.
//
// main_zerodisk.cpp (Fase 7) NO usa esto -- su flujo (MemoryFileSystem, sin
// temp dir) es lo bastante distinto como para no valer la pena forzarlo
// atras de la misma funcion (ver comentario de CMakeLists.txt sobre por
// que son plantillas separadas). Combinar --zero-disk con el stub
// precompilado de Fase 9 queda pendiente (ver README.md, seccion Fase 9).

#include <cstdint>
#include <string>

#include "embedded_project.h" // avapack::EmbeddedFile

namespace avapack {

// Vista de todo lo que antes eran symbols extern (kEmbeddedFiles,
// kEntryFile, kIntegrityMac, kDebugBuild, kEntryIsBytecode, ...) -- ahora
// pasado explicitamente para que el mismo RunPackagedProgram sirva tanto
// para constantes compiladas (main.cpp) como para datos leidos en runtime
// desde el payload apendeado (stub_main.cpp). Ninguno de los punteros es
// owning -- el llamador mantiene vivos los buffers de origen (el array
// `kEmbeddedFiles[]` compilado en un caso, el `avapack::PayloadBlob`
// decodificado en el otro) durante toda la llamada a RunPackagedProgram.
struct PackagedManifest {
    const EmbeddedFile* files = nullptr;
    std::size_t file_count = 0;
    std::string entry_file;
    const unsigned char* integrity_mac = nullptr; // 32 bytes
    bool debug_build = false;
    bool entry_is_bytecode = false;
    bool entry_strings_obfuscated = false;
    std::uint64_t entry_obfuscate_seed = 0;
};

// Corre el proyecto empacado descripto por `manifest`, con la clave AES-256
// ya reconstruida en `key` (32 bytes -- el llamador es dueño de borrarla
// despues si hace falta, aunque esta funcion tambien la borra de su propia
// copia local antes de retornar, igual que hacia main.cpp). Devuelve el
// codigo de salida del proceso (0 en exito). argc/argv son los del binario
// empacado -- se exponen al script como el global `args` (ver
// SetScriptArgsGlobal, sin cambios de comportamiento respecto a Fases 1-8).
int RunPackagedProgram(int argc, char** argv, const PackagedManifest& manifest,
                        unsigned char key[32]);

} // namespace avapack

#endif // AVAPACK_PACKAGED_RUNTIME_H
