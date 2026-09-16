#ifndef AVA_PLATFORM_SERVICES_UI_IWHEEL_H
#define AVA_PLATFORM_SERVICES_UI_IWHEEL_H

namespace ava {
namespace platform {
namespace ui {

class IWheel {
public:
    virtual ~IWheel() = default;

    virtual void ConsumeDelta(float& outDeltaX, float& outDeltaY) = 0;
};

} // namespace ui
} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_SERVICES_UI_IWHEEL_H
