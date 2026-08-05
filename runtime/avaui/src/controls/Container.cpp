#include "controls/Container.h"
#include "components/ComponentTree.h"
#include "components/PropertyValue.h"
#include "registry/ComponentTypeRegistry.h"

namespace avalang::ui::controls {

namespace {

IComponent* CreateContainer(ComponentTree* tree, const std::string& typeName) {
    if (!tree) {
        return nullptr;
    }
    return tree->CreateComponent(typeName);
}

} // namespace

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
        RegisterComponentType({"Column", "Column", /*is_container=*/true, {}});
        RegisterComponentType({"Row", "Row", /*is_container=*/true, {}});
        RegisterComponentType({"Stack", "Stack", /*is_container=*/true, {}});
        RegisterComponentType({"Page", "Page", /*is_container=*/true, {}});
        RegisterComponentType({"ScrollView", "Scroll View", /*is_container=*/true, {}});
        RegisterComponentType({
            "Grid", "Grid", /*is_container=*/true,
            {
                {"columns", PropertyValue(2.0)},
                {"rows", PropertyValue(2.0)},
            },
        });
        RegisterComponentType({"Flex", "Flex", /*is_container=*/true, {}});
    }
};
static ContainerTypeRegistrations _container_type_registrations;
} // namespace

} // namespace avalang::ui::controls
