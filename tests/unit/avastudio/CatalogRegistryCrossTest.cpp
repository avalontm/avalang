#include "design/component_catalog.h"
#include "registry/ComponentTypeRegistry.h"

#include <cassert>
#include <cctype>
#include <cstdio>
#include <string>

using avalang::ui::PropertyValue;
using avalang::ui::registry::ComponentTypeDescriptor;
using avalang::ui::registry::GetComponentTypeRegistry;
using avalang::ui::registry::PropertyDefault;
using avalang::ui::registry::RegisterComponentType;
using studio::design::ComponentTypeInfo;
using studio::design::GetComponentCatalog;

namespace {

std::string ToLowerLocal(const std::string& value) {
    std::string result = value;
    for (char& c : result) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return result;
}

const ComponentTypeDescriptor* FindDescriptorByLoweredType(const std::string& lowered_type) {
    for (const auto& descriptor : GetComponentTypeRegistry()) {
        if (ToLowerLocal(descriptor.type) == lowered_type) return &descriptor;
    }
    return nullptr;
}

bool HasPropertyNamed(const ComponentTypeInfo& info, const std::string& name) {
    for (const auto& prop : info.default_properties) {
        if (prop.key == name) return true;
    }
    return false;
}

} // namespace

int main() {
    RegisterComponentType(ComponentTypeDescriptor{
        "TempWidgetPhaseB6", "Temp Widget B6", /*is_container=*/false,
        {PropertyDefault{"note", PropertyValue(std::string("temporal"))}},
    });

    const std::vector<ComponentTypeInfo>& catalog = GetComponentCatalog();
    assert(!catalog.empty());

    for (const ComponentTypeInfo& info : catalog) {
        const ComponentTypeDescriptor* descriptor = FindDescriptorByLoweredType(info.type);
        assert(descriptor != nullptr);
        assert(descriptor->is_container == info.is_container);
        assert(descriptor->default_properties.size() == info.default_properties.size());
        for (const auto& prop : descriptor->default_properties) {
            assert(HasPropertyNamed(info, prop.name));
        }
        assert(!info.category.empty());
    }

    for (size_t i = 1; i < catalog.size(); ++i) {
        assert(catalog[i].order >= catalog[i - 1].order);
    }

    const ComponentTypeInfo* button = nullptr;
    const ComponentTypeInfo* grid = nullptr;
    for (const auto& info : catalog) {
        if (info.type == "button") button = &info;
        if (info.type == "grid") grid = &info;
    }
    assert(button != nullptr);
    assert(grid != nullptr);
    assert(grid->order < button->order);
    assert(grid->category == "Layout");
    assert(button->category == "Interactive");

    bool found_temp = false;
    for (const auto& info : catalog) {
        if (info.type == "tempwidgetphaseb6") {
            found_temp = true;
            assert(!info.category.empty());
            assert(info.order >= 1000);
        }
    }
    assert(found_temp);

    std::printf("CatalogRegistryCrossTest: OK\n");
    return 0;
}
