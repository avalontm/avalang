#include "renderer/IRenderer.h"
#include "platform/windows/GdiRenderer.h"

#include <windows.h>

namespace avalang {
namespace ui {
namespace platform {
namespace windows {

namespace {

std::unique_ptr<avalang::ui::IRenderer> CreateGdiRenderer(int width, int height) {
    return std::make_unique<GdiRenderer>(GetForegroundWindow(), width, height);
}

struct RendererRegistrar {
    RendererRegistrar() {
        avalang::ui::IRenderer::RegisterBackend("native", CreateGdiRenderer);
        avalang::ui::IRenderer::RegisterBackend("gdi", CreateGdiRenderer);
    }
};

const RendererRegistrar registrar;

}

}
}
}
}
