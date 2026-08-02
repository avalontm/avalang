#ifndef AVA_PLATFORM_SERVICES_UI_IMOUSE_H
#define AVA_PLATFORM_SERVICES_UI_IMOUSE_H

namespace ava {
namespace platform {
namespace ui {

// Phase 0 stub. Reserved input-layer extension point for avalang.ui.dll;
// intentionally independent from rendering (see docs/Platform_Foundation.md).
enum class MouseButton {
    Left,
    Right,
    Middle,
};

class IMouse {
public:
    virtual ~IMouse() = default;

    virtual void Position(int& x, int& y) const = 0;
    virtual bool IsButtonDown(MouseButton button) const = 0;
};

} // namespace ui
} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_SERVICES_UI_IMOUSE_H
