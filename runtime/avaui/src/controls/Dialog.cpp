#include "controls/Dialog.h"
#include "components/ComponentTree.h"
#include "components/PropertyValue.h"
#include "registry/ComponentTypeRegistry.h"

namespace avalang {
namespace ui {
namespace controls {

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

    return dialog;
}

void SetDialogOpen(IComponent* dialog, bool isOpen) {
    if (!dialog) {
        return;
    }
    dialog->SetProperty("isOpen", PropertyValue(isOpen));
}

namespace {
struct DialogTypeRegistration {
    DialogTypeRegistration() {
        using namespace avalang::ui::registry;
        RegisterComponentType({
            "Dialog", "Dialog", true,
            {
                {"title", PropertyValue("Dialog")},
                {"isOpen", PropertyValue(false)},
                {"dismissible", PropertyValue(true)},
            },
        });
    }
};
static DialogTypeRegistration _dialog_type_registration;
}

}
}
}