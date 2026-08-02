#ifndef AVA_UI_PLATFORM_WINDOWS_WINDISPLAY_H
#define AVA_UI_PLATFORM_WINDOWS_WINDISPLAY_H

#include "../../../../avalang/platform/interfaces/services/ui/IDisplay.h"

namespace avalang {
namespace ui {
namespace platform {
namespace windows {

// Phase 11 (Native Backend). Enumerates monitors via
// EnumDisplayMonitors(). DPI is read with GetDpiForMonitor() when
// available (Shcore, Windows 8.1+); falls back to the system DPI
// (96 * scaling) on failure -- no support for per-monitor-DPI-unaware
// process edge cases beyond that fallback.
class WinDisplay final : public ava::platform::ui::IDisplay {
public:
    int MonitorCount() const override;
    ava::platform::ui::DisplayInfo Monitor(int index) const override;
    ava::platform::ui::DisplayInfo PrimaryMonitor() const override;
};

} // namespace windows
} // namespace platform
} // namespace ui
} // namespace avalang

#endif // AVA_UI_PLATFORM_WINDOWS_WINDISPLAY_H
