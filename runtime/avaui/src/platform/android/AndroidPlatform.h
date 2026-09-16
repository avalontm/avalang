#pragma once

#include "../contract/IPlatform.h"
#include "../StubUIServices.h"
#include "AndroidClipboard.h"
#include "AndroidDisplayMetrics.h"
#include "AndroidPlatformPaths.h"
#include "AndroidInputState.h"
#include "AndroidPlatformServices.h"

namespace avalang {
namespace ui {
namespace platform {
namespace android {

class AndroidPlatform final : public IPlatform {
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
    stub::StubMouse mouse_;
    stub::StubKeyboard keyboard_;
    stub::StubCursor cursor_;
    AndroidClipboard clipboard_;
    AndroidDisplayMetrics display_;
    AndroidPlatformPaths paths_;
    stub::StubWheel wheel_;
    AndroidTextInput textInput_;
    AndroidTouch touch_;
    AndroidIme ime_;
    AndroidPlatformServices platformServices_;
};

} // namespace android
} // namespace platform
} // namespace ui
} // namespace avalang
