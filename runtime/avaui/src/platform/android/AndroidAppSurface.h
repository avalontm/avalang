#pragma once

#include "../contract/AppSurface.h"
#include "runtime/avalang/platform/interfaces/services/mobile/IMobileSurface.h"

namespace avalang {
namespace ui {
namespace platform {
namespace android {

class AndroidAppSurface final : public AppSurface, public ava::platform::mobile::IMobileSurfaceObserver {
public:
    AndroidAppSurface();
    ~AndroidAppSurface() override;

    void Create(int width, int height, const char* title) override;
    void Resize(int width, int height) override;
    void Show() override;
    void Hide() override;
    void* NativeHandle() const override;
    bool ProcessEvents() override;
    bool IsClosed() const override;

    void OnSurfaceCreated(void* nativeHandle, int width, int height) override;
    void OnSurfaceResized(int width, int height) override;
    void OnSurfaceDestroyed() override;

private:
    void* nativeHandle_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    bool closed_ = false;
};

} // namespace android
} // namespace platform
} // namespace ui
} // namespace avalang
