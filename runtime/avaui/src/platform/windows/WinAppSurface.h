#pragma once

#include "../contract/AppSurface.h"
#include "WinWindow.h"

namespace avalang {
namespace ui {
namespace platform {
namespace windows {

class WinAppSurface final : public AppSurface {
public:
    WinAppSurface();
    ~WinAppSurface() override;

    void Create(int width, int height, const char* title) override;
    void Resize(int width, int height) override;
    void Show() override;
    void Hide() override;
    void* NativeHandle() const override;
    bool ProcessEvents() override;
    bool IsClosed() const override;

private:
    WinWindow window_;
};

}
}
}
}