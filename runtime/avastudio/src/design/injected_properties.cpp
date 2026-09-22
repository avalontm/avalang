#include "design/injected_properties.h"

#include "components/IComponent.h"

namespace studio::design {

namespace {

void Snapshot(avalang::ui::IComponent* node, PropertySnapshot& out) {
    if (node == nullptr) return;
    std::unordered_set<std::string> keys;
    for (const std::string& name : node->PropertyNames()) {
        keys.insert(name);
    }
    out[node->NodeId()] = std::move(keys);
    for (avalang::ui::IComponent* child : node->Children()) {
        Snapshot(child, out);
    }
}

void Diff(avalang::ui::IComponent* node, const PropertySnapshot& before, std::vector<InjectedProperty>& out) {
    if (node == nullptr) return;
    const auto known = before.find(node->NodeId());
    for (const std::string& name : node->PropertyNames()) {
        if (known != before.end() && known->second.count(name) != 0) continue;
        out.push_back(InjectedProperty{node->NodeId(), name});
    }
    for (avalang::ui::IComponent* child : node->Children()) {
        Diff(child, before, out);
    }
}

void Index(avalang::ui::IComponent* node, std::unordered_map<std::string, avalang::ui::IComponent*>& out) {
    if (node == nullptr) return;
    out[node->NodeId()] = node;
    for (avalang::ui::IComponent* child : node->Children()) {
        Index(child, out);
    }
}

}

PropertySnapshot SnapshotProperties(avalang::ui::ComponentTree* tree) {
    PropertySnapshot snapshot;
    if (tree != nullptr) Snapshot(tree->Root(), snapshot);
    return snapshot;
}

std::vector<InjectedProperty> DiffInjectedProperties(const PropertySnapshot& before,
                                                      avalang::ui::ComponentTree* tree) {
    std::vector<InjectedProperty> injected;
    if (tree != nullptr) Diff(tree->Root(), before, injected);
    return injected;
}

size_t RevertInjectedProperties(avalang::ui::ComponentTree* tree, const std::vector<InjectedProperty>& injected,
                                const AuthoredPredicate& isAuthored) {
    if (tree == nullptr || injected.empty()) return 0;

    std::unordered_map<std::string, avalang::ui::IComponent*> nodes;
    Index(tree->Root(), nodes);

    size_t reverted = 0;
    for (const InjectedProperty& property : injected) {
        const auto found = nodes.find(property.nodeId);
        if (found == nodes.end()) continue;
        if (isAuthored && isAuthored(property.nodeId, property.key)) continue;
        if (!found->second->HasProperty(property.key)) continue;
        found->second->RemoveProperty(property.key);
        ++reverted;
    }
    return reverted;
}

}
