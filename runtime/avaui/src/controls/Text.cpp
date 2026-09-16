#include "controls/Text.h"
#include "components/ComponentTree.h"
#include "components/PropertyValue.h"
#include "registry/ComponentTypeRegistry.h"

namespace avalang {
namespace ui {
namespace controls {

IComponent* CreateText(ComponentTree* tree, const std::string& text) {
    if (!tree) {
        return nullptr;
    }

    IComponent* comp = tree->CreateComponent("Text");
    if (!comp) {
        return nullptr;
    }

    comp->SetProperty("text", PropertyValue(text));
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
            "Text", "Text", false,
            {
                {"text", PropertyValue("Text")},
            },
        });
    }
};
static TextTypeRegistration _text_type_registration;
}

}
}
}