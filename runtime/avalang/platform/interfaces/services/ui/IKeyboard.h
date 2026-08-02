#ifndef AVA_PLATFORM_SERVICES_UI_IKEYBOARD_H
#define AVA_PLATFORM_SERVICES_UI_IKEYBOARD_H

#include <cstdint>

namespace ava {
namespace platform {
namespace ui {

// Phase 0 stub. Reserved input-layer extension point for avalang.ui.dll;
// intentionally independent from rendering (see docs/Platform_Foundation.md).
class IKeyboard {
public:
    virtual ~IKeyboard() = default;

    virtual bool IsKeyDown(int keyCode) const = 0;
};

} // namespace ui
} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_SERVICES_UI_IKEYBOARD_H
