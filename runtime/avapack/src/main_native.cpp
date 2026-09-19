// Plantilla de runtime empacado -- variante "Desktop UI" (Fase 21.x) de
// src/main.cpp. Se usa cuando `ava_cli build` detecta un proyecto
// --target desktop --with-ui sin --output-kind library (ver
// runtime/avacli/src/build_command.cpp, NeedsNativeUiHost) y arma este
// .exe con AVAPACK_DESKTOP_UI=ON (ver runtime/avapack/CMakeLists.txt:
// AVAPACK_MAIN_SRC = src/main_native.cpp cuando AVAPACK_DESKTOP_UI=ON).
//
// Identico a src/main.cpp salvo por la ultima linea: en vez de delegar
// en avapack::RunPackagedProgram (el runtime de consola de siempre, un
// ava_run directo sobre el entry -- no sabe nada de `Application.run(...)`)
// delega en avapack::RunPackagedNativeApp (packaged_runtime.h/.cpp), que
// descifra el proyecto embebido a un temp dir y corre el mismo "native
// app host" (avahost::native::RunNativeApp, en avahost_native) que ya usa
// avanative.exe para F5/Preview -- registro de la clase Application +
// loop de ventana nativa via avalang_ui_win.
//
// Este target se compila WIN32_EXECUTABLE (ver CMakeLists.txt) -- sin
// consola, como avanative.exe -- asi que cualquier error temprano se
// reporta con un MessageBox (ver RunPackagedNativeApp), no por stderr.

#include "avalang.h"
#include "embedded_project.h"
#include "packaged_runtime.h"
#include "diagnostics/crash_handler.h"
#include "diagnostics/debug_mode.h"

int main(int argc, char** argv) {
    // Fase 1 de PLAN_DEBUG_MODE_AVASTUDIO.md: este es el .exe Desktop UI
    // que el usuario realmente construye y corre -- corre bajo subsystem
    // WINDOWS (sin consola), asi que sin este filtro un crash nativo era
    // literalmente indistinguible de "la ventana se cerro" para el
    // usuario. Antes solo avanative.exe (el binario de F5/Preview dentro
    // de AvaStudio, no el .exe final) tenia este handler.
    ava::diag::InstallCrashHandler("avapack (desktop ui)");
    ava::diag::InitDebugRuntime(argc, argv);

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

    return avapack::RunPackagedNativeApp(argc, argv, manifest, key);
}
