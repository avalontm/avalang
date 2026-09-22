#pragma once

#include <vector>

#include "designer/responsive.h"
#include "designer/types.h"

namespace studio::designer {

constexpr double kMinCustomDimension = 120.0;
constexpr double kMaxCustomDimension = 4000.0;

enum class DeviceOrientation {
    Default,
    Rotated,
};

struct DeviceViewport {
    LayoutSize deviceSize;
    LayoutRect contentRect;
    SafeAreaInsets insets;
    bool fixed = false;
};

bool SupportsRotation(const DeviceProfile& profile);

bool HasSafeArea(const SafeAreaInsets& insets);

LayoutSize ClampCustomSize(double width, double height);

LayoutSize OrientedSize(const DeviceProfile& profile, DeviceOrientation orientation);

SafeAreaInsets OrientedInsets(const DeviceProfile& profile, DeviceOrientation orientation);

DeviceViewport ResolveDeviceViewport(const DeviceProfile& profile, DeviceOrientation orientation,
                                      bool respectSafeArea, const LayoutSize& available);

std::vector<LayoutRect> SafeAreaBands(const LayoutSize& deviceSize, const SafeAreaInsets& insets);

}
