#pragma once

#include "runtime/avalang/platform/interfaces/services/mobile/IVirtualKeyboard.h"

namespace avalang {
namespace ui {
namespace platform {
namespace android {

class AndroidVirtualKeyboard final : public ava::platform::mobile::IVirtualKeyboard {
public:
    void SetObserver(ava::platform::mobile::IVirtualKeyboardObserver* observer) override;
    void Show() override;
    void Hide() override;
    bool IsVisible() const override;
    int OccupiedHeight() const override;

    void NotifyVisibility(bool visible, int occupiedHeight);

private:
    ava::platform::mobile::IVirtualKeyboardObserver* observer_ = nullptr;
    bool visible_ = false;
    int occupiedHeight_ = 0;
};

AndroidVirtualKeyboard& GetVirtualKeyboard();

} // namespace android
} // namespace platform
} // namespace ui
} // namespace avalang
