#ifndef AVA_PLATFORM_SERVICES_MOBILE_ISAFEAREA_H
#define AVA_PLATFORM_SERVICES_MOBILE_ISAFEAREA_H

namespace ava {
namespace platform {
namespace mobile {

struct SafeAreaInsets {
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
    float left = 0.0f;
};

class ISafeAreaObserver {
public:
    virtual ~ISafeAreaObserver() = default;

    virtual void OnSafeAreaChanged(const SafeAreaInsets& insets) = 0;
};

class ISafeArea {
public:
    virtual ~ISafeArea() = default;

    virtual void SetObserver(ISafeAreaObserver* observer) = 0;
    virtual SafeAreaInsets Insets() const = 0;
};

} // namespace mobile
} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_SERVICES_MOBILE_ISAFEAREA_H
