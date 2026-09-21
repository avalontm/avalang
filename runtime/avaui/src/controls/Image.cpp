#include "controls/Image.h"
#include "components/ComponentTree.h"
#include "components/PropertyValue.h"
#include "registry/ComponentTypeRegistry.h"

namespace avalang {
namespace ui {
namespace controls {

IComponent* CreateImage(ComponentTree* tree, const std::string& src) {
    if (!tree) {
        return nullptr;
    }

    IComponent* comp = tree->CreateComponent("Image");
    if (!comp) {
        return nullptr;
    }

    comp->SetProperty("source", PropertyValue(src));
    comp->SetProperty("alt", PropertyValue(std::string()));
    return comp;
}

void SetImageSource(IComponent* imageComponent, const std::string& src) {
    if (!imageComponent) {
        return;
    }
    imageComponent->SetProperty("source", PropertyValue(src));
}

namespace {
struct ImageTypeRegistration {
    ImageTypeRegistration() {
        using namespace avalang::ui::registry;
        RegisterComponentType({
            "Image", "Image", false,
            {
                PropertyDescriptor{
                    .name = "source",
                    .defaultValue = PropertyValue(""),
                    .kind = PropertyKind::Resource,
                    .group = PropertyGroup::Appearance,
                    .description = "Ruta o URL del archivo de imagen a mostrar",
                },
                PropertyDescriptor{
                    .name = "alt",
                    .defaultValue = PropertyValue(""),
                    .kind = PropertyKind::Text,
                    .group = PropertyGroup::Accessibility,
                    .description = "Texto alternativo para lectores de pantalla",
                },
            },
        });
    }
};
static ImageTypeRegistration _image_type_registration;
}

}
}
}