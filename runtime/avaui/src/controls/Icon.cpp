#include "controls/Icon.h"
#include "components/ComponentTree.h"
#include "components/PropertyValue.h"
#include "registry/ComponentTypeRegistry.h"

namespace avalang {
namespace ui {
namespace controls {

IComponent* CreateIcon(ComponentTree* tree, const std::string& src) {
    if (!tree) {
        return nullptr;
    }

    IComponent* comp = tree->CreateComponent("Icon");
    if (!comp) {
        return nullptr;
    }

    comp->SetProperty("source", PropertyValue(src));
    return comp;
}

void SetIconSource(IComponent* iconComponent, const std::string& src) {
    if (!iconComponent) {
        return;
    }
    iconComponent->SetProperty("source", PropertyValue(src));
}

namespace {
struct IconTypeRegistration {
    IconTypeRegistration() {
        using namespace avalang::ui::registry;
        RegisterComponentType({
            "Icon", "Icon", false,
            {
                {"source", PropertyValue("")},
            },
        });
    }
};
static IconTypeRegistration _icon_type_registration;
}

}
}
}
