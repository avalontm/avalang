#pragma once

#include "contract/AppSurface.h"
#include "contract/IPlatformPaths.h"
#include "runtime/avalang/platform/interfaces/services/ui/UIPlatformInterfaces.h"

namespace avalang {
namespace ui {
namespace platform {
namespace stub {

class StubAppSurface final : public AppSurface {
public:
    void Create(int, int, const char*) override {}
    void Resize(int, int) override {}
    void Show() override {}
    void Hide() override {}
    void* NativeHandle() const override { return nullptr; }
    bool ProcessEvents() override { return true; }
    bool IsClosed() const override { return false; }
};

class StubMouse final : public ava::platform::ui::IMouse {
public:
    void Position(int& x, int& y) const override { x = 0; y = 0; }
    bool IsButtonDown(ava::platform::ui::MouseButton) const override { return false; }
};

class StubKeyboard final : public ava::platform::ui::IKeyboard {
public:
    bool IsKeyDown(int) const override { return false; }
};

class StubCursor final : public ava::platform::ui::ICursor {
public:
    void SetShape(ava::platform::ui::CursorShape) override {}
    void SetVisible(bool) override {}
};

class StubClipboard final : public ava::platform::ui::IClipboard {
public:
    void SetText(const std::string&) override {}
    std::string GetText() const override { return {}; }
    bool HasText() const override { return false; }
};

class StubDisplay final : public ava::platform::ui::IDisplay {
public:
    int MonitorCount() const override { return 0; }
    ava::platform::ui::DisplayInfo Monitor(int) const override { return {}; }
    ava::platform::ui::DisplayInfo PrimaryMonitor() const override { return {}; }
};

class StubPlatformPaths final : public IPlatformPaths {
public:
    std::string FontsDir() const override { return "/usr/share/fonts"; }
    std::string SystemDir() const override { return {}; }
    std::string AppDataDir() const override { return {}; }
};

class StubWheel final : public ava::platform::ui::IWheel {
public:
    void ConsumeDelta(float& outDeltaX, float& outDeltaY) override { outDeltaX = 0.0f; outDeltaY = 0.0f; }
};

class StubTextInput final : public ava::platform::ui::ITextInput {
public:
    std::string ConsumeCommittedText() override { return {}; }
};

class StubTouch final : public ava::platform::ui::ITouch {
public:
    std::vector<ava::platform::ui::NativeTouchPoint> ActivePoints() const override { return {}; }
};

class StubIme final : public ava::platform::ui::IIme {
public:
    ava::platform::ui::ImeComposition CurrentComposition() const override { return {}; }
};

class StubPlatformServices final : public ava::platform::ui::IPlatformServices {
public:
    bool OpenFileDialog(std::string&) override { return false; }
    bool SaveFileDialog(std::string&) override { return false; }
    void ShowNotification(const std::string&, const std::string&) override {}
    void OpenUri(const std::string&) override {}
};

}
}
}
}