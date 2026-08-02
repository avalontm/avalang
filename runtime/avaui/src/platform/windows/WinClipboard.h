#ifndef AVA_UI_PLATFORM_WINDOWS_WINCLIPBOARD_H
#define AVA_UI_PLATFORM_WINDOWS_WINCLIPBOARD_H

#include "../../../../avalang/platform/interfaces/services/ui/IClipboard.h"

namespace avalang {
namespace ui {
namespace platform {
namespace windows {

// Phase 11 (Native Backend). Wraps the Win32 clipboard using
// CF_UNICODETEXT so non-ASCII text round-trips correctly; converts
// to/from UTF-8 at the IClipboard boundary since std::string in this
// codebase is UTF-8 (see WinWindow::Create()'s title conversion for
// the same convention).
class WinClipboard final : public ava::platform::ui::IClipboard {
public:
    void SetText(const std::string& text) override;
    std::string GetText() const override;
    bool HasText() const override;
};

} // namespace windows
} // namespace platform
} // namespace ui
} // namespace avalang

#endif // AVA_UI_PLATFORM_WINDOWS_WINCLIPBOARD_H
