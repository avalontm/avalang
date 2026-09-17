#include "designer/responsive.h"

#include <algorithm>

namespace studio::designer {

namespace {

std::vector<DeviceProfile> BuildDeviceProfiles() {
    std::vector<DeviceProfile> profiles;

    profiles.push_back({"Responsive", DeviceKind::Responsive, LayoutSize{0.0, 0.0}, SafeAreaInsets{}});
    profiles.push_back({"Desktop (1280x720)", DeviceKind::Desktop, LayoutSize{1280.0, 720.0}, SafeAreaInsets{}});
    profiles.push_back({"Web (1366x768)", DeviceKind::Desktop, LayoutSize{1366.0, 768.0}, SafeAreaInsets{}});
    profiles.push_back({"Android Phone (412x915)", DeviceKind::Mobile, LayoutSize{412.0, 915.0},
                         SafeAreaInsets{24.0f, 0.0f, 16.0f, 0.0f}});
    profiles.push_back({"Android Tablet (800x1280)", DeviceKind::Tablet, LayoutSize{800.0, 1280.0},
                         SafeAreaInsets{24.0f, 0.0f, 0.0f, 0.0f}});
    profiles.push_back({"iOS Phone (390x844)", DeviceKind::Mobile, LayoutSize{390.0, 844.0},
                         SafeAreaInsets{47.0f, 0.0f, 34.0f, 0.0f}});
    profiles.push_back({"iOS Tablet (820x1180)", DeviceKind::Tablet, LayoutSize{820.0, 1180.0},
                         SafeAreaInsets{24.0f, 0.0f, 20.0f, 0.0f}});

    return profiles;
}

}

const std::vector<DeviceProfile>& DeviceProfiles() {
    static const std::vector<DeviceProfile> profiles = BuildDeviceProfiles();
    return profiles;
}

DeviceProfile MakeCustomProfile(const std::string& name, double width, double height) {
    return DeviceProfile{name, DeviceKind::Custom, LayoutSize{width, height}, SafeAreaInsets{}};
}

LayoutRect ApplySafeArea(const LayoutRect& frame, const SafeAreaInsets& insets) {
    const double left = frame.x + insets.left;
    const double top = frame.y + insets.top;
    const double width = std::max(0.0, frame.width - insets.left - insets.right);
    const double height = std::max(0.0, frame.height - insets.top - insets.bottom);
    return LayoutRect{left, top, width, height};
}

avalang::ui::theme::ControlStyleOverride ResolveResponsiveStyle(const avalang::ui::theme::ProjectStyleSheet& styles,
                                                                  const std::string& typeLower, double viewportWidth,
                                                                  bool isPureLayoutContainer) {
    avalang::ui::theme::ControlStyleOverride result = styles.Resolve(typeLower, isPureLayoutContainer);

    for (const avalang::ui::theme::BreakpointOverride& breakpoint : styles.Breakpoints()) {
        if (static_cast<double>(breakpoint.minWidthPx) > viewportWidth) {
            continue;
        }
        if (breakpoint.hasGlobal) {
            breakpoint.global.MergeOnto(result);
        }
        const auto it = breakpoint.perType.find(typeLower);
        if (it != breakpoint.perType.end()) {
            it->second.MergeOnto(result);
        }
    }

    return result;
}

}
