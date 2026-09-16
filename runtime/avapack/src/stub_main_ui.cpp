// avapack_stub_ui -- variante "Desktop UI" (Fase 21.x) de src/stub_main.cpp.
// Ver ese archivo para el contrato completo del payload apendeado (Fase 9,
// payload_format.h) -- este archivo reusa exactamente el mismo mecanismo
// de "leer los ultimos bytes del propio .exe" para reconstruir el
// avapack::PackagedManifest; lo unico que cambia es:
//
//   1. Delega en avapack::RunPackagedNativeApp (packaged_runtime.h/.cpp)
//      en vez de avapack::RunPackagedProgram -- el mismo "native app
//      host" (avahost_native) que usa avanative.exe, para proyectos
//      Desktop que llaman `Application.run(viewPath)`.
//   2. Se compila WIN32_EXECUTABLE (ver CMakeLists.txt: avapack_stub_ui,
//      solo cuando el target avahost_native esta disponible) -- sin
//      consola, asi que los errores se reportan con MessageBoxA en vez
//      de fprintf(stderr, ...): sin consola asignada, stderr no lo ve
//      nadie (mismo motivo que avanative.exe / avapack::RunPackagedNativeApp,
//      ver runtime/avahost/src/native/main_native.cpp).
//
// `ava_cli build` elige este binario prebuilt en vez de avapack_stub.exe
// cuando el proyecto es --with-ui --target desktop sin --output-kind
// library (ver runtime/avacli/src/build_command.cpp, NeedsNativeUiHost) Y
// avapack_stub_ui.exe/avalang_ui_win.dll estan prebuilt junto a
// ava_cli.exe (ver scripts/build_pack_tools.bat). Si no estan, `ava_cli
// build` cae al empaquetado con CMake (AVAPACK_DESKTOP_UI=ON).
//
// --zero-disk y --obfuscate no estan soportados por este camino Desktop
// UI todavia -- ver el chequeo de manifest.entry_is_bytecode dentro de
// RunPackagedNativeApp (packaged_runtime.cpp), que devuelve error si el
// payload fue generado con --obfuscate.

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

#include "embedded_project.h" // GetKeyFromFragments
#include "packaged_runtime.h"
#include "payload_format.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace fs = std::filesystem;

namespace {

// Mismo mecanismo que GetSelfExecutablePath en src/stub_main.cpp
// (duplicado a proposito -- este binario tampoco linkea avacli).
fs::path GetSelfExecutablePath() {
    char buf[MAX_PATH];
    DWORD len = ::GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (len == 0 || len == MAX_PATH) return {};
    return fs::path(std::string(buf, len));
}

bool ReadTail(std::ifstream& f, std::uint64_t file_size, std::size_t n,
              std::vector<unsigned char>& out) {
    if (file_size < n) return false;
    out.resize(n);
    f.seekg(static_cast<std::streamoff>(file_size - n), std::ios::beg);
    f.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(n));
    return static_cast<std::size_t>(f.gcount()) == n;
}

void ReportError(const std::string& message) {
    // Sin consola (WIN32_EXECUTABLE) -- ver comentario de cabecera.
    MessageBoxA(nullptr, message.c_str(), "Avalang", MB_OK | MB_ICONERROR);
}

} // namespace

int main(int argc, char** argv) {
    fs::path self_path = GetSelfExecutablePath();
    if (self_path.empty()) {
        ReportError("avapack: no se pudo determinar la ruta del propio ejecutable");
        return 1;
    }

    std::error_code ec;
    std::uint64_t file_size = static_cast<std::uint64_t>(fs::file_size(self_path, ec));
    if (ec) {
        ReportError("avapack: no se pudo leer el tamaño de " + self_path.string());
        return 1;
    }

    std::ifstream f(self_path, std::ios::binary);
    if (!f) {
        ReportError("avapack: no se pudo abrir " + self_path.string());
        return 1;
    }

    std::vector<unsigned char> footer_bytes;
    if (!ReadTail(f, file_size, avapack::kFooterSize, footer_bytes)) {
        ReportError(
            "avapack: este ejecutable no tiene un payload valido apendeado (¿se corrio "
            "avapack_stub_ui.exe directo en vez de un binario armado por 'ava_cli build'?)");
        return 1;
    }

    avapack::PayloadFooter footer;
    if (!avapack::DecodeFooter(footer_bytes.data(), footer_bytes.size(), footer)) {
        ReportError("avapack: footer invalido -- binario corrupto o de una version incompatible "
                     "de avapack_stub_ui");
        return 1;
    }
    if (footer.blob_offset + footer.blob_size + avapack::kFooterSize > file_size) {
        ReportError("avapack: footer inconsistente (offset/size fuera de rango) -- binario "
                     "corrupto");
        return 1;
    }

    std::vector<unsigned char> blob_bytes(static_cast<std::size_t>(footer.blob_size));
    f.seekg(static_cast<std::streamoff>(footer.blob_offset), std::ios::beg);
    f.read(reinterpret_cast<char*>(blob_bytes.data()),
           static_cast<std::streamsize>(blob_bytes.size()));
    if (static_cast<std::uint64_t>(f.gcount()) != footer.blob_size) {
        ReportError("avapack: no se pudo leer el payload completo");
        return 1;
    }
    f.close();

    avapack::PayloadBlob blob;
    if (!avapack::DecodePayloadBlob(blob_bytes.data(), blob_bytes.size(), blob)) {
        ReportError("avapack: payload invalido/truncado -- binario corrupto");
        return 1;
    }

    unsigned char key[32];
    avapack::GetKeyFromFragments(blob.key_seed, blob.key_fragment_a, blob.key_fragment_b, key);

    // BuildEmbeddedFilesView apunta DENTRO de `blob` (punteros crudos, no
    // owning) -- `blob` tiene que seguir viva hasta que
    // RunPackagedNativeApp retorne, cosa que ya se cumple aca (variables
    // locales de este mismo main()).
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

    int rc = avapack::RunPackagedNativeApp(argc, argv, manifest, key);

    std::memset(blob.key_fragment_a, 0, sizeof(blob.key_fragment_a));
    std::memset(blob.key_fragment_b, 0, sizeof(blob.key_fragment_b));
    return rc;
}
