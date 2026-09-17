#pragma once

#include <string>
#include <vector>

#include "designer/component_metadata.h"
#include "designer/component_registry.h"

namespace studio::designer {

struct ToolboxSection {
    std::string category;
    std::vector<const ComponentMetadata*> entries;
};

class ToolboxModel {
public:
    explicit ToolboxModel(const ComponentRegistry& registry = ComponentRegistry::Instance());

    std::vector<ToolboxSection> Sections(const std::string& query = "") const;
    bool Matches(const ComponentMetadata& entry, const std::string& query) const;

private:
    const ComponentRegistry& registry_;
};

}
