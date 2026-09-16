#ifndef AVA_PLATFORM_SERVICES_MOBILE_IPERMISSIONS_H
#define AVA_PLATFORM_SERVICES_MOBILE_IPERMISSIONS_H

#include <string>

namespace ava {
namespace platform {
namespace mobile {

enum class PermissionStatus {
    Granted,
    Denied,
    NotDetermined,
};

class IPermissionsObserver {
public:
    virtual ~IPermissionsObserver() = default;

    virtual void OnPermissionResult(const std::string& permission, PermissionStatus status) = 0;
};

class IPermissions {
public:
    virtual ~IPermissions() = default;

    virtual void SetObserver(IPermissionsObserver* observer) = 0;
    virtual PermissionStatus Check(const std::string& permission) const = 0;
    virtual void Request(const std::string& permission) = 0;
};

} // namespace mobile
} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_SERVICES_MOBILE_IPERMISSIONS_H
