#include "controls/Dialog.h"
#include "components/ComponentTree.h"
#include "components/PropertyValue.h"

namespace avalang::ui::controls {

IComponent* CreateDialog(ComponentTree* tree, const std::string& title) {
    if (!tree) {
        return nullptr;
    }

    IComponent* dialog = tree->CreateComponent("Dialog");
    if (!dialog) {
        return nullptr;
    }

    dialog->SetProperty("title", PropertyValue(title));
    dialog->SetProperty("isOpen", PropertyValue(false));
    dialog->SetProperty("dismissible", PropertyValue(true));

    // overlay/backdrop and surface/border colors are filled by
    // RenderTheme::Apply() for type "dialog" -- not set here, same
    // division of responsibility as Button's background/font.

    return dialog;
}

void SetDialogOpen(IComponent* dialog, bool isOpen) {
    if (!dialog) {
        return;
    }
    dialog->SetProperty("isOpen", PropertyValue(isOpen));
}

} // namespace avalang::ui::controls
