#include "renderer/IRenderer.h"
#include "renderer/NullRenderer.h"
#include "renderer/HTMLRenderer.h"

#include <cstring>
#include <unordered_map>

namespace avalang {
namespace ui {

namespace {

std::unordered_map<std::string, IRenderer::CreateFn>& Registry() {
    static std::unordered_map<std::string, IRenderer::CreateFn> registry;
    return registry;
}

}

void IRenderer::RegisterBackend(const std::string& name, CreateFn factory) {
    Registry()[name] = std::move(factory);
}

std::unique_ptr<IRenderer> IRenderer::Create(const char* backend, int width, int height) {
    if (!backend) backend = "html";

    if (std::strcmp(backend, "html") == 0) {
        return std::make_unique<HTMLRenderer>(width, height);
    }

    if (std::strcmp(backend, "null") == 0) {
        return std::make_unique<NullRenderer>(width, height);
    }

    auto it = Registry().find(backend);
    if (it != Registry().end()) {
        return it->second(width, height);
    }

    return std::make_unique<HTMLRenderer>(width, height);
}

}
}
