#pragma once

#include <string>

namespace studio::designer {

enum class PropertyCategory {
    Identity,
    Layout,
    Appearance,
    Typography,
    Behavior,
    Accessibility,
    Events,
    Advanced,
};

enum class PropertyEditorKind {
    String,
    Number,
    Boolean,
    Enum,
    Color,
    Font,
    Dimension,
    Padding,
    Margin,
    Resource,
    Binding,
    Event,
    Style,
};

enum class PropertySource {
    Local,
    Style,
    Inherited,
    Theme,
    Default,
};

struct PropertyMetadata {
    std::string name;
    std::string type;
    std::string defaultValue;
    PropertyCategory category = PropertyCategory::Advanced;
    std::string description;
    bool readOnly = false;
    bool inherited = false;
    bool styleable = false;
    bool animatable = false;
    PropertyEditorKind designerEditor = PropertyEditorKind::String;
    std::string validation;
};

}
