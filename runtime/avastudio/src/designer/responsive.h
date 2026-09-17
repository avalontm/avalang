#pragma once

#include <string>
#include <vector>

#include "designer/types.h"
#include "platform/interfaces/services/mobile/ISafeArea.h"
#include "theme/ProjectStyleOverrides.h"

namespace studio::designer {

enum class DeviceKind {
    Responsive,
    Desktop,
    Tablet,
    Mobile,
    Custom,
};

using SafeAreaInsets = ava::platform::mobile::SafeAreaInsets;

struct DeviceProfile {
    std::string name;
    DeviceKind kind;
    LayoutSize size;
    SafeAreaInsets safeArea;
};

const std::vector<DeviceProfile>& DeviceProfiles();

DeviceProfile MakeCustomProfile(const std::string& name, double width, double height);

LayoutRect ApplySafeArea(const LayoutRect& frame, const SafeAreaInsets& insets);

avalang::ui::theme::ControlStyleOverride ResolveResponsiveStyle(const avalang::ui::theme::ProjectStyleSheet& styles,
                                                                  const std::string& typeLower, double viewportWidth,
                                                                  bool isPureLayoutContainer = false);

}
