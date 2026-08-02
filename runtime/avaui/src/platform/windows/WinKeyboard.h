#ifndef AVA_UI_PLATFORM_WINDOWS_WINKEYBOARD_H
#define AVA_UI_PLATFORM_WINDOWS_WINKEYBOARD_H

#include "../../../../avalang/platform/interfaces/services/ui/IKeyboard.h"

namespace avalang {
namespace ui {
namespace platform {
namespace windows {

// Phase 11 (Native Backend). keyCode is a raw Win32 virtual-key code
// (VK_*) -- IKeyboard::IsKeyDown() is intentionally untyped (int) at
// the PAL level, so no translation table lives here; that belongs to
// events/ (Phase 5) if/when it needs a cross-platform key enum.
class WinKeyboard final : public ava::platform::ui::IKeyboard {
public:
    bool IsKeyDown(int keyCode) const override;
};

} // namespace windows
} // namespace platform
} // namespace ui
} // namespace avalang

#endif // AVA_UI_PLATFORM_WINDOWS_WINKEYBOARD_H
