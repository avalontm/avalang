#ifndef AVA_PLATFORM_SERVICES_UI_ITOUCH_H
#define AVA_PLATFORM_SERVICES_UI_ITOUCH_H

#include <cstdint>
#include <vector>

namespace ava {
namespace platform {
namespace ui {

struct NativeTouchPoint {
    uint64_t id = 0;
    int x = 0;
    int y = 0;
};

class ITouch {
public:
    virtual ~ITouch() = default;

    virtual std::vector<NativeTouchPoint> ActivePoints() const = 0;
};

} // namespace ui
} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_SERVICES_UI_ITOUCH_H
