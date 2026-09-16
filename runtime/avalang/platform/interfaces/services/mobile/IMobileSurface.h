#ifndef AVA_PLATFORM_SERVICES_MOBILE_IMOBILESURFACE_H
#define AVA_PLATFORM_SERVICES_MOBILE_IMOBILESURFACE_H

namespace ava {
namespace platform {
namespace mobile {

class IMobileSurfaceObserver {
public:
    virtual ~IMobileSurfaceObserver() = default;

    virtual void OnSurfaceCreated(void* nativeHandle, int width, int height) = 0;
    virtual void OnSurfaceResized(int width, int height) = 0;
    virtual void OnSurfaceDestroyed() = 0;
};

class IMobileSurface {
public:
    virtual ~IMobileSurface() = default;

    virtual void SetObserver(IMobileSurfaceObserver* observer) = 0;
    virtual void* NativeHandle() const = 0;
    virtual int Width() const = 0;
    virtual int Height() const = 0;
};

} // namespace mobile
} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_SERVICES_MOBILE_IMOBILESURFACE_H
