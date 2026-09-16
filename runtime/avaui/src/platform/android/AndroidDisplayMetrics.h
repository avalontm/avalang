#pragma once

#include "runtime/avalang/platform/interfaces/services/mobile/IMobileDisplayMetrics.h"

namespace avalang {
namespace ui {
namespace platform {
namespace android {

class AndroidDisplayMetrics final : public ava::platform::mobile::IMobileDisplayMetrics {
public:
    int MonitorCount() const override;
    ava::platform::ui::DisplayInfo Monitor(int index) const override;
    ava::platform::ui::DisplayInfo PrimaryMonitor() const override;
    ava::platform::mobile::DensityBucket Density() const override;
};

} // namespace android
} // namespace platform
} // namespace ui
} // namespace avalang
