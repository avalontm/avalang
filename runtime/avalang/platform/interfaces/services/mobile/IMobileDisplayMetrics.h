#ifndef AVA_PLATFORM_SERVICES_MOBILE_IMOBILEDISPLAYMETRICS_H
#define AVA_PLATFORM_SERVICES_MOBILE_IMOBILEDISPLAYMETRICS_H

#include "../ui/IDisplay.h"

namespace ava {
namespace platform {
namespace mobile {

enum class DensityBucket {
    LDPI,
    MDPI,
    HDPI,
    XHDPI,
    XXHDPI,
    XXXHDPI,
};

class IMobileDisplayMetrics : public ava::platform::ui::IDisplay {
public:
    virtual DensityBucket Density() const = 0;
};

} // namespace mobile
} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_SERVICES_MOBILE_IMOBILEDISPLAYMETRICS_H
