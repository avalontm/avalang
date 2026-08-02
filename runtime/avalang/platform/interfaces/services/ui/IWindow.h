#ifndef AVA_PLATFORM_SERVICES_UI_IWINDOW_H
#define AVA_PLATFORM_SERVICES_UI_IWINDOW_H

#include <cstdint>

namespace ava {
namespace platform {
namespace ui {

// Phase 0 stub. Not consumed by avalang.dll today; reserved extension
// point for avalang.ui.dll (see docs/Platform_Foundation.md).
enum class WindowState {
    Normal,
    Minimized,
    Maximized,
    Fullscreen,
};

class IWindow {
public:
    virtual ~IWindow() = default;

    virtual void Create(int width, int height, const char* title) = 0;
    virtual void Destroy() = 0;

    virtual void Resize(int width, int height) = 0;
    virtual void Show() = 0;
    virtual void Hide() = 0;

    virtual WindowState State() const = 0;
    virtual void SetState(WindowState state) = 0;

    // Phase 11 (Native Backend). Native handle backing this window
    // (HWND on Windows, X11 Window*/Wayland surface on Linux, NSWindow*
    // on macOS). Needed so a render surface / renderer can attach to the
    // same native object the window owns. nullptr before Create().
    virtual void* NativeHandle() const = 0;
};

} // namespace ui
} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_SERVICES_UI_IWINDOW_H
