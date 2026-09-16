#include "AndroidSafeArea.h"

namespace avalang {
namespace ui {
namespace platform {
namespace android {

void AndroidSafeArea::SetObserver(ava::platform::mobile::ISafeAreaObserver* observer) {
    observer_ = observer;
}

ava::platform::mobile::SafeAreaInsets AndroidSafeArea::Insets() const {
    return insets_;
}

void AndroidSafeArea::NotifyChanged(float top, float right, float bottom, float left) {
    insets_.top = top;
    insets_.right = right;
    insets_.bottom = bottom;
    insets_.left = left;
    if (observer_) observer_->OnSafeAreaChanged(insets_);
}

AndroidSafeArea& GetSafeArea() {
    static AndroidSafeArea instance;
    return instance;
}

} // namespace android
} // namespace platform
} // namespace ui
} // namespace avalang
