// Windows-only. avahost.exe links avalang.dll (and, when AVA_BUILD_UI is
// ON, avalang_ui.dll) as delay-loaded imports (see CMakeLists.txt,
// /DELAYLOAD + delayimp.lib, MSVC only). Without delay-loading, a missing
// DLL is a load-time failure resolved by the Windows loader before
// main() ever runs -- the process just vanishes with no console output
// at all (only a native "entry point not found"/"module not found"
// message box, if even that). Delay-loading defers resolution to first
// call, so this hook gets a chance to print something useful on stderr
// and exit cleanly instead.
#include <windows.h>
#include <delayimp.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

FARPROC WINAPI OnDelayLoadFailure(unsigned dliNotify, PDelayLoadInfo pdli) {
    if (dliNotify == dliFailLoadLib) {
        std::fprintf(stderr,
            "avahost: fatal - could not load '%s'.\n"
            "  Make sure %s is next to avahost.exe (or on PATH).\n"
            "  It ships from the matching runtime/%s build output.\n",
            pdli->szDll, pdli->szDll,
            (std::strcmp(pdli->szDll, "avalang_ui.dll") == 0) ? "avaui" : "avalang");
        std::fflush(stderr);
        std::exit(1);
    }

    if (dliNotify == dliFailGetProc) {
        const char* symbol = pdli->dlp.fImportByName ? pdli->dlp.szProcName : "(ordinal import)";
        std::fprintf(stderr,
            "avahost: fatal - '%s' does not export '%s'.\n"
            "  The DLL next to avahost.exe is likely from a mismatched/older build.\n",
            pdli->szDll, symbol);
        std::fflush(stderr);
        std::exit(1);
    }

    return nullptr;
}

}  // namespace

extern "C" const PfnDliHook __pfnDliFailureHook2 = OnDelayLoadFailure;
