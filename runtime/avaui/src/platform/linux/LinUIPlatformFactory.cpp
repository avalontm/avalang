#include "LinUIPlatformFactory.h"

namespace avalang {
namespace ui {
namespace platform {

ava::platform::ui::IWindow* LinUIPlatformFactory::CreateWindow() {
    return new stub::StubWindow();
}

ava::platform::ui::IRenderSurface* LinUIPlatformFactory::CreateRenderSurface(ava::platform::ui::IWindow*) {
    return new stub::StubRenderSurface();
}

IUIPlatformFactory& GetUIPlatformFactory() {
    static LinUIPlatformFactory instance;
    return instance;
}

} // namespace platform
} // namespace ui
} // namespace avalang
