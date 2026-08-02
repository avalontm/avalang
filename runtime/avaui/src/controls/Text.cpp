#include "controls/Text.h"
#include "components/ComponentTree.h"
#include "components/PropertyValue.h"

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

} // namespace avalang::ui::controls
