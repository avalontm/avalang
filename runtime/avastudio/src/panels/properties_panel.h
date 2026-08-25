#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace studio {

struct PropertyRow {
    std::string key;
    std::string value;
};

struct PropertiesState {
    std::string selected_component_type;
    std::string selected_component_id;
    std::vector<PropertyRow> properties;

    std::vector<PropertyRow> events;

    bool editable = false;

    int source_tab_id = -1;
    std::string selected_node_id;
};

enum class PropertyEditKind {
    kValue,
    kId,
    kType,
    kAddProperty,
    kRemoveProperty,
    kEvent,
    kRemoveEvent,
};

struct PropertyEdit {
    int tab_id = -1;
    std::string node_id;
    PropertyEditKind kind = PropertyEditKind::kValue;
    std::string key;
    std::string new_value;
};

std::optional<PropertyEdit> DrawPropertiesPanel(PropertiesState& state, bool* p_open = nullptr);

}
