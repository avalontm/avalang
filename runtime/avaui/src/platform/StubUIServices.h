#ifndef AVA_UI_PLATFORM_STUBUISERVICES_H
#define AVA_UI_PLATFORM_STUBUISERVICES_H

// Shared no-op implementations of the PAL UI services, used by the
// Linux and macOS backends only (see docs/AVALANG_UI_PROGRESS.md,
// "Alcance actual: solo Windows" -- those backends are paused
// STUB / en estudio and get no real work in Phase 11). Kept in one
// place instead of duplicated per-OS to avoid two copies of the same
// dead code. Windows has real implementations under platform/windows/.

#include "../../../avalang/platform/interfaces/services/ui/UIPlatformInterfaces.h"

namespace avalang {
namespace ui {
namespace platform {
namespace stub {

class StubWindow final : public ava::platform::ui::IWindow {
public:
    void Create(int, int, const char*) override {}
    void Destroy() override {}
    void Resize(int, int) override {}
    void Show() override {}
    void Hide() override {}
    ava::platform::ui::WindowState State() const override { return ava::platform::ui::WindowState::Normal; }
    void SetState(ava::platform::ui::WindowState) override {}
    void* NativeHandle() const override { return nullptr; }
};

class StubRenderSurface final : public ava::platform::ui::IRenderSurface {
public:
    void* NativeHandle() const override { return nullptr; }
    int Width() const override { return 0; }
    int Height() const override { return 0; }
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

} // namespace stub
} // namespace platform
} // namespace ui
} // namespace avalang

#endif // AVA_UI_PLATFORM_STUBUISERVICES_H
