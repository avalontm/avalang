#include "registry/ComponentTypeRegistry.h"

#include <cassert>
#include <cstdio>
#include <string>

using avalang::ui::PropertyType;
using avalang::ui::registry::ComponentTypeDescriptor;
using avalang::ui::registry::FindComponentType;
using avalang::ui::registry::PropertyDefault;

namespace {

const PropertyDefault* FindProp(const ComponentTypeDescriptor& desc, const std::string& name) {
    for (const auto& p : desc.default_properties) {
        if (p.name == name) return &p;
    }
    return nullptr;
}

void ExpectRegistered(const std::string& type, bool expectedIsContainer) {
    const ComponentTypeDescriptor* desc = FindComponentType(type);
    assert(desc != nullptr);
    assert(desc->is_container == expectedIsContainer);
}

} // namespace

int main() {
    ExpectRegistered("Button", false);
    ExpectRegistered("CheckBox", false);
    ExpectRegistered("RadioButton", false);
    ExpectRegistered("TextBox", false);
    ExpectRegistered("ComboBox", false);
    ExpectRegistered("Dialog", true);
    ExpectRegistered("Image", false);
    ExpectRegistered("Text", false);
    ExpectRegistered("Column", true);
    ExpectRegistered("Row", true);
    ExpectRegistered("Stack", true);
    ExpectRegistered("Page", true);
    ExpectRegistered("ScrollView", true);

    const ComponentTypeDescriptor* button = FindComponentType("Button");
    const PropertyDefault* buttonEnabled = FindProp(*button, "isEnabled");
    assert(buttonEnabled != nullptr);
    assert(buttonEnabled->value.Type() == PropertyType::Bool);
    assert(buttonEnabled->value.AsBool() == true);
    assert(FindProp(*button, "enabled") == nullptr);

    const ComponentTypeDescriptor* checkbox = FindComponentType("CheckBox");
    const PropertyDefault* checkboxEnabled = FindProp(*checkbox, "isEnabled");
    assert(checkboxEnabled != nullptr);
    assert(checkboxEnabled->value.AsBool() == true);

    const ComponentTypeDescriptor* radio = FindComponentType("RadioButton");
    const PropertyDefault* radioEnabled = FindProp(*radio, "isEnabled");
    assert(radioEnabled != nullptr);
    assert(radioEnabled->value.AsBool() == true);

    const ComponentTypeDescriptor* textbox = FindComponentType("TextBox");
    const PropertyDefault* textboxEnabled = FindProp(*textbox, "isEnabled");
    assert(textboxEnabled != nullptr);
    assert(textboxEnabled->value.AsBool() == true);

    const ComponentTypeDescriptor* combobox = FindComponentType("ComboBox");
    const PropertyDefault* comboboxEnabled = FindProp(*combobox, "isEnabled");
    assert(comboboxEnabled != nullptr);
    assert(comboboxEnabled->value.AsBool() == true);

    std::printf("ComponentTypeRegistryControlsTest: OK\n");
    return 0;
}
