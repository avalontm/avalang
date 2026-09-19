#include "controls/CheckBox.h"
#include "components/ComponentTree.h"
#include "components/PropertyValue.h"
#include "registry/ComponentTypeRegistry.h"
#include <unordered_map>
#include <mutex>

namespace avalang {
namespace ui {
namespace controls {

namespace {

std::unordered_map<ComponentId, CheckBoxChangeCallback> g_checkBoxCallbacks;
std::mutex g_callbackMutex;

void NotifyChange(IComponent* checkBoxComponent, bool isChecked) {
    CheckBoxChangeCallback callback;
    {
        std::lock_guard<std::mutex> lock(g_callbackMutex);
        auto it = g_checkBoxCallbacks.find(checkBoxComponent->Id());
        if (it != g_checkBoxCallbacks.end()) {
            callback = it->second;
        }
    }
    if (callback) {
        callback(checkBoxComponent->Id(), isChecked);
    }
}

}

IComponent* CreateCheckBox(ComponentTree* tree, const std::string& label, bool isChecked) {
    if (!tree) {
        return nullptr;
    }

    IComponent* comp = tree->CreateComponent("CheckBox");
    if (!comp) {
        return nullptr;
    }

    comp->SetProperty("label", PropertyValue(label));
    comp->SetProperty("isChecked", PropertyValue(isChecked));
    comp->SetProperty("isEnabled", PropertyValue(true));

    return comp;
}

void SetCheckBoxChecked(IComponent* checkBoxComponent, bool isChecked) {
    if (!checkBoxComponent) {
        return;
    }
    checkBoxComponent->SetProperty("isChecked", PropertyValue(isChecked));
    NotifyChange(checkBoxComponent, isChecked);
}

bool ToggleCheckBox(IComponent* checkBoxComponent) {
    if (!checkBoxComponent) {
        return false;
    }
    bool newState = !GetCheckBoxChecked(checkBoxComponent);
    SetCheckBoxChecked(checkBoxComponent, newState);
    return newState;
}

bool GetCheckBoxChecked(IComponent* checkBoxComponent) {
    if (!checkBoxComponent) {
        return false;
    }
    const auto* prop = checkBoxComponent->GetProperty("isChecked");
    if (!prop) {
        return false;
    }
    if (prop->Type() == PropertyType::Bool) {
        return prop->AsBool();
    }
    // A literal written as text (`isChecked = "true"`) is readable here. A
    // state binding (`isChecked = agreed`) is NOT: resolving it needs the
    // host's expression evaluator, which this layer does not have. Hosts that
    // support bindings plug one in through CheckBoxController::SetBinding.
    if (prop->Type() == PropertyType::String) {
        return prop->AsString() == "true";
    }
    return false;
}

void BindCheckBoxChange(ComponentId checkBoxId, CheckBoxChangeCallback callback) {
    std::lock_guard<std::mutex> lock(g_callbackMutex);
    g_checkBoxCallbacks[checkBoxId] = std::move(callback);
}

void UnbindCheckBoxChange(ComponentId checkBoxId) {
    std::lock_guard<std::mutex> lock(g_callbackMutex);
    g_checkBoxCallbacks.erase(checkBoxId);
}

namespace {
struct CheckBoxTypeRegistration {
    CheckBoxTypeRegistration() {
        using namespace avalang::ui::registry;
        RegisterComponentType({
            "CheckBox", "CheckBox", false,
            {
                {"label", PropertyValue("CheckBox")},
                {"isChecked", PropertyValue(false)},
                {"isEnabled", PropertyValue(true)},
            },
        });
    }
};
static CheckBoxTypeRegistration _checkbox_type_registration;
}

}
}
}