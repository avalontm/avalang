#ifndef AVA_UI_PLATFORM_WINDOWS_WINMOUSE_H
#define AVA_UI_PLATFORM_WINDOWS_WINMOUSE_H

#include "../../../../avalang/platform/interfaces/services/ui/IMouse.h"

#include <windows.h>

namespace avalang {
namespace ui {
namespace platform {
namespace windows {

// Phase 11 (Native Backend). Screen-space cursor position + button
// state via GetCursorPos()/GetAsyncKeyState().
//
// Position() converts to client coordinates via ScreenToClient() once a
// target window is set with SetWindow() -- without it, HitTest()/the
// EventDispatcher compare a screen-space point against layout rects that
// are in client space, so every click lands off by exactly the window's
// screen offset (visible as "se ajusta si muevo la ventana"). The native
// host (native_app_host.cpp) calls SetWindow() right after creating the
// surface's HWND.
class WinMouse final : public ava::platform::ui::IMouse {
public:
    void SetWindow(HWND hwnd) { hwnd_ = hwnd; }

    void Position(int& x, int& y) const override;
    bool IsButtonDown(ava::platform::ui::MouseButton button) const override;

private:
    HWND hwnd_ = nullptr;
};

} // namespace windows
} // namespace platform
} // namespace ui
} // namespace avalang

#endif // AVA_UI_PLATFORM_WINDOWS_WINMOUSE_H
