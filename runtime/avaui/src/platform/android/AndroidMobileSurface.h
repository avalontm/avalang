#pragma once

#include "runtime/avalang/platform/interfaces/services/mobile/IMobileSurface.h"

namespace avalang {
namespace ui {
namespace platform {
namespace android {

class AndroidMobileSurface final : public ava::platform::mobile::IMobileSurface {
public:
    void SetObserver(ava::platform::mobile::IMobileSurfaceObserver* observer) override;
    void* NativeHandle() const override;
    int Width() const override;
    int Height() const override;

    void NotifyCreated(void* nativeHandle, int width, int height);
    void NotifyResized(int width, int height);
    void NotifyDestroyed();

private:
    ava::platform::mobile::IMobileSurfaceObserver* observer_ = nullptr;
    void* nativeHandle_ = nullptr;
    int width_ = 0;
    int height_ = 0;
};

AndroidMobileSurface& GetMobileSurface();

} // namespace android
} // namespace platform
} // namespace ui
} // namespace avalang
