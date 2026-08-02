#include "WinDisplay.h"
#include <windows.h>
#include <shellscalingapi.h>
#include <vector>

namespace avalang {
namespace ui {
namespace platform {
namespace windows {

namespace {

BOOL CALLBACK CollectMonitor(HMONITOR monitor, HDC, LPRECT, LPARAM lparam) {
    auto* out = reinterpret_cast<std::vector<HMONITOR>*>(lparam);
    out->push_back(monitor);
    return TRUE;
}

ava::platform::ui::DisplayInfo InfoFor(HMONITOR monitor) {
    ava::platform::ui::DisplayInfo info;
    if (!monitor) return info;

    MONITORINFO mi{};
    mi.cbSize = sizeof(MONITORINFO);
    if (GetMonitorInfoW(monitor, &mi)) {
        info.width = mi.rcMonitor.right - mi.rcMonitor.left;
        info.height = mi.rcMonitor.bottom - mi.rcMonitor.top;
    }

    UINT dpiX = 96, dpiY = 96;
    if (SUCCEEDED(GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY))) {
        info.dpi = static_cast<float>(dpiX);
        info.scaling = static_cast<float>(dpiX) / 96.0f;
    }

    return info;
}

std::vector<HMONITOR> AllMonitors() {
    std::vector<HMONITOR> monitors;
    EnumDisplayMonitors(nullptr, nullptr, &CollectMonitor, reinterpret_cast<LPARAM>(&monitors));
    return monitors;
}

} // namespace

int WinDisplay::MonitorCount() const {
    return static_cast<int>(AllMonitors().size());
}

ava::platform::ui::DisplayInfo WinDisplay::Monitor(int index) const {
    auto monitors = AllMonitors();
    if (index < 0 || static_cast<size_t>(index) >= monitors.size()) {
        return ava::platform::ui::DisplayInfo{};
    }
    return InfoFor(monitors[static_cast<size_t>(index)]);
}

ava::platform::ui::DisplayInfo WinDisplay::PrimaryMonitor() const {
    POINT origin{0, 0};
    HMONITOR primary = MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY);
    return InfoFor(primary);
}

} // namespace windows
} // namespace platform
} // namespace ui
} // namespace avalang
