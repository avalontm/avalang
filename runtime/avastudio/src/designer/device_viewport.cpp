#include "designer/device_viewport.h"

#include <algorithm>

namespace studio::designer {

namespace {

bool IsRotated(const DeviceProfile& profile, DeviceOrientation orientation) {
    return orientation == DeviceOrientation::Rotated && SupportsRotation(profile);
}

}

bool SupportsRotation(const DeviceProfile& profile) {
    return profile.kind == DeviceKind::Mobile || profile.kind == DeviceKind::Tablet ||
           profile.kind == DeviceKind::Custom;
}

bool HasSafeArea(const SafeAreaInsets& insets) {
    return insets.top > 0.0f || insets.right > 0.0f || insets.bottom > 0.0f || insets.left > 0.0f;
}

LayoutSize ClampCustomSize(double width, double height) {
    return LayoutSize{std::clamp(width, kMinCustomDimension, kMaxCustomDimension),
                      std::clamp(height, kMinCustomDimension, kMaxCustomDimension)};
}

LayoutSize OrientedSize(const DeviceProfile& profile, DeviceOrientation orientation) {
    if (!IsRotated(profile, orientation)) {
        return profile.size;
    }
    return LayoutSize{profile.size.height, profile.size.width};
}

SafeAreaInsets OrientedInsets(const DeviceProfile& profile, DeviceOrientation orientation) {
    if (!IsRotated(profile, orientation)) {
        return profile.safeArea;
    }
    SafeAreaInsets rotated;
    rotated.top = profile.safeArea.left;
    rotated.right = profile.safeArea.top;
    rotated.bottom = profile.safeArea.right;
    rotated.left = profile.safeArea.bottom;
    return rotated;
}

DeviceViewport ResolveDeviceViewport(const DeviceProfile& profile, DeviceOrientation orientation,
                                      bool respectSafeArea, const LayoutSize& available) {
    DeviceViewport result;
    if (profile.kind == DeviceKind::Responsive) {
        result.deviceSize = available;
        result.contentRect = LayoutRect{0.0, 0.0, available.width, available.height};
        return result;
    }

    result.fixed = true;
    result.deviceSize = OrientedSize(profile, orientation);
    result.insets = OrientedInsets(profile, orientation);
    const LayoutRect full{0.0, 0.0, result.deviceSize.width, result.deviceSize.height};
    result.contentRect = respectSafeArea ? ApplySafeArea(full, result.insets) : full;
    return result;
}

std::vector<LayoutRect> SafeAreaBands(const LayoutSize& deviceSize, const SafeAreaInsets& insets) {
    const double top = std::clamp(static_cast<double>(insets.top), 0.0, deviceSize.height);
    const double bottom = std::clamp(static_cast<double>(insets.bottom), 0.0, deviceSize.height - top);
    const double left = std::clamp(static_cast<double>(insets.left), 0.0, deviceSize.width);
    const double right = std::clamp(static_cast<double>(insets.right), 0.0, deviceSize.width - left);
    const double middleHeight = deviceSize.height - top - bottom;

    std::vector<LayoutRect> bands;
    if (top > 0.0) bands.push_back(LayoutRect{0.0, 0.0, deviceSize.width, top});
    if (bottom > 0.0) bands.push_back(LayoutRect{0.0, deviceSize.height - bottom, deviceSize.width, bottom});
    if (middleHeight > 0.0) {
        if (left > 0.0) bands.push_back(LayoutRect{0.0, top, left, middleHeight});
        if (right > 0.0) bands.push_back(LayoutRect{deviceSize.width - right, top, right, middleHeight});
    }
    return bands;
}

}
