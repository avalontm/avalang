#include "WinCursor.h"
#include <windows.h>

namespace avalang {
namespace ui {
namespace platform {
namespace windows {

void WinCursor::SetShape(ava::platform::ui::CursorShape shape) {
    const wchar_t* id = IDC_ARROW;
    switch (shape) {
        case ava::platform::ui::CursorShape::Arrow:            id = IDC_ARROW;    break;
        case ava::platform::ui::CursorShape::IBeam:             id = IDC_IBEAM;    break;
        case ava::platform::ui::CursorShape::Hand:              id = IDC_HAND;     break;
        case ava::platform::ui::CursorShape::ResizeHorizontal:  id = IDC_SIZEWE;   break;
        case ava::platform::ui::CursorShape::ResizeVertical:    id = IDC_SIZENS;   break;
    }
    SetCursor(LoadCursorW(nullptr, id));
}

void WinCursor::SetVisible(bool visible) {
    if (visible == visible_) return;
    visible_ = visible;
    ShowCursor(visible ? TRUE : FALSE);
}

} // namespace windows
} // namespace platform
} // namespace ui
} // namespace avalang
