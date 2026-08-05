#include "registry/ComponentTypeRegistry.h"

#include <algorithm>

namespace avalang {
namespace ui {
namespace registry {

namespace {

std::vector<ComponentTypeDescriptor>& MutableRegistry() {
    static std::vector<ComponentTypeDescriptor> registry;
    return registry;
}

} // namespace

void RegisterComponentType(ComponentTypeDescriptor desc) {
    std::vector<ComponentTypeDescriptor>& registry = MutableRegistry();
    auto it = std::find_if(registry.begin(), registry.end(),
                            [&desc](const ComponentTypeDescriptor& existing) {
                                return existing.type == desc.type;
                            });
    if (it != registry.end()) {
        *it = std::move(desc);
    } else {
        registry.push_back(std::move(desc));
    }
}

const std::vector<ComponentTypeDescriptor>& GetComponentTypeRegistry() {
    return MutableRegistry();
}

const ComponentTypeDescriptor* FindComponentType(const std::string& type) {
    const std::vector<ComponentTypeDescriptor>& registry = MutableRegistry();
    auto it = std::find_if(registry.begin(), registry.end(),
                            [&type](const ComponentTypeDescriptor& existing) {
                                return existing.type == type;
                            });
    return it != registry.end() ? &(*it) : nullptr;
}

} // namespace registry
} // namespace ui
} // namespace avalang
