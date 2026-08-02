#include "MacUIPlatformFactory.h"

namespace avalang {
namespace ui {
namespace platform {

ava::platform::ui::IWindow* MacUIPlatformFactory::CreateWindow() {
    return new stub::StubWindow();
}

ava::platform::ui::IRenderSurface* MacUIPlatformFactory::CreateRenderSurface(ava::platform::ui::IWindow*) {
    return new stub::StubRenderSurface();
}

IUIPlatformFactory& GetUIPlatformFactory() {
    static MacUIPlatformFactory instance;
    return instance;
}

} // namespace platform
} // namespace ui
} // namespace avalang
