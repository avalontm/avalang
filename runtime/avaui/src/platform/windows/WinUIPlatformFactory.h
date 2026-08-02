#ifndef AVA_UI_PLATFORM_WINDOWS_WINUIPLATFORMFACTORY_H
#define AVA_UI_PLATFORM_WINDOWS_WINUIPLATFORMFACTORY_H

#include "../IUIPlatformFactory.h"
#include "WinMouse.h"
#include "WinKeyboard.h"
#include "WinCursor.h"
#include "WinClipboard.h"
#include "WinDisplay.h"

namespace avalang {
namespace ui {
namespace platform {

// Windows backend. Only backend in active development (see
// docs/AVALANG_UI_PROGRESS.md). Phase 11 (Native Backend): CreateWindow()
// / CreateRenderSurface() return real Win32 objects (WinWindow,
// WinRenderSurface); Mouse()/Keyboard()/Cursor()/Clipboard()/Display()
// return the real singletons below instead of no-ops.
class WinUIPlatformFactory final : public IUIPlatformFactory {
public:
    ava::platform::ui::IWindow* CreateWindow() override;
    ava::platform::ui::IRenderSurface* CreateRenderSurface(ava::platform::ui::IWindow* window) override;

    ava::platform::ui::IMouse& Mouse() override { return mouse_; }
    ava::platform::ui::IKeyboard& Keyboard() override { return keyboard_; }
    ava::platform::ui::ICursor& Cursor() override { return cursor_; }
    ava::platform::ui::IClipboard& Clipboard() override { return clipboard_; }
    ava::platform::ui::IDisplay& Display() override { return display_; }

private:
    windows::WinMouse mouse_;
    windows::WinKeyboard keyboard_;
    windows::WinCursor cursor_;
    windows::WinClipboard clipboard_;
    windows::WinDisplay display_;
};

} // namespace platform
} // namespace ui
} // namespace avalang

#endif // AVA_UI_PLATFORM_WINDOWS_WINUIPLATFORMFACTORY_H
