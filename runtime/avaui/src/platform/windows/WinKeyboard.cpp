#include "WinKeyboard.h"
#include <windows.h>

namespace avalang {
namespace ui {
namespace platform {
namespace windows {

bool WinKeyboard::IsKeyDown(int keyCode) const {
    return (GetAsyncKeyState(keyCode) & 0x8000) != 0;
}

} // namespace windows
} // namespace platform
} // namespace ui
} // namespace avalang
