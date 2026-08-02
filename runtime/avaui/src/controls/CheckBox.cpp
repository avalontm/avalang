#include "controls/CheckBox.h"
#include "components/ComponentTree.h"
#include "components/PropertyValue.h"
#include <unordered_map>
#include <mutex>

namespace avalang::ui::controls {

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

}  // anonymous namespace

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

    // borderColor / borderWidth filled by RenderTheme::Apply().
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
    if (!prop || prop->Type() != PropertyType::Bool) {
        return false;
    }
    return prop->AsBool();
}

void BindCheckBoxChange(ComponentId checkBoxId, CheckBoxChangeCallback callback) {
    std::lock_guard<std::mutex> lock(g_callbackMutex);
    g_checkBoxCallbacks[checkBoxId] = std::move(callback);
}

void UnbindCheckBoxChange(ComponentId checkBoxId) {
    std::lock_guard<std::mutex> lock(g_callbackMutex);
    g_checkBoxCallbacks.erase(checkBoxId);
}

} // namespace avalang::ui::controls
