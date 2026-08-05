#include "controls/Text.h"
#include "components/ComponentTree.h"
#include "components/PropertyValue.h"
#include "registry/ComponentTypeRegistry.h"

namespace avalang::ui::controls {

IComponent* CreateText(ComponentTree* tree, const std::string& text) {
    if (!tree) {
        return nullptr;
    }

    IComponent* comp = tree->CreateComponent("Text");
    if (!comp) {
        return nullptr;
    }

    comp->SetProperty("text", PropertyValue(text));
    // fontName/fontSize/textColor filled by RenderTheme::Apply().
    return comp;
}

void SetTextValue(IComponent* textComponent, const std::string& text) {
    if (!textComponent) {
        return;
    }
    textComponent->SetProperty("text", PropertyValue(text));
}

namespace {
struct TextTypeRegistration {
    TextTypeRegistration() {
        using namespace avalang::ui::registry;
        RegisterComponentType({
            "Text", "Text", /*is_container=*/false,
            {
                {"text", PropertyValue("Text")},
            },
        });
    }
};
static TextTypeRegistration _text_type_registration;
} // namespace

} // namespace avalang::ui::controls
