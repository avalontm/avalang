#include "AndroidDisplayMetrics.h"
#include "AndroidJNI.h"

namespace avalang {
namespace ui {
namespace platform {
namespace android {

namespace {

ava::platform::ui::DisplayInfo QueryInfo() {
    int width = 0, height = 0, densityDpi = 160;
    float density = 1.0f;
    Bridge_QueryDisplayMetrics(width, height, density, densityDpi);

    ava::platform::ui::DisplayInfo info;
    info.width = width;
    info.height = height;
    info.dpi = static_cast<float>(densityDpi);
    info.scaling = density;
    return info;
}

ava::platform::mobile::DensityBucket BucketFor(int densityDpi) {
    if (densityDpi <= 120) return ava::platform::mobile::DensityBucket::LDPI;
    if (densityDpi <= 160) return ava::platform::mobile::DensityBucket::MDPI;
    if (densityDpi <= 240) return ava::platform::mobile::DensityBucket::HDPI;
    if (densityDpi <= 320) return ava::platform::mobile::DensityBucket::XHDPI;
    if (densityDpi <= 480) return ava::platform::mobile::DensityBucket::XXHDPI;
    return ava::platform::mobile::DensityBucket::XXXHDPI;
}

}

int AndroidDisplayMetrics::MonitorCount() const {
    return 1;
}

ava::platform::ui::DisplayInfo AndroidDisplayMetrics::Monitor(int index) const {
    if (index != 0) return ava::platform::ui::DisplayInfo{};
    return QueryInfo();
}

ava::platform::ui::DisplayInfo AndroidDisplayMetrics::PrimaryMonitor() const {
    return QueryInfo();
}

ava::platform::mobile::DensityBucket AndroidDisplayMetrics::Density() const {
    int width = 0, height = 0, densityDpi = 160;
    float density = 1.0f;
    Bridge_QueryDisplayMetrics(width, height, density, densityDpi);
    return BucketFor(densityDpi);
}

} // namespace android
} // namespace platform
} // namespace ui
} // namespace avalang
