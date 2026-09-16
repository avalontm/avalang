#include "renderer/IRenderer.h"
#include "AndroidCanvasRenderer.h"

namespace avalang {
namespace ui {
namespace platform {
namespace android {

namespace {

std::unique_ptr<avalang::ui::IRenderer> CreateAndroidCanvasRenderer(int width, int height) {
    return std::make_unique<avalang::ui::AndroidCanvasRenderer>(width, height);
}

struct RendererRegistrar {
    RendererRegistrar() {
        avalang::ui::IRenderer::RegisterBackend("native", CreateAndroidCanvasRenderer);
        avalang::ui::IRenderer::RegisterBackend("canvas", CreateAndroidCanvasRenderer);
    }
};

const RendererRegistrar registrar;

}

}
}
}
}
