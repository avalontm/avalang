#include "designer/component_registry.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <unordered_map>

#include "design/component_catalog.h"
#include "designer/property_editor.h"
#include "registry/ComponentTypeRegistry.h"

namespace studio::designer {

namespace {

std::string ToLower(const std::string& value) {
    std::string result = value;
    std::transform(result.begin(), result.end(), result.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

PropertyEditorKind EditorKindFor(const std::string& name, PropertyType type) {
    if (type == PropertyType::Bool) {
        return PropertyEditorKind::Boolean;
    }
    if (type == PropertyType::Number) {
        return PropertyEditorKind::Number;
    }
    std::string lowered = ToLower(name);
    if (lowered.find("color") != std::string::npos || lowered.find("background") != std::string::npos) {
        return PropertyEditorKind::Color;
    }
    if (lowered == "source" || lowered == "href" || lowered == "icon") {
        return PropertyEditorKind::Resource;
    }
    return PropertyEditorKind::String;
}

PropertyCategory CategoryFor(const std::string& name) {
    static const std::vector<std::pair<std::string, PropertyCategory>> patterns = {
        {"text", PropertyCategory::Typography},
        {"label", PropertyCategory::Typography},
        {"title", PropertyCategory::Typography},
        {"placeholder", PropertyCategory::Typography},
        {"width", PropertyCategory::Layout},
        {"height", PropertyCategory::Layout},
        {"gap", PropertyCategory::Layout},
        {"padding", PropertyCategory::Layout},
        {"margin", PropertyCategory::Layout},
        {"align", PropertyCategory::Layout},
        {"columns", PropertyCategory::Layout},
        {"rows", PropertyCategory::Layout},
        {"style", PropertyCategory::Appearance},
        {"color", PropertyCategory::Appearance},
        {"background", PropertyCategory::Appearance},
        {"source", PropertyCategory::Appearance},
        {"href", PropertyCategory::Behavior},
        {"isenabled", PropertyCategory::Behavior},
        {"ischecked", PropertyCategory::Behavior},
        {"isselected", PropertyCategory::Behavior},
        {"isopen", PropertyCategory::Behavior},
        {"isfocused", PropertyCategory::Behavior},
        {"dismissible", PropertyCategory::Behavior},
        {"group", PropertyCategory::Behavior},
        {"selectedvalue", PropertyCategory::Behavior},
        {"alt", PropertyCategory::Accessibility},
    };
    std::string lowered = ToLower(name);
    for (const auto& pattern : patterns) {
        if (lowered.find(pattern.first) != std::string::npos) {
            return pattern.second;
        }
    }
    return PropertyCategory::Advanced;
}

const std::vector<EventMetadata>& EventsFor(const std::string& type) {
    static const std::unordered_map<std::string, std::vector<EventMetadata>> table = {
        {"button", {{"click", "Se dispara cuando el usuario presiona el botón"}}},
        {"link", {{"click", "Se dispara cuando el usuario activa el enlace"}}},
        {"checkbox", {{"change", "Se dispara cuando cambia el estado marcado/desmarcado"}}},
        {"radiobutton", {{"change", "Se dispara cuando cambia la selección del grupo"}}},
        {"combobox", {{"change", "Se dispara cuando cambia el valor seleccionado"}}},
        {"textbox", {{"change", "Se dispara cuando cambia el texto"}}},
        {"dialog",
         {{"open", "Se dispara cuando el diálogo se abre"}, {"close", "Se dispara cuando el diálogo se cierra"}}},
    };
    static const std::vector<EventMetadata> empty;
    auto it = table.find(type);
    return it != table.end() ? it->second : empty;
}

ComponentMetadata BuildMetadata(const avalang::ui::registry::ComponentTypeDescriptor& descriptor) {
    ComponentMetadata metadata;
    metadata.type = ToLower(descriptor.type);
    metadata.displayName = descriptor.display_name;

    for (const auto& prop : descriptor.default_properties) {
        PropertyMetadata property;
        property.name = prop.name;
        property.type = PropertyTypeName(prop.value.Type());
        property.defaultValue = FormatPropertyValue(prop.value);
        property.category = CategoryFor(prop.name);
        property.designerEditor = EditorKindFor(prop.name, prop.value.Type());
        metadata.defaultProperties.push_back(property);
        metadata.supportedProperties.push_back(property);
    }

    metadata.supportedEvents = EventsFor(metadata.type);

    if (const auto* legacy = studio::design::FindComponentType(metadata.type)) {
        metadata.category = legacy->category;
        metadata.icon = legacy->icon;
        metadata.order = legacy->order;
    } else {
        metadata.category = descriptor.is_container ? "Layout" : "Controles";
        metadata.order = std::numeric_limits<int>::max();
    }

    metadata.designerCapabilities.isContainer = descriptor.is_container;
    metadata.designerCapabilities.canReorderChildren = descriptor.is_container;
    metadata.designerCapabilities.canResize = metadata.type != "page";

    return metadata;
}

}

ComponentRegistry::ComponentRegistry() {
    for (const auto& descriptor : avalang::ui::registry::GetComponentTypeRegistry()) {
        entries_.push_back(BuildMetadata(descriptor));
    }
    std::stable_sort(entries_.begin(), entries_.end(),
                      [](const ComponentMetadata& a, const ComponentMetadata& b) { return a.order < b.order; });
}

const ComponentRegistry& ComponentRegistry::Instance() {
    static const ComponentRegistry instance;
    return instance;
}

const std::vector<ComponentMetadata>& ComponentRegistry::All() const {
    return entries_;
}

const ComponentMetadata* ComponentRegistry::Find(const ComponentTypeId& type) const {
    std::string lowered = ToLower(type);
    for (const auto& entry : entries_) {
        if (entry.type == lowered) {
            return &entry;
        }
    }
    return nullptr;
}

std::vector<const ComponentMetadata*> ComponentRegistry::ByCategory(const std::string& category) const {
    std::vector<const ComponentMetadata*> result;
    for (const auto& entry : entries_) {
        if (entry.category == category) {
            result.push_back(&entry);
        }
    }
    return result;
}

std::vector<std::string> ComponentRegistry::Categories() const {
    std::vector<std::string> categories;
    for (const auto& entry : entries_) {
        if (std::find(categories.begin(), categories.end(), entry.category) == categories.end()) {
            categories.push_back(entry.category);
        }
    }
    return categories;
}

}
