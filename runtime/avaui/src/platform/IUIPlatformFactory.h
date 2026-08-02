#ifndef AVA_UI_PLATFORM_IUIPLATFORMFACTORY_H
#define AVA_UI_PLATFORM_IUIPLATFORMFACTORY_H

// Internal interface. Bridges avalang.ui.dll to the UI service stubs
// already reserved in avalang.dll's PAL (core/platform/interfaces/
// services/ui/, Phase 0 of docs/PAL_PROGRESS.md). One implementation
// per OS backend, selected at compile time in ui/CMakeLists.txt --
// same convention as ava::platform::IPlatform, no runtime OS
// detection.
//
// Phase 1-10: declaration only, every backend returned nullptr /
// stub singletons. Phase 11 (Native Backend) wires this up for real
// on Windows: CreateWindow() returns a live Win32 window,
// CreateRenderSurface() a surface attached to it, and Mouse()/
// Keyboard()/Cursor()/Clipboard()/Display() return real input/OS
// services instead of no-ops. Linux and macOS stay STUB / en estudio
// (see docs/AVALANG_UI_PROGRESS.md, "Alcance actual") -- their
// factories implement this same interface with the shared no-op
// classes in platform/StubUIServices.h so the module keeps compiling
// on those targets without any real behavior.

#include "../../../avalang/platform/interfaces/services/ui/UIPlatformInterfaces.h"

namespace avalang {
namespace ui {
namespace platform {

class IUIPlatformFactory {
public:
    virtual ~IUIPlatformFactory() = default;

    // Ownership: caller owns the returned instance.
    virtual ava::platform::ui::IWindow* CreateWindow() = 0;

    // A render surface is always attached to a window (Phase 11: on
    // Windows this is the same HWND, obtained via window->NativeHandle()).
    // `window` must outlive the returned surface.
    virtual ava::platform::ui::IRenderSurface* CreateRenderSurface(ava::platform::ui::IWindow* window) = 0;

    // Shared, long-lived OS services -- not per-window, so exposed as
    // references to backend-owned singletons rather than factory
    // methods (same convention as ava::platform::IPlatform::Timer(),
    // ::Console(), etc.).
    virtual ava::platform::ui::IMouse& Mouse() = 0;
    virtual ava::platform::ui::IKeyboard& Keyboard() = 0;
    virtual ava::platform::ui::ICursor& Cursor() = 0;
    virtual ava::platform::ui::IClipboard& Clipboard() = 0;
    virtual ava::platform::ui::IDisplay& Display() = 0;
};

// Returns the compile-time selected factory for the current OS.
// Implemented once per backend (windows/, linux/, macos/).
IUIPlatformFactory& GetUIPlatformFactory();

} // namespace platform
} // namespace ui
} // namespace avalang

#endif // AVA_UI_PLATFORM_IUIPLATFORMFACTORY_H
