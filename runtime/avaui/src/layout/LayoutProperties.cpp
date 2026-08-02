#include "layout/LayoutProperties.h"

namespace avalang {
namespace ui {
namespace layout {

bool TryReadNumber(const IComponent* component, const std::string& name, double* out) {
    if (!component) {
        return false;
    }
    const PropertyValue* value = component->GetProperty(name);
    if (!value || value->Type() != PropertyType::Number) {
        return false;
    }
    *out = value->AsNumber();
    return true;
}

double ReadNumber(const IComponent* component, const std::string& name, double defaultValue) {
    double value = defaultValue;
    TryReadNumber(component, name, &value);
    return value;
}

EdgeInsets ReadEdgeInsets(const IComponent* component, const std::string& baseName) {
    double uniform = ReadNumber(component, baseName, 0.0);
    EdgeInsets insets;
    insets.left = uniform;
    insets.top = uniform;
    insets.right = uniform;
    insets.bottom = uniform;

    insets.left = ReadNumber(component, baseName + "-left", insets.left);
    insets.top = ReadNumber(component, baseName + "-top", insets.top);
    insets.right = ReadNumber(component, baseName + "-right", insets.right);
    insets.bottom = ReadNumber(component, baseName + "-bottom", insets.bottom);
    return insets;
}

LayoutAlignment ReadAlignment(const IComponent* component, const std::string& name) {
    if (!component) {
        return LayoutAlignment::Stretch;
    }
    const PropertyValue* value = component->GetProperty(name);
    if (!value || value->Type() != PropertyType::String) {
        return LayoutAlignment::Stretch;
    }
    const std::string& text = value->AsString();
    if (text == "start") {
        return LayoutAlignment::Start;
    }
    if (text == "center") {
        return LayoutAlignment::Center;
    }
    if (text == "end") {
        return LayoutAlignment::End;
    }
    return LayoutAlignment::Stretch;
}

} // namespace layout
} // namespace ui
} // namespace avalang
