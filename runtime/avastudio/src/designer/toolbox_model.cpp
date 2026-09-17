#include "designer/toolbox_model.h"

#include <algorithm>
#include <cctype>

namespace studio::designer {

namespace {

std::string ToLower(const std::string& value) {
    std::string result = value;
    std::transform(result.begin(), result.end(), result.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

}

ToolboxModel::ToolboxModel(const ComponentRegistry& registry) : registry_(registry) {}

bool ToolboxModel::Matches(const ComponentMetadata& entry, const std::string& query) const {
    if (query.empty()) {
        return true;
    }
    const std::string needle = ToLower(query);
    return ToLower(entry.displayName).find(needle) != std::string::npos ||
           ToLower(entry.type).find(needle) != std::string::npos;
}

std::vector<ToolboxSection> ToolboxModel::Sections(const std::string& query) const {
    std::vector<ToolboxSection> sections;
    for (const ComponentMetadata& entry : registry_.All()) {
        if (!Matches(entry, query)) {
            continue;
        }
        auto it = std::find_if(sections.begin(), sections.end(),
                                [&](const ToolboxSection& section) { return section.category == entry.category; });
        if (it == sections.end()) {
            sections.push_back(ToolboxSection{entry.category, {}});
            it = sections.end() - 1;
        }
        it->entries.push_back(&entry);
    }
    return sections;
}

}
