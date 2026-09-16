#ifndef AVA_PLATFORM_SERVICES_MOBILE_IAPPLIFECYCLE_H
#define AVA_PLATFORM_SERVICES_MOBILE_IAPPLIFECYCLE_H

namespace ava {
namespace platform {
namespace mobile {

enum class AppLifecycleState {
    Created,
    Started,
    Resumed,
    Paused,
    Stopped,
    Destroyed,
};

class IAppLifecycleObserver {
public:
    virtual ~IAppLifecycleObserver() = default;

    virtual void OnStateChanged(AppLifecycleState state) = 0;
    virtual void OnLowMemory() = 0;
};

class IAppLifecycle {
public:
    virtual ~IAppLifecycle() = default;

    virtual void AddObserver(IAppLifecycleObserver* observer) = 0;
    virtual void RemoveObserver(IAppLifecycleObserver* observer) = 0;
    virtual AppLifecycleState CurrentState() const = 0;
};

} // namespace mobile
} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_SERVICES_MOBILE_IAPPLIFECYCLE_H
