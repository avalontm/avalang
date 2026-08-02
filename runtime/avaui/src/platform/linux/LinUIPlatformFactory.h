#ifndef AVA_UI_PLATFORM_LINUX_LINUIPLATFORMFACTORY_H
#define AVA_UI_PLATFORM_LINUX_LINUIPLATFORMFACTORY_H

#include "../IUIPlatformFactory.h"
#include "../StubUIServices.h"

namespace avalang {
namespace ui {
namespace platform {

// Linux backend. STUB -- en estudio / pausado a propósito (ver
// docs/AVALANG_UI_PROGRESS.md, "Alcance actual"). Compila, no
// implementa nada real, incluyendo tras Fase 11 (Native Backend):
// esa fase solo aplica a Windows. Todos los métodos delegan a las
// clases no-op de StubUIServices.h.
class LinUIPlatformFactory final : public IUIPlatformFactory {
public:
    ava::platform::ui::IWindow* CreateWindow() override;
    ava::platform::ui::IRenderSurface* CreateRenderSurface(ava::platform::ui::IWindow* window) override;
    ava::platform::ui::IMouse& Mouse() override { return mouse_; }
    ava::platform::ui::IKeyboard& Keyboard() override { return keyboard_; }
    ava::platform::ui::ICursor& Cursor() override { return cursor_; }
    ava::platform::ui::IClipboard& Clipboard() override { return clipboard_; }
    ava::platform::ui::IDisplay& Display() override { return display_; }

private:
    stub::StubMouse mouse_;
    stub::StubKeyboard keyboard_;
    stub::StubCursor cursor_;
    stub::StubClipboard clipboard_;
    stub::StubDisplay display_;
};

} // namespace platform
} // namespace ui
} // namespace avalang

#endif // AVA_UI_PLATFORM_LINUX_LINUIPLATFORMFACTORY_H
