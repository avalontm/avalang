#ifndef AVA_PLATFORM_SERVICES_UI_ICURSOR_H
#define AVA_PLATFORM_SERVICES_UI_ICURSOR_H

namespace ava {
namespace platform {
namespace ui {

// Phase 0 stub. Reserved input-layer extension point for avalang.ui.dll
// (see docs/Platform_Foundation.md).
enum class CursorShape {
    Arrow,
    IBeam,
    Hand,
    ResizeHorizontal,
    ResizeVertical,
};

class ICursor {
public:
    virtual ~ICursor() = default;

    virtual void SetShape(CursorShape shape) = 0;
    virtual void SetVisible(bool visible) = 0;
};

} // namespace ui
} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_SERVICES_UI_ICURSOR_H
