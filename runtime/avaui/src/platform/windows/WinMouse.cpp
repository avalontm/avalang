#include "WinMouse.h"
#include <windows.h>

namespace avalang {
namespace ui {
namespace platform {
namespace windows {

void WinMouse::Position(int& x, int& y) const {
    POINT pt{};
    GetCursorPos(&pt);
    x = pt.x;
    y = pt.y;
}

bool WinMouse::IsButtonDown(ava::platform::ui::MouseButton button) const {
    int vk = VK_LBUTTON;
    switch (button) {
        case ava::platform::ui::MouseButton::Left:   vk = VK_LBUTTON; break;
        case ava::platform::ui::MouseButton::Right:  vk = VK_RBUTTON; break;
        case ava::platform::ui::MouseButton::Middle: vk = VK_MBUTTON; break;
    }
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

} // namespace windows
} // namespace platform
} // namespace ui
} // namespace avalang
