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
        RegisterComponentType({"ScrollView", "Scroll View", true, {}});
        RegisterComponentType({"ListView", "List View", true, {}});
        RegisterComponentType({
            "Grid", "Grid", true,
            {
                {"columns", PropertyValue(2.0)},
                {"rows", PropertyValue(2.0)},
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