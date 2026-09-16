#include "AndroidAppLifecycle.h"

#include <algorithm>

namespace avalang {
namespace ui {
namespace platform {
namespace android {

void AndroidAppLifecycle::AddObserver(ava::platform::mobile::IAppLifecycleObserver* observer) {
    if (!observer) return;
    if (std::find(observers_.begin(), observers_.end(), observer) != observers_.end()) return;
    observers_.push_back(observer);
}

void AndroidAppLifecycle::RemoveObserver(ava::platform::mobile::IAppLifecycleObserver* observer) {
    observers_.erase(std::remove(observers_.begin(), observers_.end(), observer), observers_.end());
}

ava::platform::mobile::AppLifecycleState AndroidAppLifecycle::CurrentState() const {
    return state_;
}

void AndroidAppLifecycle::SetState(ava::platform::mobile::AppLifecycleState state) {
    state_ = state;
    for (auto* observer : observers_) {
        observer->OnStateChanged(state);
    }
}

void AndroidAppLifecycle::NotifyLowMemory() {
    for (auto* observer : observers_) {
        observer->OnLowMemory();
    }
}

AndroidAppLifecycle& GetAppLifecycle() {
    static AndroidAppLifecycle instance;
    return instance;
}

} // namespace android
} // namespace platform
} // namespace ui
} // namespace avalang
