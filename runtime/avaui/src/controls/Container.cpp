#include "controls/Container.h"
#include "components/ComponentTree.h"
#include "components/PropertyValue.h"
#include "registry/ComponentTypeRegistry.h"

namespace avalang {
namespace ui {
namespace controls {

namespace {

IComponent* CreateContainer(ComponentTree* tree, const std::string& typeName) {
    if (!tree) {
        return nullptr;
    }
    return tree->CreateComponent(typeName);
}

}

IComponent* CreateColumn(ComponentTree* tree) {
    return CreateContainer(tree, "Column");
}

IComponent* CreateRow(ComponentTree* tree) {
    return CreateContainer(tree, "Row");
}

IComponent* CreateStack(ComponentTree* tree) {
    return CreateContainer(tree, "Stack");
}

void SetContainerSpacing(IComponent* container, double spacingPx) {
    if (!container) {
        return;
    }
    container->SetProperty("spacing", PropertyValue(spacingPx));
}

namespace {
struct ContainerTypeRegistrations {
    ContainerTypeRegistrations() {
        using namespace avalang::ui::registry;
        RegisterComponentType({"Container", "Container", true, {}});
        RegisterComponentType({"Column", "Column", true, {}});
        RegisterComponentType({"Row", "Row", true, {}});
        RegisterComponentType({"Stack", "Stack", true, {}});
        RegisterComponentType({"Page", "Page", true, {}});
        RegisterComponentType({
            "Grid", "Grid", true,
            {
                PropertyDescriptor{
                    .name = "columns",
                    .defaultValue = PropertyValue(2.0),
                    .kind = PropertyKind::Number,
                    .group = PropertyGroup::Layout,
                    .description = "Cantidad de columnas de la grilla (mínimo 1)",
                    .hasRange = true,
                    .minValue = 1.0,
                    .maxValue = 24.0,
                },
                PropertyDescriptor{
                    .name = "rows",
                    .defaultValue = PropertyValue(2.0),
                    .kind = PropertyKind::Number,
                    .group = PropertyGroup::Layout,
                    .description = "Cantidad de filas explícitas. 0 = automático, calculado a "
                                   "partir de la cantidad de hijos y de columns",
                    .hasRange = true,
                    .minValue = 0.0,
                    .maxValue = 24.0,
                },
            },
        });
        RegisterComponentType({"Flex", "Flex", true, {}});
    }
};
static ContainerTypeRegistrations _container_type_registrations;
}

}
}
}