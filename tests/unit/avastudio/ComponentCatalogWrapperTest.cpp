#include "design/component_catalog.h"
#include "registry/ComponentTypeRegistry.h"

#include <cassert>
#include <cstdio>
#include <string>

using avalang::ui::PropertyValue;
using avalang::ui::registry::ComponentTypeDescriptor;
using avalang::ui::registry::PropertyDefault;
using avalang::ui::registry::RegisterComponentType;
using studio::design::ComponentTypeInfo;
using studio::design::FindComponentType;
using studio::design::GetComponentCatalog;

namespace {

const std::string* FindPropertyValue(const ComponentTypeInfo& info, const std::string& key) {
    for (const auto& prop : info.default_properties) {
        if (prop.key == key) return &prop.value;
    }
    return nullptr;
}

} // namespace

int main() {
    // Registrado antes de la primera lectura del catalogo: el wrapper
    // de B.4 construye su cache estatico en la primera llamada a
    // GetComponentCatalog()/FindComponentType, asi que este control
    // temporal debe existir en el registro de avaui *antes* de esa
    // primera llamada para probar el criterio de aceptacion "un
    // control agregado solo en avaui aparece en el catalogo de
    // avastudio sin tocar avastudio".
    RegisterComponentType(ComponentTypeDescriptor{
        "TempWidgetPhaseB4", "Temp Widget", /*is_container=*/false,
        {PropertyDefault{"note", PropertyValue(std::string("temporal"))}},
    });

    // Fase B.0: bug real -- el catalogo hardcodeado de avastudio ponia
    // "enabled" como default de Button; el control real usa
    // "isEnabled". El wrapper de B.4 lee directamente de avaui, asi que
    // ese bug no puede reaparecer sin que este test lo detecte.
    const ComponentTypeInfo* button = FindComponentType("button");
    assert(button != nullptr);
    assert(button->is_container == false);
    const std::string* isEnabled = FindPropertyValue(*button, "isEnabled");
    assert(isEnabled != nullptr);
    assert(*isEnabled == "true");
    assert(FindPropertyValue(*button, "enabled") == nullptr);

    const ComponentTypeInfo* grid = FindComponentType("grid");
    assert(grid != nullptr);
    assert(grid->is_container == true);
    const std::string* columns = FindPropertyValue(*grid, "columns");
    assert(columns != nullptr);
    assert(*columns == "2"); // no "2.000000" -- ToDisplayString redondea enteros
    const std::string* rows = FindPropertyValue(*grid, "rows");
    assert(rows != nullptr);
    assert(*rows == "2");

    const ComponentTypeInfo* row = FindComponentType("row");
    assert(row != nullptr);
    assert(row->is_container == true);

    const ComponentTypeInfo* link = FindComponentType("link");
    assert(link != nullptr);
    assert(link->is_container == false);
    assert(FindPropertyValue(*link, "href") != nullptr);

    // Ningun tipo retirado deliberadamente en B.3 (spacer/divider) debe
    // colarse de vuelta via el registro.
    assert(FindComponentType("spacer") == nullptr);
    assert(FindComponentType("divider") == nullptr);

    bool foundTemp = false;
    for (const auto& info : GetComponentCatalog()) {
        if (info.type == "tempwidgetphaseb4") {
            foundTemp = true;
            break;
        }
    }
    assert(foundTemp);

    std::printf("ComponentCatalogWrapperTest: OK\n");
    return 0;
}
