#ifndef AVA_PLATFORM_SERVICES_UI_IRENDERSURFACE_H
#define AVA_PLATFORM_SERVICES_UI_IRENDERSURFACE_H

namespace ava {
namespace platform {
namespace ui {

// Phase 0 stub. Represents a native drawing surface only (HWND / Wayland /
// X11 / NSView); it does NOT perform rendering -- the renderer itself
// belongs to avalang.ui.dll (see docs/Platform_Foundation.md).
class IRenderSurface {
public:
    virtual ~IRenderSurface() = default;

    virtual void* NativeHandle() const = 0;
    virtual int Width() const = 0;
    virtual int Height() const = 0;
};

} // namespace ui
} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_SERVICES_UI_IRENDERSURFACE_H
