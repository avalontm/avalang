#pragma once

#include <string>
#include <utility>
#include <vector>

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

    // Fase 1 del plan de metadatos dinámicos (ver PROPERTY_METADATA_PLAN.md):
    // opciones declaradas explícitamente por el control para
    // `designerEditor == Enum` (value, label). Reemplaza el hack previo de
    // extraer alternativas desde un regex en `validation`. Si un control aún
    // no declara `options` pero sí un `validation` con alternancia regex
    // `(a|b|c)`, el panel sigue soportando ese camino como fallback.
    std::vector<std::pair<std::string, std::string>> enumOptions;

    // Rango/unidad opcional para editores numéricos (`Number`/`Dimension`).
    bool hasRange = false;
    double minValue = 0.0;
    double maxValue = 0.0;
    std::string unit;
};

}

