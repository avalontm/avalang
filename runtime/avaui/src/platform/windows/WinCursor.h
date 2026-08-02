#ifndef AVA_UI_PLATFORM_WINDOWS_WINCURSOR_H
#define AVA_UI_PLATFORM_WINDOWS_WINCURSOR_H

#include "../../../../avalang/platform/interfaces/services/ui/ICursor.h"

namespace avalang {
namespace ui {
namespace platform {
namespace windows {

// Phase 11 (Native Backend). Maps CursorShape to a stock Win32 cursor
// (IDC_*) and applies it via SetCursor()/LoadCursorW(). Global, not
// per-window -- matches the PAL's ICursor contract.
class WinCursor final : public ava::platform::ui::ICursor {
public:
    void SetShape(ava::platform::ui::CursorShape shape) override;
    void SetVisible(bool visible) override;

private:
    bool visible_ = true;
};

} // namespace windows
} // namespace platform
} // namespace ui
} // namespace avalang

#endif // AVA_UI_PLATFORM_WINDOWS_WINCURSOR_H
