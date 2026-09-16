#include "WinAppSurface.h"

namespace avalang {
namespace ui {
namespace platform {
namespace windows {

WinAppSurface::WinAppSurface() = default;
WinAppSurface::~WinAppSurface() = default;

void WinAppSurface::Create(int width, int height, const char* title) {
    window_.Create(width, height, title);
}

void WinAppSurface::Resize(int width, int height) {
    window_.Resize(width, height);
}

void WinAppSurface::Show() {
    window_.Show();
}

void WinAppSurface::Hide() {
    window_.Hide();
}

void* WinAppSurface::NativeHandle() const {
    return window_.NativeHandle();
}

bool WinAppSurface::ProcessEvents() {
    return window_.PumpMessages();
}

bool WinAppSurface::IsClosed() const {
    return window_.IsClosed();
}

}
}
}
}