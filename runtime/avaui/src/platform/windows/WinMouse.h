#ifndef AVA_UI_PLATFORM_WINDOWS_WINMOUSE_H
#define AVA_UI_PLATFORM_WINDOWS_WINMOUSE_H

#include "../../../../avalang/platform/interfaces/services/ui/IMouse.h"

namespace avalang {
namespace ui {
namespace platform {
namespace windows {

// Phase 11 (Native Backend). Screen-space cursor position + button
// state via GetCursorPos()/GetAsyncKeyState(). Not tied to a specific
// window -- the interface carries none -- so Position() is in screen
// coordinates; callers that need client coordinates must subtract
// their window's origin (e.g. via ScreenToClient with the HWND from
// IWindow::NativeHandle()). Documented limitation, same convention as
// events/EventDispatcher's own documented polling limitations (Phase 5).
class WinMouse final : public ava::platform::ui::IMouse {
public:
    void Position(int& x, int& y) const override;
    bool IsButtonDown(ava::platform::ui::MouseButton button) const override;
};

} // namespace windows
} // namespace platform
} // namespace ui
} // namespace avalang

#endif // AVA_UI_PLATFORM_WINDOWS_WINMOUSE_H
