#pragma once

namespace avalang {
namespace ui {
namespace platform {

class AppSurface {
public:
    virtual ~AppSurface() = default;

    virtual void Create(int width, int height, const char* title) = 0;
    virtual void Resize(int width, int height) = 0;
    virtual void Show() = 0;
    virtual void Hide() = 0;
    virtual void* NativeHandle() const = 0;
    virtual bool ProcessEvents() = 0;
    virtual bool IsClosed() const = 0;
};

}
}
}