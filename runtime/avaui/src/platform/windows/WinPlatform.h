#pragma once

#include "../contract/IPlatform.h"
#include "WinMouse.h"
#include "WinKeyboard.h"
#include "WinCursor.h"
#include "WinClipboard.h"
#include "WinDisplay.h"
#include "WinPlatformPaths.h"
#include "WinInputState.h"
#include "WinPlatformServices.h"

namespace avalang {
namespace ui {
namespace platform {

class WinPlatform final : public IPlatform {
public:
    AppSurface* CreateSurface() override;

    ava::platform::ui::IMouse& Mouse() override { return mouse_; }
    ava::platform::ui::IKeyboard& Keyboard() override { return keyboard_; }
    ava::platform::ui::ICursor& Cursor() override { return cursor_; }
    ava::platform::ui::IClipboard& Clipboard() override { return clipboard_; }
    ava::platform::ui::IDisplay& Display() override { return display_; }
    IPlatformPaths& Paths() override { return paths_; }

    ava::platform::ui::IWheel& Wheel() override { return wheel_; }
    ava::platform::ui::ITextInput& TextInput() override { return textInput_; }
    ava::platform::ui::ITouch& Touch() override { return touch_; }
    ava::platform::ui::IIme& Ime() override { return ime_; }
    ava::platform::ui::IPlatformServices& PlatformServices() override { return platformServices_; }

private:
    windows::WinMouse mouse_;
    windows::WinKeyboard keyboard_;
    windows::WinCursor cursor_;
    windows::WinClipboard clipboard_;
    windows::WinDisplay display_;
    windows::WinPlatformPaths paths_;
    windows::WinWheel wheel_;
    windows::WinTextInput textInput_;
    windows::WinTouch touch_;
    windows::WinIme ime_;
    windows::WinPlatformServices platformServices_;
};

}
}
}