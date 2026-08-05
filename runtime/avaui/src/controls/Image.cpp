#include "controls/Image.h"
#include "components/ComponentTree.h"
#include "components/PropertyValue.h"
#include "registry/ComponentTypeRegistry.h"

namespace avalang::ui::controls {

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
            "Image", "Image", /*is_container=*/false,
            {
                {"source", PropertyValue("")},
                {"alt", PropertyValue("")},
            },
        });
    }
};
static ImageTypeRegistration _image_type_registration;
} // namespace

} // namespace avalang::ui::controls
