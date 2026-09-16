#include "AndroidAppSurface.h"
#include "AndroidMobileSurface.h"

namespace avalang {
namespace ui {
namespace platform {
namespace android {

AndroidAppSurface::AndroidAppSurface() {
    GetMobileSurface().SetObserver(this);
}

AndroidAppSurface::~AndroidAppSurface() {
    GetMobileSurface().SetObserver(nullptr);
}

void AndroidAppSurface::Create(int, int, const char*) {
}

void AndroidAppSurface::Resize(int, int) {
}

void AndroidAppSurface::Show() {
}

void AndroidAppSurface::Hide() {
}

void* AndroidAppSurface::NativeHandle() const {
    return nativeHandle_;
}

bool AndroidAppSurface::ProcessEvents() {
    return !closed_;
}

bool AndroidAppSurface::IsClosed() const {
    return closed_;
}

void AndroidAppSurface::OnSurfaceCreated(void* nativeHandle, int width, int height) {
    nativeHandle_ = nativeHandle;
    width_ = width;
    height_ = height;
    closed_ = false;
}

void AndroidAppSurface::OnSurfaceResized(int width, int height) {
    width_ = width;
    height_ = height;
}

void AndroidAppSurface::OnSurfaceDestroyed() {
    nativeHandle_ = nullptr;
    closed_ = true;
}

} // namespace android
} // namespace platform
} // namespace ui
} // namespace avalang
