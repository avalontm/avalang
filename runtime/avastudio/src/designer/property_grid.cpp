#include "designer/property_grid.h"

#include <array>

#include "designer/component_registry.h"
#include "designer/property_editor.h"

namespace studio::designer {

namespace {

constexpr std::array<PropertyCategory, 8> kCategoryOrder = {
    PropertyCategory::Identity,   PropertyCategory::Layout,        PropertyCategory::Appearance,
    PropertyCategory::Typography, PropertyCategory::Behavior,      PropertyCategory::Accessibility,
    PropertyCategory::Events,     PropertyCategory::Advanced,
};

PropertyGridRow BuildRow(UiNode* node, const PropertyMetadata& metadata) {
    PropertyGridRow row;
    row.metadata = metadata;

    const PropertyValue* current = node->GetProperty(metadata.name);
    if (current) {
        row.value = FormatPropertyValue(*current);
        row.source = PropertySource::Local;
    } else {
        row.value = metadata.defaultValue;
        row.source = PropertySource::Default;
    }

    return row;
}

PropertyMetadata IdentityMetadata() {
    PropertyMetadata metadata;
    metadata.name = "id";
    metadata.type = "string";
    metadata.category = PropertyCategory::Identity;
    metadata.designerEditor = PropertyEditorKind::String;
    metadata.description = "Identificador semántico del nodo, usado por Tree y code-behind";
    return metadata;
}

PropertyMetadata ToPropertyMetadata(const EventMetadata& event) {
    PropertyMetadata metadata;
    metadata.name = event.name;
    metadata.type = "string";
    metadata.category = PropertyCategory::Events;
    metadata.designerEditor = PropertyEditorKind::Event;
    metadata.description = event.description;
    return metadata;
}

}

std::vector<PropertyGridSection> BuildPropertyGrid(UiNode* node) {
    std::vector<PropertyGridSection> sections;
    if (!node) {
        return sections;
    }

    std::array<std::vector<PropertyGridRow>, kCategoryOrder.size()> rowsByCategory;
    rowsByCategory[static_cast<size_t>(PropertyCategory::Identity)].push_back(BuildRow(node, IdentityMetadata()));

    if (const ComponentMetadata* component = ComponentRegistry::Instance().Find(TypeOf(node))) {
        for (const PropertyMetadata& metadata : component->supportedProperties) {
            rowsByCategory[static_cast<size_t>(metadata.category)].push_back(BuildRow(node, metadata));
        }
        for (const EventMetadata& event : component->supportedEvents) {
            rowsByCategory[static_cast<size_t>(PropertyCategory::Events)].push_back(
                BuildRow(node, ToPropertyMetadata(event)));
        }
    }

    for (PropertyCategory category : kCategoryOrder) {
        std::vector<PropertyGridRow>& rows = rowsByCategory[static_cast<size_t>(category)];
        if (rows.empty()) {
            continue;
        }
        sections.push_back(PropertyGridSection{category, std::move(rows)});
    }

    return sections;
}

}
