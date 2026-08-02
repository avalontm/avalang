#include "controls/Container.h"
#include "components/ComponentTree.h"
#include "components/PropertyValue.h"

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

} // namespace avalang::ui::controls
