#include "designer/property_catalog.h"

#include <algorithm>
#include <unordered_set>
#include <utility>

#include "designer/component_registry.h"
#include "designer/property_editor.h"

namespace studio::designer {

namespace {

struct CatalogEntry {
    PropertyMetadata metadata;
    bool containerOnly = false;
};

PropertyMetadata MakeMetadata(std::string name, std::string type, std::string defaultValue,
                               PropertyCategory category, PropertyEditorKind editor) {
    PropertyMetadata metadata;
    metadata.name = std::move(name);
    metadata.type = std::move(type);
    metadata.defaultValue = std::move(defaultValue);
    metadata.category = category;
    metadata.designerEditor = editor;
    return metadata;
}

PropertyMetadata MakeEnum(std::string name, std::string defaultValue, PropertyCategory category,
                           const std::vector<std::string>& values) {
    PropertyMetadata metadata =
        MakeMetadata(std::move(name), "string", std::move(defaultValue), category, PropertyEditorKind::Enum);
    for (const std::string& value : values) {
        metadata.enumOptions.emplace_back(value, value);
    }
    return metadata;
}

const std::vector<CatalogEntry>& Catalog() {
    static const std::vector<CatalogEntry> catalog = [] {
        using Category = PropertyCategory;
        using Editor = PropertyEditorKind;

        std::vector<CatalogEntry> entries;
        entries.push_back({MakeMetadata("width", "number", "120", Category::Layout, Editor::Number), false});
        entries.push_back({MakeMetadata("height", "number", "40", Category::Layout, Editor::Number), false});
        entries.push_back({MakeMetadata("margin", "number", "0", Category::Layout, Editor::Number), false});
        entries.push_back({MakeMetadata("padding", "number", "0", Category::Layout, Editor::Number), true});
        entries.push_back({MakeMetadata("spacing", "number", "0", Category::Layout, Editor::Number), true});
        entries.push_back({MakeEnum("direction", "vertical", Category::Layout, {"vertical", "horizontal"}), true});
        entries.push_back({MakeMetadata("wrap", "bool", "false", Category::Layout, Editor::Boolean), true});
        entries.push_back({MakeMetadata("grow", "bool", "false", Category::Layout, Editor::Boolean), false});
        entries.push_back(
            {MakeEnum("align", "stretch", Category::Layout, {"start", "center", "end", "stretch"}), false});
        entries.push_back({MakeMetadata("zIndex", "number", "0", Category::Layout, Editor::Number), false});
        entries.push_back(
            {MakeMetadata("backgroundColor", "string", "#FFFFFF", Category::Appearance, Editor::Color), false});
        entries.push_back(
            {MakeMetadata("borderColor", "string", "#CCCCCC", Category::Appearance, Editor::Color), false});
        entries.push_back({MakeMetadata("borderWidth", "number", "1", Category::Appearance, Editor::Number), false});
        entries.push_back({MakeMetadata("borderRadius", "number", "4", Category::Appearance, Editor::Number), false});
        entries.push_back(
            {MakeMetadata("textColor", "string", "#000000", Category::Typography, Editor::Color), false});
        entries.push_back({MakeMetadata("fontSize", "number", "14", Category::Typography, Editor::Number), false});
        entries.push_back({MakeMetadata("fontName", "string", "", Category::Typography, Editor::Font), false});
        return entries;
    }();
    return catalog;
}

std::string EdgeInsetBaseName(const std::string& key) {
    static const char* const kSuffixes[] = {"-left", "-top", "-right", "-bottom"};
    for (const char* suffix : kSuffixes) {
        const std::string tail(suffix);
        if (key.size() > tail.size() && key.compare(key.size() - tail.size(), tail.size(), tail) == 0) {
            return key.substr(0, key.size() - tail.size());
        }
    }
    return key;
}

PropertyType DeclaredPropertyType(const UiNode* node, const std::string& key) {
    if (const ComponentMetadata* component = ComponentRegistry::Instance().Find(TypeOf(node))) {
        for (const PropertyMetadata& metadata : component->supportedProperties) {
            if (metadata.name == key) {
                return PropertyTypeFromName(metadata.type);
            }
        }
    }
    if (const PropertyMetadata* metadata = FindCatalogProperty(EdgeInsetBaseName(key))) {
        return PropertyTypeFromName(metadata->type);
    }
    if (const PropertyValue* existing = node->GetProperty(key)) {
        return existing->Type();
    }
    return PropertyType::String;
}

bool IsListable(const PropertyGridRow& row) {
    if (row.source == PropertySource::Local || row.metadata.readOnly) {
        return false;
    }
    if (row.metadata.category == PropertyCategory::Identity || row.metadata.category == PropertyCategory::Events) {
        return false;
    }
    return row.metadata.designerEditor != PropertyEditorKind::Event;
}

}

const PropertyMetadata* FindCatalogProperty(const std::string& name) {
    for (const CatalogEntry& entry : Catalog()) {
        if (entry.metadata.name == name) {
            return &entry.metadata;
        }
    }
    return nullptr;
}

PropertyValue ResolveTypedPropertyValue(const UiNode* node, const std::string& key, const std::string& text) {
    if (!node) {
        return PropertyValue(text);
    }
    const PropertyType type = DeclaredPropertyType(node, key);
    PropertyValue parsed;
    if ((type == PropertyType::Number || type == PropertyType::Bool) && TryParsePropertyValue(text, type, parsed)) {
        return parsed;
    }
    return PropertyValue(text);
}

std::vector<AddablePropertyOption> ListAddableProperties(UiNode* node,
                                                          const std::vector<PropertyGridSection>& grid) {
    std::vector<AddablePropertyOption> options;
    if (!node) {
        return options;
    }

    std::unordered_set<std::string> present{"id"};
    for (const PropertyGridSection& section : grid) {
        for (const PropertyGridRow& row : section.rows) {
            present.insert(row.metadata.name);
            if (IsListable(row)) {
                options.push_back(AddablePropertyOption{row.metadata, row.value, true});
            }
        }
    }

    const ComponentMetadata* component = ComponentRegistry::Instance().Find(TypeOf(node));
    const bool isContainer = component && component->designerCapabilities.isContainer;
    for (const CatalogEntry& entry : Catalog()) {
        if (entry.containerOnly && !isContainer) {
            continue;
        }
        if (present.count(entry.metadata.name) || node->HasProperty(entry.metadata.name)) {
            continue;
        }
        options.push_back(AddablePropertyOption{entry.metadata, entry.metadata.defaultValue, false});
    }

    std::stable_sort(options.begin(), options.end(),
                      [](const AddablePropertyOption& a, const AddablePropertyOption& b) {
                          return static_cast<int>(a.metadata.category) < static_cast<int>(b.metadata.category);
                      });
    return options;
}

}
