#ifndef AVA_PLATFORM_SERVICES_UI_IPLATFORMSERVICES_H
#define AVA_PLATFORM_SERVICES_UI_IPLATFORMSERVICES_H

#include <string>
#include <vector>

namespace ava {
namespace platform {
namespace ui {

// Phase 0 stub. Optional higher-level OS services (file dialog, native
// notifications, drag & drop, URI launcher) -- see
// docs/Platform_Foundation.md. Not required by avalang.dll today.
class IPlatformServices {
public:
    virtual ~IPlatformServices() = default;

    virtual bool OpenFileDialog(std::string& outPath) = 0;
    virtual bool SaveFileDialog(std::string& outPath) = 0;

    virtual void ShowNotification(const std::string& title, const std::string& body) = 0;

    virtual void OpenUri(const std::string& uri) = 0;
};

} // namespace ui
} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_SERVICES_UI_IPLATFORMSERVICES_H
