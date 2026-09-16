#include "AndroidVirtualKeyboard.h"
#include "AndroidJNI.h"

namespace avalang {
namespace ui {
namespace platform {
namespace android {

void AndroidVirtualKeyboard::SetObserver(ava::platform::mobile::IVirtualKeyboardObserver* observer) {
    observer_ = observer;
}

void AndroidVirtualKeyboard::Show() {
    Bridge_ShowKeyboard();
}

void AndroidVirtualKeyboard::Hide() {
    Bridge_HideKeyboard();
}

bool AndroidVirtualKeyboard::IsVisible() const {
    return visible_;
}

int AndroidVirtualKeyboard::OccupiedHeight() const {
    return occupiedHeight_;
}

void AndroidVirtualKeyboard::NotifyVisibility(bool visible, int occupiedHeight) {
    visible_ = visible;
    occupiedHeight_ = visible ? occupiedHeight : 0;
    if (!observer_) return;
    if (visible) {
        observer_->OnVirtualKeyboardShown(occupiedHeight_);
    } else {
        observer_->OnVirtualKeyboardHidden();
    }
}

AndroidVirtualKeyboard& GetVirtualKeyboard() {
    static AndroidVirtualKeyboard instance;
    return instance;
}

} // namespace android
} // namespace platform
} // namespace ui
} // namespace avalang
