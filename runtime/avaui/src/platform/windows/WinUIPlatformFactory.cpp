#include "WinUIPlatformFactory.h"
#include "WinWindow.h"
#include "WinRenderSurface.h"

// windows.h (pulled in via WinWindow.h) #defines CreateWindow to
// CreateWindowW/A -- collides with IUIPlatformFactory::CreateWindow().
// Standard Win32 fix: undef the macro, we don't call the raw API by
// that name in this file (WinWindow::Create() does, in its own TU).
#ifdef CreateWindow
#undef CreateWindow
#endif

namespace avalang {
namespace ui {
namespace platform {

ava::platform::ui::IWindow* WinUIPlatformFactory::CreateWindow() {
    return new windows::WinWindow();
}

ava::platform::ui::IRenderSurface* WinUIPlatformFactory::CreateRenderSurface(ava::platform::ui::IWindow* window) {
    // Every IWindow reaching this factory on Windows is a WinWindow
    // (it is the only type WinUIPlatformFactory::CreateWindow() ever
    // hands out) -- static_cast is safe here, unlike a factory that
    // had to accept windows from arbitrary backends.
    HWND hwnd = window ? static_cast<HWND>(window->NativeHandle()) : nullptr;
    return new windows::WinRenderSurface(hwnd);
}

IUIPlatformFactory& GetUIPlatformFactory() {
    static WinUIPlatformFactory instance;
    return instance;
}

} // namespace platform
} // namespace ui
} // namespace avalang
