#pragma once

#include "AppSurface.h"
#include "IPlatformPaths.h"
#include "Export.h"
#include "runtime/avalang/platform/interfaces/services/ui/UIPlatformInterfaces.h"

namespace avalang {
namespace ui {
namespace platform {

class IPlatform {
public:
    virtual ~IPlatform() = default;

    virtual AppSurface* CreateSurface() = 0;

    virtual ava::platform::ui::IMouse& Mouse() = 0;
    virtual ava::platform::ui::IKeyboard& Keyboard() = 0;
    virtual ava::platform::ui::ICursor& Cursor() = 0;
    virtual ava::platform::ui::IClipboard& Clipboard() = 0;
    virtual ava::platform::ui::IDisplay& Display() = 0;
    virtual IPlatformPaths& Paths() = 0;

    virtual ava::platform::ui::IWheel& Wheel() = 0;
    virtual ava::platform::ui::ITextInput& TextInput() = 0;
    virtual ava::platform::ui::ITouch& Touch() = 0;
    virtual ava::platform::ui::IIme& Ime() = 0;
    virtual ava::platform::ui::IPlatformServices& PlatformServices() = 0;

    AVA_UI_API static void SetInstance(IPlatform* instance);
    AVA_UI_API static IPlatform& Current();
};

AVA_UI_API IPlatform& GetPlatform();

}
}
}