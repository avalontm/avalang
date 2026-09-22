#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "components/ComponentTree.h"

namespace studio::design {

struct InjectedProperty {
    std::string nodeId;
    std::string key;
};

using PropertySnapshot = std::unordered_map<std::string, std::unordered_set<std::string>>;
using AuthoredPredicate = std::function<bool(const std::string& nodeId, const std::string& key)>;

PropertySnapshot SnapshotProperties(avalang::ui::ComponentTree* tree);

std::vector<InjectedProperty> DiffInjectedProperties(const PropertySnapshot& before,
                                                      avalang::ui::ComponentTree* tree);

size_t RevertInjectedProperties(avalang::ui::ComponentTree* tree, const std::vector<InjectedProperty>& injected,
                                const AuthoredPredicate& isAuthored);

}
