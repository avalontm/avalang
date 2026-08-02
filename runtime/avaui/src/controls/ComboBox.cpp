#include "controls/ComboBox.h"
#include "components/ComponentTree.h"
#include "components/PropertyValue.h"
#include <unordered_map>
#include <mutex>

namespace avalang::ui::controls {

namespace {

std::unordered_map<ComponentId, ComboBoxChangeCallback> g_comboBoxCallbacks;
std::mutex g_callbackMutex;

}  // anonymous namespace

IComponent* CreateComboBox(ComponentTree* tree) {
    if (!tree) {
        return nullptr;
    }

    IComponent* comp = tree->CreateComponent("ComboBox");
    if (!comp) {
        return nullptr;
    }

    comp->SetProperty("selectedValue", PropertyValue(std::string()));
    comp->SetProperty("isEnabled", PropertyValue(true));

    return comp;
}

IComponent* AddOption(ComponentTree* tree, IComponent* comboBoxComponent,
                       const std::string& value, const std::string& label) {
    if (!tree || !comboBoxComponent) {
        return nullptr;
    }

    IComponent* option = tree->CreateComponent("Option");
    if (!option) {
        return nullptr;
    }

    option->SetProperty("value", PropertyValue(value));
    option->SetProperty("label", PropertyValue(label));
    comboBoxComponent->AddChild(option, "default");
    return option;
}

void SetSelectedValue(IComponent* comboBoxComponent, const std::string& value) {
    if (!comboBoxComponent) {
        return;
    }
    comboBoxComponent->SetProperty("selectedValue", PropertyValue(value));

    ComboBoxChangeCallback callback;
    {
        std::lock_guard<std::mutex> lock(g_callbackMutex);
        auto it = g_comboBoxCallbacks.find(comboBoxComponent->Id());
        if (it != g_comboBoxCallbacks.end()) {
            callback = it->second;
        }
    }
    if (callback) {
        callback(comboBoxComponent->Id(), value);
    }
}

std::string GetSelectedValue(IComponent* comboBoxComponent) {
    if (!comboBoxComponent) {
        return std::string();
    }
    const auto* prop = comboBoxComponent->GetProperty("selectedValue");
    if (!prop || prop->Type() != PropertyType::String) {
        return std::string();
    }
    return prop->AsString();
}

std::string GetSelectedLabel(IComponent* comboBoxComponent) {
    if (!comboBoxComponent) {
        return std::string();
    }
    const std::string selected = GetSelectedValue(comboBoxComponent);
    for (IComponent* child : comboBoxComponent->Children()) {
        const auto* valueProp = child->GetProperty("value");
        if (valueProp && valueProp->Type() == PropertyType::String &&
            valueProp->AsString() == selected) {
            const auto* labelProp = child->GetProperty("label");
            if (labelProp && labelProp->Type() == PropertyType::String) {
                return labelProp->AsString();
            }
        }
    }
    return std::string();
}

void BindComboBoxChange(ComponentId comboBoxId, ComboBoxChangeCallback callback) {
    std::lock_guard<std::mutex> lock(g_callbackMutex);
    g_comboBoxCallbacks[comboBoxId] = std::move(callback);
}

void UnbindComboBoxChange(ComponentId comboBoxId) {
    std::lock_guard<std::mutex> lock(g_callbackMutex);
    g_comboBoxCallbacks.erase(comboBoxId);
}

} // namespace avalang::ui::controls
