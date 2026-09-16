#include "AndroidMobileSurface.h"

namespace avalang {
namespace ui {
namespace platform {
namespace android {

void AndroidMobileSurface::SetObserver(ava::platform::mobile::IMobileSurfaceObserver* observer) {
    observer_ = observer;
}

void* AndroidMobileSurface::NativeHandle() const {
    return nativeHandle_;
}

int AndroidMobileSurface::Width() const {
    return width_;
}

int AndroidMobileSurface::Height() const {
    return height_;
}

void AndroidMobileSurface::NotifyCreated(void* nativeHandle, int width, int height) {
    nativeHandle_ = nativeHandle;
    width_ = width;
    height_ = height;
    if (observer_) observer_->OnSurfaceCreated(nativeHandle, width, height);
}

void AndroidMobileSurface::NotifyResized(int width, int height) {
    width_ = width;
    height_ = height;
    if (observer_) observer_->OnSurfaceResized(width, height);
}

void AndroidMobileSurface::NotifyDestroyed() {
    nativeHandle_ = nullptr;
    width_ = 0;
    height_ = 0;
    if (observer_) observer_->OnSurfaceDestroyed();
}

AndroidMobileSurface& GetMobileSurface() {
    static AndroidMobileSurface instance;
    return instance;
}

} // namespace android
} // namespace platform
} // namespace ui
} // namespace avalang
