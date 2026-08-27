// avapack_stub -- Fase 9 ("build sin repo"). Ver runtime/avapack/README.md,
// seccion Fase 9, y payload_format.h para el contrato binario.
//
// A diferencia de src/main.cpp (que se compila junto a un
// embedded_project.cpp generado, con el proyecto ya adentro como
// constantes C++), este binario NO tiene ningun proyecto embebido -- se
// compila UNA sola vez, generico, y se distribuye prebuilt al lado de
// avalang.dll/avalang_ui.dll/ava_cli.exe/avapack_gen.exe (mismo lugar,
// mismo mecanismo que ya usaba AVA_PACK_USE_PREBUILT_AVALANG). `ava_cli
// build` (runtime/avacli/src/build_command.cpp) arma el .exe final
// copiando ESTE binario y apendeandole al final el payload que genera
// avapack_gen --payload-out (cifrado, HMAC, etc. -- exactamente el mismo
// contenido que antes terminaba en un embedded_project.cpp, solo que como
// blob binario en vez de C++) mas un footer de tamaño fijo (ver
// payload_format.h::PayloadFooter) que le dice a este main() donde
// arranca ese blob dentro de su propio archivo.
//
// Todo lo de "correr el proyecto ya decodificado" (temp dir + hooks de
// Fase 4, verificacion de integridad de Fase 5, entry-como-bytecode de
// Fase 6) es identico a src/main.cpp -- vive en avapack::RunPackagedProgram
// (packaged_runtime.h/.cpp), compartido entre los dos. Lo unico que agrega
// este archivo es "de donde sale el avapack::PackagedManifest": en vez de
// symbols extern compilados, de leer+parsear los ultimos bytes del propio
// ejecutable.
//
// --zero-disk (Fase 7, main_zerodisk.cpp) NO esta soportado todavia por
// este camino -- ver README.md, seccion Fase 9, "Pendiente". `ava_cli
// build --zero-disk` sigue cayendo al flujo con CMake/repo mientras tanto.

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

#include "embedded_project.h" // GetKeyFromFragments
#include "packaged_runtime.h"
#include "payload_format.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <limits.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace {

// Mismo mecanismo que GetSelfExecutableDir en avacli/src/build_command.cpp
// (duplicado a proposito: este binario no linkea avacli, es standalone
// salvo por avalang.dll/avalang_ui.dll) -- devuelve la ruta absoluta al
// propio .exe en ejecucion (con el payload ya apendeado por ava_cli build).
fs::path GetSelfExecutablePath() {
#if defined(_WIN32)
    char buf[MAX_PATH];
    DWORD len = ::GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (len == 0 || len == MAX_PATH) return {};
    return fs::path(std::string(buf, len));
#else
    char buf[PATH_MAX];
    ssize_t len = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len <= 0) return {};
    return fs::path(std::string(buf, static_cast<size_t>(len)));
#endif
}

// Lee los ultimos `n` bytes de un archivo ya abierto. false si el archivo
// es mas chico que `n` (footer/payload faltante -- avapack_stub.exe corrido
// directo, sin pasar por `ava_cli build`, o un build incompleto).
bool ReadTail(std::ifstream& f, std::uint64_t file_size, std::size_t n,
              std::vector<unsigned char>& out) {
    if (file_size < n) return false;
    out.resize(n);
    f.seekg(static_cast<std::streamoff>(file_size - n), std::ios::beg);
    f.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(n));
    return static_cast<std::size_t>(f.gcount()) == n;
}

} // namespace

int main(int argc, char** argv) {
    fs::path self_path = GetSelfExecutablePath();
    if (self_path.empty()) {
        std::fprintf(stderr, "error: no se pudo determinar la ruta del propio ejecutable\n");
        return 1;
    }

    std::error_code ec;
    std::uint64_t file_size = static_cast<std::uint64_t>(fs::file_size(self_path, ec));
    if (ec) {
        std::fprintf(stderr, "error: no se pudo leer el tamaño de %s\n", self_path.string().c_str());
        return 1;
    }

    std::ifstream f(self_path, std::ios::binary);
    if (!f) {
        std::fprintf(stderr, "error: no se pudo abrir %s\n", self_path.string().c_str());
        return 1;
    }

    std::vector<unsigned char> footer_bytes;
    if (!ReadTail(f, file_size, avapack::kFooterSize, footer_bytes)) {
        std::fprintf(stderr,
                      "error: este ejecutable no tiene un payload valido apendeado (¿se "
                      "corrio avapack_stub.exe directo en vez de un binario armado por "
                      "'ava_cli build'?)\n");
        return 1;
    }

    avapack::PayloadFooter footer;
    if (!avapack::DecodeFooter(footer_bytes.data(), footer_bytes.size(), footer)) {
        std::fprintf(stderr, "error: footer invalido -- binario corrupto o de una version "
                              "incompatible de avapack_stub\n");
        return 1;
    }
    if (footer.blob_offset + footer.blob_size + avapack::kFooterSize > file_size) {
        std::fprintf(stderr, "error: footer inconsistente (offset/size fuera de rango) -- "
                              "binario corrupto\n");
        return 1;
    }

    std::vector<unsigned char> blob_bytes(static_cast<std::size_t>(footer.blob_size));
    f.seekg(static_cast<std::streamoff>(footer.blob_offset), std::ios::beg);
    f.read(reinterpret_cast<char*>(blob_bytes.data()),
           static_cast<std::streamsize>(blob_bytes.size()));
    if (static_cast<std::uint64_t>(f.gcount()) != footer.blob_size) {
        std::fprintf(stderr, "error: no se pudo leer el payload completo (%llu bytes)\n",
                     static_cast<unsigned long long>(footer.blob_size));
        return 1;
    }
    f.close();

    avapack::PayloadBlob blob;
    if (!avapack::DecodePayloadBlob(blob_bytes.data(), blob_bytes.size(), blob)) {
        std::fprintf(stderr, "error: payload invalido/truncado -- binario corrupto\n");
        return 1;
    }

    unsigned char key[32];
    avapack::GetKeyFromFragments(blob.key_seed, blob.key_fragment_a, blob.key_fragment_b, key);

    // BuildEmbeddedFilesView apunta DENTRO de `blob` (punteros crudos, no
    // owning) -- `blob` tiene que seguir viva hasta que RunPackagedProgram
    // retorne, cosa que ya se cumple: ambas son variables locales de este
    // mismo main() y `files` no escapa de este scope.
    std::vector<avapack::EmbeddedFile> files = avapack::BuildEmbeddedFilesView(blob);

    avapack::PackagedManifest manifest;
    manifest.files = files.data();
    manifest.file_count = files.size();
    manifest.entry_file = blob.entry_file;
    manifest.integrity_mac = blob.integrity_mac;
    manifest.debug_build = blob.debug_build;
    manifest.entry_is_bytecode = blob.entry_is_bytecode;
    manifest.entry_strings_obfuscated = blob.entry_strings_obfuscated;
    manifest.entry_obfuscate_seed = blob.entry_obfuscate_seed;

    int rc = avapack::RunPackagedProgram(argc, argv, manifest, key);

    std::memset(blob.key_fragment_a, 0, sizeof(blob.key_fragment_a));
    std::memset(blob.key_fragment_b, 0, sizeof(blob.key_fragment_b));
    return rc;
}
