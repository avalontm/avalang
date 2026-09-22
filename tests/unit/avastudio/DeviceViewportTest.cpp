#include <cstdlib>
#include <iostream>

#include "designer/device_viewport.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* expression, int line) {
    if (condition) return;
    std::cerr << "FAIL line " << line << ": " << expression << "\n";
    ++g_failures;
}

#define CHECK(expr) Check((expr), #expr, __LINE__)

const studio::designer::DeviceProfile& FindProfile(const char* prefix) {
    for (const studio::designer::DeviceProfile& profile : studio::designer::DeviceProfiles()) {
        if (profile.name.rfind(prefix, 0) == 0) return profile;
    }
    std::abort();
}

void ResponsiveProfileUsesAvailableSpace() {
    using namespace studio::designer;
    const DeviceViewport view =
        ResolveDeviceViewport(FindProfile("Responsive"), DeviceOrientation::Rotated, true, LayoutSize{640.0, 480.0});
    CHECK(!view.fixed);
    CHECK(view.deviceSize.width == 640.0 && view.deviceSize.height == 480.0);
    CHECK(view.contentRect.width == 640.0 && view.contentRect.height == 480.0);
    CHECK(view.insets.top == 0.0f && view.insets.left == 0.0f);
}

void FixedProfileUsesExactDeviceSize() {
    using namespace studio::designer;
    const DeviceViewport view =
        ResolveDeviceViewport(FindProfile("Android Phone"), DeviceOrientation::Default, false, LayoutSize{1000.0, 1000.0});
    CHECK(view.fixed);
    CHECK(view.deviceSize.width == 412.0 && view.deviceSize.height == 915.0);
    CHECK(view.contentRect.x == 0.0 && view.contentRect.y == 0.0);
    CHECK(view.contentRect.width == 412.0 && view.contentRect.height == 915.0);
}

void RotationSwapsSizeAndRotatesInsets() {
    using namespace studio::designer;
    const DeviceProfile& phone = FindProfile("iOS Phone");
    const DeviceViewport view = ResolveDeviceViewport(phone, DeviceOrientation::Rotated, false, LayoutSize{});
    CHECK(view.deviceSize.width == 844.0 && view.deviceSize.height == 390.0);
    CHECK(view.insets.top == phone.safeArea.left);
    CHECK(view.insets.right == phone.safeArea.top);
    CHECK(view.insets.bottom == phone.safeArea.right);
    CHECK(view.insets.left == phone.safeArea.bottom);
}

void DesktopIgnoresRotation() {
    using namespace studio::designer;
    const DeviceProfile& desktop = FindProfile("Desktop");
    CHECK(!SupportsRotation(desktop));
    const DeviceViewport view = ResolveDeviceViewport(desktop, DeviceOrientation::Rotated, true, LayoutSize{});
    CHECK(view.deviceSize.width == 1280.0 && view.deviceSize.height == 720.0);
}

void CustomProfileRotates() {
    using namespace studio::designer;
    const DeviceProfile custom = MakeCustomProfile("Custom", 500.0, 300.0);
    CHECK(SupportsRotation(custom));
    const DeviceViewport view = ResolveDeviceViewport(custom, DeviceOrientation::Rotated, true, LayoutSize{});
    CHECK(view.deviceSize.width == 300.0 && view.deviceSize.height == 500.0);
}

void SafeAreaShrinksContentRect() {
    using namespace studio::designer;
    const DeviceProfile& phone = FindProfile("iOS Phone");
    const DeviceViewport respected = ResolveDeviceViewport(phone, DeviceOrientation::Default, true, LayoutSize{});
    CHECK(respected.deviceSize.width == 390.0 && respected.deviceSize.height == 844.0);
    CHECK(respected.contentRect.x == 0.0 && respected.contentRect.y == 47.0);
    CHECK(respected.contentRect.width == 390.0);
    CHECK(respected.contentRect.height == 844.0 - 47.0 - 34.0);
    const DeviceViewport ignored = ResolveDeviceViewport(phone, DeviceOrientation::Default, false, LayoutSize{});
    CHECK(ignored.contentRect.y == 0.0 && ignored.contentRect.height == 844.0);
}

void SafeAreaWithRotationInsetsSides() {
    using namespace studio::designer;
    const DeviceViewport view =
        ResolveDeviceViewport(FindProfile("iOS Phone"), DeviceOrientation::Rotated, true, LayoutSize{});
    CHECK(view.contentRect.x == 34.0 && view.contentRect.y == 0.0);
    CHECK(view.contentRect.width == 844.0 - 34.0 - 47.0);
    CHECK(view.contentRect.height == 390.0);
}

void BandsCoverExactlyTheInsets() {
    using namespace studio::designer;
    const SafeAreaInsets insets{47.0f, 10.0f, 34.0f, 20.0f};
    const std::vector<LayoutRect> bands = SafeAreaBands(LayoutSize{390.0, 844.0}, insets);
    CHECK(bands.size() == 4);
    double area = 0.0;
    for (const LayoutRect& band : bands) area += band.width * band.height;
    const double expected = 390.0 * 844.0 - (390.0 - 30.0) * (844.0 - 81.0);
    CHECK(area == expected);
    CHECK(SafeAreaBands(LayoutSize{390.0, 844.0}, SafeAreaInsets{}).empty());
    CHECK(!HasSafeArea(SafeAreaInsets{}));
    CHECK(HasSafeArea(insets));
}

void OversizedInsetsAreClamped() {
    using namespace studio::designer;
    const std::vector<LayoutRect> bands =
        SafeAreaBands(LayoutSize{100.0, 100.0}, SafeAreaInsets{80.0f, 80.0f, 80.0f, 80.0f});
    double area = 0.0;
    for (const LayoutRect& band : bands) area += band.width * band.height;
    CHECK(area <= 100.0 * 100.0);
}

void CustomSizeIsClamped() {
    using namespace studio::designer;
    const LayoutSize small = ClampCustomSize(10.0, 20.0);
    CHECK(small.width == kMinCustomDimension && small.height == kMinCustomDimension);
    const LayoutSize large = ClampCustomSize(99999.0, 5000.0);
    CHECK(large.width == kMaxCustomDimension && large.height == kMaxCustomDimension);
    const LayoutSize normal = ClampCustomSize(390.0, 844.0);
    CHECK(normal.width == 390.0 && normal.height == 844.0);
}

}

int main() {
    ResponsiveProfileUsesAvailableSpace();
    FixedProfileUsesExactDeviceSize();
    RotationSwapsSizeAndRotatesInsets();
    DesktopIgnoresRotation();
    CustomProfileRotates();
    SafeAreaShrinksContentRect();
    SafeAreaWithRotationInsetsSides();
    BandsCoverExactlyTheInsets();
    OversizedInsetsAreClamped();
    CustomSizeIsClamped();
    if (g_failures == 0) std::cout << "all passed\n";
    return g_failures == 0 ? 0 : 1;
}
