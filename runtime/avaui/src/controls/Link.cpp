#include "controls/Link.h"
#include "components/ComponentTree.h"
#include "components/PropertyValue.h"
#include "registry/ComponentTypeRegistry.h"

namespace avalang {
namespace ui {
namespace controls {

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
            "Link", "Link", false,
            {
                PropertyDescriptor{
                    .name = "text",
                    .defaultValue = PropertyValue("Link"),
                    .kind = PropertyKind::Text,
                    .group = PropertyGroup::Typography,
                    .description = "Texto visible del enlace",
                },
                PropertyDescriptor{
                    .name = "href",
                    .defaultValue = PropertyValue(""),
                    .kind = PropertyKind::Text,
                    .group = PropertyGroup::Behavior,
                    .description = "URL o ruta de destino que se abre al activar el enlace",
                },
            },
        });
    }
};
static LinkTypeRegistration _link_type_registration;
}

}
}
}