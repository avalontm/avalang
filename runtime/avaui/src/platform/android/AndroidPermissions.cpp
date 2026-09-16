#include "AndroidPermissions.h"
#include "AndroidJNI.h"

namespace avalang {
namespace ui {
namespace platform {
namespace android {

void AndroidPermissions::SetObserver(ava::platform::mobile::IPermissionsObserver* observer) {
    observer_ = observer;
}

ava::platform::mobile::PermissionStatus AndroidPermissions::Check(const std::string& permission) const {
    return Bridge_CheckPermission(permission)
        ? ava::platform::mobile::PermissionStatus::Granted
        : ava::platform::mobile::PermissionStatus::NotDetermined;
}

void AndroidPermissions::Request(const std::string& permission) {
    Bridge_RequestPermission(permission, 0);
}

void AndroidPermissions::NotifyResult(const std::string& permission, bool granted) {
    if (!observer_) return;
    observer_->OnPermissionResult(permission,
        granted ? ava::platform::mobile::PermissionStatus::Granted
                : ava::platform::mobile::PermissionStatus::Denied);
}

AndroidPermissions& GetPermissions() {
    static AndroidPermissions instance;
    return instance;
}

} // namespace android
} // namespace platform
} // namespace ui
} // namespace avalang
