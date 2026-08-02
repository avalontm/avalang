#include "controls/TextBox.h"
#include "components/ComponentTree.h"
#include "components/PropertyValue.h"
#include <unordered_map>
#include <mutex>

namespace avalang::ui::controls {

namespace {

std::unordered_map<ComponentId, TextBoxChangeCallback> g_textBoxCallbacks;
std::mutex g_callbackMutex;

}  // anonymous namespace

IComponent* CreateTextBox(ComponentTree* tree, const std::string& placeholder) {
    if (!tree) {
        return nullptr;
    }

    IComponent* comp = tree->CreateComponent("TextBox");
    if (!comp) {
        return nullptr;
    }

    comp->SetProperty("text", PropertyValue(std::string()));
    comp->SetProperty("placeholder", PropertyValue(placeholder));
    comp->SetProperty("isFocused", PropertyValue(false));
    comp->SetProperty("isEnabled", PropertyValue(true));

    // backgroundColor / borderColor / borderWidth / fontSize filled by
    // RenderTheme::Apply() (type == "textbox" branch).
    return comp;
}

void SetTextBoxValue(IComponent* textBoxComponent, const std::string& value) {
    if (!textBoxComponent) {
        return;
    }
    textBoxComponent->SetProperty("text", PropertyValue(value));

    TextBoxChangeCallback callback;
    {
        std::lock_guard<std::mutex> lock(g_callbackMutex);
        auto it = g_textBoxCallbacks.find(textBoxComponent->Id());
        if (it != g_textBoxCallbacks.end()) {
            callback = it->second;
        }
    }
    if (callback) {
        callback(textBoxComponent->Id(), value);
    }
}

std::string GetTextBoxValue(IComponent* textBoxComponent) {
    if (!textBoxComponent) {
        return std::string();
    }
    const auto* prop = textBoxComponent->GetProperty("text");
    if (!prop || prop->Type() != PropertyType::String) {
        return std::string();
    }
    return prop->AsString();
}

void BindTextBoxChange(ComponentId textBoxId, TextBoxChangeCallback callback) {
    std::lock_guard<std::mutex> lock(g_callbackMutex);
    g_textBoxCallbacks[textBoxId] = std::move(callback);
}

void UnbindTextBoxChange(ComponentId textBoxId) {
    std::lock_guard<std::mutex> lock(g_callbackMutex);
    g_textBoxCallbacks.erase(textBoxId);
}

} // namespace avalang::ui::controls
