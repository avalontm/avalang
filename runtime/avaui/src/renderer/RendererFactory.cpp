#include "renderer/IRenderer.h"
#include "renderer/NullRenderer.h"
#include "renderer/HTMLRenderer.h"
#include <cstring>

#ifdef _WIN32
#include "renderer/GdiRenderer.h"
#include <windows.h>
#endif

namespace avalang {
namespace ui {

std::unique_ptr<IRenderer> IRenderer::Create(const char* backend, int width, int height) {
    if (!backend) backend = "html";
    
    // Phase 10: HTML backend + Phase 9 null/stub.
    // Phase 11: "native"/"gdi" backend, Windows-only. On non-Windows
    // targets (Linux/macOS backends are STUB -- see
    // docs/AVALANG_UI_PROGRESS.md) this name simply isn't compiled in
    // and falls through to HTML below, same as any unrecognized name.
    
    if (std::strcmp(backend, "html") == 0) {
        return std::make_unique<HTMLRenderer>(width, height);
    }
    
    if (std::strcmp(backend, "null") == 0) {
        return std::make_unique<NullRenderer>(width, height);
    }

#ifdef _WIN32
    if (std::strcmp(backend, "native") == 0 || std::strcmp(backend, "gdi") == 0) {
        // This convenience path targets whatever window currently has
        // the foreground -- fine for a quick smoke test, but real
        // callers should construct GdiRenderer directly with the HWND
        // from their own WinWindow (via IWindow::NativeHandle()).
        return std::make_unique<GdiRenderer>(GetForegroundWindow(), width, height);
    }
#endif
    
    // Default to HTML if backend unknown (or null as ultra-safe fallback)
    return std::make_unique<HTMLRenderer>(width, height);
}

} // namespace ui
} // namespace avalang
