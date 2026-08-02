#ifndef AVA_UI_PLATFORM_WINDOWS_WINRENDERSURFACE_H
#define AVA_UI_PLATFORM_WINDOWS_WINRENDERSURFACE_H

#include "../../../../avalang/platform/interfaces/services/ui/IRenderSurface.h"
#include <windows.h>

namespace avalang {
namespace ui {
namespace platform {
namespace windows {

// Phase 11 (Native Backend). Thin wrapper around the HWND of an
// already-created WinWindow -- does not own the handle (the window
// does) and performs no drawing itself, per IRenderSurface's contract.
// Width()/Height() read the live client rect on every call rather than
// caching, so callers see the current size after a WM_SIZE/Resize().
class WinRenderSurface final : public ava::platform::ui::IRenderSurface {
public:
    explicit WinRenderSurface(HWND hwnd) : hwnd_(hwnd) {}

    void* NativeHandle() const override { return hwnd_; }

    int Width() const override {
        RECT rect{};
        if (hwnd_) GetClientRect(hwnd_, &rect);
        return rect.right - rect.left;
    }

    int Height() const override {
        RECT rect{};
        if (hwnd_) GetClientRect(hwnd_, &rect);
        return rect.bottom - rect.top;
    }

private:
    HWND hwnd_;
};

} // namespace windows
} // namespace platform
} // namespace ui
} // namespace avalang

#endif // AVA_UI_PLATFORM_WINDOWS_WINRENDERSURFACE_H
