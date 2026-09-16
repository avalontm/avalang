#pragma once

#include "runtime/avalang/platform/interfaces/services/mobile/IPermissions.h"

namespace avalang {
namespace ui {
namespace platform {
namespace android {

class AndroidPermissions final : public ava::platform::mobile::IPermissions {
public:
    void SetObserver(ava::platform::mobile::IPermissionsObserver* observer) override;
    ava::platform::mobile::PermissionStatus Check(const std::string& permission) const override;
    void Request(const std::string& permission) override;

    void NotifyResult(const std::string& permission, bool granted);

private:
    ava::platform::mobile::IPermissionsObserver* observer_ = nullptr;
};

AndroidPermissions& GetPermissions();

} // namespace android
} // namespace platform
} // namespace ui
} // namespace avalang
