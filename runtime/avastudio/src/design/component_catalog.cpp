#include "design/component_catalog.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <unordered_map>

#include "components/PropertyValue.h"
#include "registry/ComponentTypeRegistry.h"
#include "util/csv.h"
#include "util/data_dir.h"

namespace studio::design {

namespace {

std::string ToLower(const std::string& value) {
    std::string result = value;
    std::transform(result.begin(), result.end(), result.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

struct CatalogMetadata {
    int order = 0;
    std::string category;
    std::string icon;
};

std::unordered_map<std::string, CatalogMetadata> LoadCatalogMetadata() {
    std::unordered_map<std::string, CatalogMetadata> metadata;
    std::string text;
    if (!util::ReadFileToString(util::ResolveDataDir() + "component_catalog.csv", text)) {
        return metadata;
    }
    auto rows = util::ParseCsv(text);
    for (size_t r = 1; r < rows.size(); ++r) {
        const auto& row = rows[r];
        if (row.size() < 4) continue;
        if (row[0].empty()) continue;
        CatalogMetadata entry;
        entry.order = std::atoi(row[1].c_str());
        entry.category = util::UnescapeCell(row[2]);
        entry.icon = util::UnescapeCell(row[3]);
        metadata[row[0]] = std::move(entry);
    }
    return metadata;
}

std::string ToDisplayString(const avalang::ui::PropertyValue& value) {
    switch (value.Type()) {
        case avalang::ui::PropertyType::Bool:
            return value.AsBool() ? "true" : "false";
        case avalang::ui::PropertyType::Number: {
            double number = value.AsNumber();
            double rounded = std::round(number);
            if (std::fabs(number - rounded) < 1e-9) {
                return std::to_string(static_cast<long long>(rounded));
            }
            std::ostringstream out;
            out << number;
            return out.str();
        }
        case avalang::ui::PropertyType::String:
            return value.AsString();
        case avalang::ui::PropertyType::Nil:
        default:
            return "";
    }
}

} // namespace

const std::vector<ComponentTypeInfo>& GetComponentCatalog() {
    static const std::vector<ComponentTypeInfo> catalog = [] {
        std::vector<ComponentTypeInfo> built;
        const std::unordered_map<std::string, CatalogMetadata> metadata = LoadCatalogMetadata();
        int next_order = 1000;
        for (const auto& descriptor : avalang::ui::registry::GetComponentTypeRegistry()) {
            ComponentTypeInfo info;
            info.type = ToLower(descriptor.type);
            info.display_name = descriptor.display_name;
            info.is_container = descriptor.is_container;
            for (const auto& prop : descriptor.default_properties) {
                info.default_properties.push_back({prop.name, ToDisplayString(prop.value)});
            }
            auto it = metadata.find(info.type);
            if (it != metadata.end()) {
                info.order = it->second.order;
                info.category = it->second.category;
                info.icon = it->second.icon;
            } else {
                info.order = next_order++;
                info.category = info.is_container ? "Layout" : "Controles";
            }
            built.push_back(std::move(info));
        }
        std::stable_sort(built.begin(), built.end(),
                          [](const ComponentTypeInfo& a, const ComponentTypeInfo& b) {
                              return a.order < b.order;
                          });
        return built;
    }();
    return catalog;
}

const ComponentTypeInfo* FindComponentType(const std::string& type) {
    for (const auto& info : GetComponentCatalog()) {
        if (info.type == type) return &info;
    }
    return nullptr;
}

} // namespace studio::design
