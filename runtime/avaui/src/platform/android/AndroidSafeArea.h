#pragma once

#include "runtime/avalang/platform/interfaces/services/mobile/ISafeArea.h"

namespace avalang {
namespace ui {
namespace platform {
namespace android {

class AndroidSafeArea final : public ava::platform::mobile::ISafeArea {
public:
    void SetObserver(ava::platform::mobile::ISafeAreaObserver* observer) override;
    ava::platform::mobile::SafeAreaInsets Insets() const override;

    void NotifyChanged(float top, float right, float bottom, float left);

private:
    ava::platform::mobile::ISafeAreaObserver* observer_ = nullptr;
    ava::platform::mobile::SafeAreaInsets insets_;
};

AndroidSafeArea& GetSafeArea();

} // namespace android
} // namespace platform
} // namespace ui
} // namespace avalang
