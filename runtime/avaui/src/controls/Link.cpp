#include "controls/Link.h"
#include "components/ComponentTree.h"
#include "components/PropertyValue.h"
#include "registry/ComponentTypeRegistry.h"

namespace avalang::ui::controls {

IComponent* CreateLink(ComponentTree* tree, const std::string& text, const std::string& href) {
    if (!tree) {
        return nullptr;
    }

    IComponent* comp = tree->CreateComponent("Link");
    if (!comp) {
        return nullptr;
    }

    comp->SetProperty("text", PropertyValue(text));
    comp->SetProperty("href", PropertyValue(href));
    return comp;
}

void SetLinkText(IComponent* linkComponent, const std::string& text) {
    if (!linkComponent) {
        return;
    }
    linkComponent->SetProperty("text", PropertyValue(text));
}

void SetLinkHref(IComponent* linkComponent, const std::string& href) {
    if (!linkComponent) {
        return;
    }
    linkComponent->SetProperty("href", PropertyValue(href));
}

namespace {
struct LinkTypeRegistration {
    LinkTypeRegistration() {
        using namespace avalang::ui::registry;
        RegisterComponentType({
            "Link", "Link", /*is_container=*/false,
            {
                {"text", PropertyValue("Link")},
                {"href", PropertyValue("")},
            },
        });
    }
};
static LinkTypeRegistration _link_type_registration;
} // namespace

} // namespace avalang::ui::controls
