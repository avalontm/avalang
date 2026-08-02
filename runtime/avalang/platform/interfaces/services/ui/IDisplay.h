#ifndef AVA_PLATFORM_SERVICES_UI_IDISPLAY_H
#define AVA_PLATFORM_SERVICES_UI_IDISPLAY_H

namespace ava {
namespace platform {
namespace ui {

// Phase 0 stub. Monitor information required by avalang.ui.dll
// (see docs/Platform_Foundation.md).
struct DisplayInfo {
    int width = 0;
    int height = 0;
    float dpi = 96.0f;
    float scaling = 1.0f;
};

class IDisplay {
public:
    virtual ~IDisplay() = default;

    virtual int MonitorCount() const = 0;
    virtual DisplayInfo Monitor(int index) const = 0;
    virtual DisplayInfo PrimaryMonitor() const = 0;
};

} // namespace ui
} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_SERVICES_UI_IDISPLAY_H
