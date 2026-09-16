#ifndef AVA_PLATFORM_SERVICES_MOBILE_IVIRTUALKEYBOARD_H
#define AVA_PLATFORM_SERVICES_MOBILE_IVIRTUALKEYBOARD_H

namespace ava {
namespace platform {
namespace mobile {

class IVirtualKeyboardObserver {
public:
    virtual ~IVirtualKeyboardObserver() = default;

    virtual void OnVirtualKeyboardShown(int occupiedHeight) = 0;
    virtual void OnVirtualKeyboardHidden() = 0;
};

class IVirtualKeyboard {
public:
    virtual ~IVirtualKeyboard() = default;

    virtual void SetObserver(IVirtualKeyboardObserver* observer) = 0;
    virtual void Show() = 0;
    virtual void Hide() = 0;
    virtual bool IsVisible() const = 0;
    virtual int OccupiedHeight() const = 0;
};

} // namespace mobile
} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_SERVICES_MOBILE_IVIRTUALKEYBOARD_H
