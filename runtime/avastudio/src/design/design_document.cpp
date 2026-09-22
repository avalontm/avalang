#include "design/design_document.h"

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <functional>
#include <unordered_set>

#include "parser/AvauiParser.h"
#include "parser/AvauiPropertyCoercion.h"
#include "parser/AvauiWriter.h"
#include "components/IComponent.h"
#include "components/PropertyValue.h"
#include "events/AutoBind.h"

namespace studio::design {

namespace {

std::string NextAutoId(avalang::ui::IComponent* root, const std::string& prefix) {
    std::unordered_set<std::string> existing;
    std::function<void(avalang::ui::IComponent*)> collect = [&](avalang::ui::IComponent* n) {
        if (!n) return;
        if (const auto* idProp = n->GetProperty("id")) {
            if (idProp->Type() == avalang::ui::PropertyType::String) {
                existing.insert(idProp->AsString());
            }
        }
        for (auto* c : n->Children()) collect(c);
    };
    collect(root);
    int i = 1;
    while (existing.count(prefix + std::to_string(i))) ++i;
    return prefix + std::to_string(i);
}

std::string SanitizeIdentifier(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
            out.push_back(c);
        }
    }
    if (!out.empty() && std::isdigit(static_cast<unsigned char>(out[0]))) {
        out = "_" + out;
    }
    return out.empty() ? "handler" : out;
}

std::string SlotOf(avalang::ui::IComponent* parent, avalang::ui::IComponent* child) {
    for (const std::string& slot : parent->SlotNames()) {
        const auto& children = parent->SlotChildren(slot);
        if (std::find(children.begin(), children.end(), child) != children.end()) return slot;
    }
    return "default";
}

void InsertChildAt(avalang::ui::IComponent* parent, const std::string& slot, avalang::ui::IComponent* child,
                   size_t index) {
    std::vector<avalang::ui::IComponent*> ordered = parent->SlotChildren(slot);
    for (auto* existing : ordered) parent->RemoveChild(existing);
    ordered.insert(ordered.begin() + static_cast<std::ptrdiff_t>(std::min(index, ordered.size())), child);
    for (auto* sibling : ordered) parent->AddChild(sibling, slot);
}

size_t SiblingInsertIndex(avalang::ui::IComponent* parent, const std::string& slot, avalang::ui::IComponent* target,
                          DropZone zone) {
    const auto& siblings = parent->SlotChildren(slot);
    auto it = std::find(siblings.begin(), siblings.end(), target);
    if (it == siblings.end()) return siblings.size();
    const size_t targetIndex = static_cast<size_t>(std::distance(siblings.begin(), it));
    return zone == DropZone::kAfter ? targetIndex + 1 : targetIndex;
}

void CollectAuthoredProperties(avalang::ui::IComponent* node,
                                std::unordered_map<std::string, std::unordered_set<std::string>>& out) {
    if (!node) return;
    std::unordered_set<std::string> keys;
    for (const auto& name : node->PropertyNames()) {
        keys.insert(name);
    }
    out.emplace(node->NodeId(), std::move(keys));
    for (auto* child : node->Children()) {
        CollectAuthoredProperties(child, out);
    }
}

}

void SnapshotAuthoredProperties(DesignDocument& doc) {
    doc.authored_properties.clear();
    CollectAuthoredProperties(doc.Root(), doc.authored_properties);
}

bool IsPropertyAuthored(const DesignDocument& doc, const std::string& nodeId, const std::string& key) {
    auto it = doc.authored_properties.find(nodeId);
    if (it == doc.authored_properties.end()) return false;
    return it->second.count(key) != 0;
}

void MarkPropertyAuthored(DesignDocument& doc, const std::string& nodeId, const std::string& key) {
    doc.authored_properties[nodeId].insert(key);
}

void UnmarkPropertyAuthored(DesignDocument& doc, const std::string& nodeId, const std::string& key) {
    auto it = doc.authored_properties.find(nodeId);
    if (it == doc.authored_properties.end()) return;
    it->second.erase(key);
}

std::string GenerateNodeUid() {
    static std::atomic<uint64_t> counter{0};
    return "uid_" + std::to_string(counter.fetch_add(1));
}

DesignDocument NewBlankAvauiDocument() {
    DesignDocument doc;
    doc.tree = avalang::ui::ComponentTree::Create();
    auto* root = doc.tree->CreateComponent("Page");
    doc.tree->SetRoot(root);
    SnapshotAuthoredProperties(doc);
    return doc;
}

bool ParseAvauiText(const std::string& text, DesignDocument& out_doc, std::string& out_error,
                     const std::string& sourcePath,
                     avalang::ui::parser::ParseErrorInfo* out_info) {
    out_doc = DesignDocument{};
    out_doc.tree = avalang::ui::ComponentTree::Create();

    try {
        auto parsed = avalang::ui::parser::AvauiParser::Parse(text, sourcePath);
        out_doc.code_behind = parsed.code;
        for (const auto& [k, v] : parsed.state) {
            out_doc.initial_state.push_back(PropertyRow{k, v});
        }
        out_doc.const_state_names = parsed.constNames;
        out_doc.imports = parsed.imports;
        out_doc.extends = parsed.extends;

        if (parsed.tree && parsed.tree->Root()) {
            out_doc.tree = std::move(parsed.tree);
        }

        out_doc.animations.reserve(parsed.animations.size());
        for (const auto& spec : parsed.animations) {
            NodeAnimation entry;
            entry.spec = spec;
            if (out_doc.tree) {
                if (avalang::ui::IComponent* target = out_doc.tree->FindById(spec.target)) {
                    entry.node_id = target->NodeId();
                }
            }
            out_doc.animations.push_back(std::move(entry));
        }

        SnapshotAuthoredProperties(out_doc);
    } catch (const avalang::ui::parser::ParseError& e) {

        out_error = e.what();
        if (out_info) {
            out_info->message = e.RawMessage();
            out_info->line = e.Line();
            out_info->column = e.Column();
            out_info->source = e.Source();
        }
        return false;
    } catch (const std::exception& e) {
        out_error = e.what();
        return false;
    }

    return true;
}

bool LoadAvauiFile(const std::string& path, DesignDocument& out_doc, std::string& out_error,
                    avalang::ui::parser::ParseErrorInfo* out_info) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        out_error = "could not open " + path;
        return false;
    }
    std::ostringstream buf;
    buf << in.rdbuf();
    return ParseAvauiText(buf.str(), out_doc, out_error, path, out_info);
}

avalang::ui::parser::AvauiWriteOptions BuildWriteOptions(const DesignDocument& doc) {
    avalang::ui::parser::AvauiWriteOptions opts;
    opts.code_behind = doc.code_behind;
    opts.imports = doc.imports;
    opts.extends = doc.extends;
    opts.initial_state.reserve(doc.initial_state.size());
    for (const auto& row : doc.initial_state) {
        opts.initial_state.push_back(
            {row.key, row.value, doc.const_state_names.count(row.key) != 0});
    }

    for (const NodeAnimation* entry : ResolvedAnimations(doc)) {
        avalang::ui::IComponent* target = FindNodeById(doc.Root(), entry->node_id);
        if (!target) continue;
        avalang::ui::parser::AnimationSpec spec = entry->spec;
        spec.target = target->Id();
        opts.animations.push_back(std::move(spec));
    }

    return opts;
}

bool SaveAvauiFile(const DesignDocument& doc, const std::string& path) {
    if (!doc.tree || !doc.tree->Root()) return false;

    std::string text = avalang::ui::parser::WriteAvaui(doc.tree->Root(), BuildWriteOptions(doc));

    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << text;
    return true;
}

avalang::ui::IComponent* FindNodeById(avalang::ui::IComponent* root, const std::string& nodeId) {
    if (!root) return nullptr;
    if (root->NodeId() == nodeId) return root;
    for (auto* child : root->Children()) {
        if (auto* found = FindNodeById(child, nodeId)) return found;
    }
    return nullptr;
}

std::vector<const NodeAnimation*> AnimationsForNode(const DesignDocument& doc, const std::string& nodeId) {
    std::vector<const NodeAnimation*> result;
    if (nodeId.empty()) return result;
    for (const NodeAnimation& entry : doc.animations) {
        if (entry.node_id == nodeId) result.push_back(&entry);
    }
    return result;
}

std::vector<const NodeAnimation*> ResolvedAnimations(const DesignDocument& doc) {
    std::vector<const NodeAnimation*> result;
    avalang::ui::IComponent* root = doc.Root();
    if (!root) return result;
    for (const NodeAnimation& entry : doc.animations) {
        if (entry.node_id.empty()) continue;
        if (FindNodeById(root, entry.node_id)) result.push_back(&entry);
    }
    return result;
}

avalang::ui::IComponent* FindParentOf(avalang::ui::IComponent* root, avalang::ui::IComponent* target) {
    if (!root || !target) return nullptr;
    if (root == target) return nullptr;
    for (auto* child : root->Children()) {
        if (child == target) return root;
        if (auto* found = FindParentOf(child, target)) return found;
    }
    return nullptr;
}

bool NodeContains(avalang::ui::IComponent* node, avalang::ui::IComponent* target) {
    if (!node || !target) return false;
    if (node == target) return true;
    for (auto* child : node->Children()) {
        if (NodeContains(child, target)) return true;
    }
    return false;
}

bool MoveNode(avalang::ui::IComponent* root, const std::string& movedNodeId, const std::string& targetNodeId,
              DropZone zone) {
    if (!root) return false;
    if (movedNodeId == targetNodeId) return false;
    if (root->NodeId() == movedNodeId) return false;

    auto* moved = FindNodeById(root, movedNodeId);
    if (!moved) return false;
    auto* target = FindNodeById(root, targetNodeId);
    if (!target) return false;
    if (NodeContains(moved, target)) return false;

    avalang::ui::IComponent* targetParent = nullptr;
    if (zone != DropZone::kInto) {
        targetParent = FindParentOf(root, target);
        if (!targetParent) return false;
    }

    if (auto* oldParent = FindParentOf(root, moved)) oldParent->RemoveChild(moved);

    if (zone == DropZone::kInto) {
        target->AddChild(moved);
        return true;
    }

    const std::string slot = SlotOf(targetParent, target);
    InsertChildAt(targetParent, slot, moved, SiblingInsertIndex(targetParent, slot, target, zone));
    return true;
}

bool MoveNode(DesignDocument& doc, const std::string& movedNodeId, const std::string& targetNodeId,
              DropZone zone) {
    if (!doc.tree) return false;
    if (!MoveNode(doc.tree->Root(), movedNodeId, targetNodeId, zone)) return false;

    doc.dirty = true;
    return true;
}

std::string EnsureClickHandler(DesignDocument& doc, const std::string& nodeId) {
    if (!doc.tree) return "";
    auto* node = FindNodeById(doc.tree->Root(), nodeId);
    if (!node) return "";

    std::string id;
    if (const auto* idProp = node->GetProperty("id")) {
        if (idProp->Type() == avalang::ui::PropertyType::String) {
            id = idProp->AsString();
        }
    }
    if (id.empty()) {
        id = NextAutoId(doc.tree->Root(), node->TypeName());
        node->SetProperty("id", avalang::ui::PropertyValue(id));
    }

    std::string handlerName;
    if (const auto* clickProp = node->GetProperty("click")) {
        if (clickProp->Type() == avalang::ui::PropertyType::String) {
            handlerName = clickProp->AsString();
        }
    }

    if (handlerName.empty()) {
        handlerName = SanitizeIdentifier(id) + "_Click";
        node->SetProperty("click", avalang::ui::PropertyValue(handlerName));
    }

    std::string stub = "function " + handlerName + "()\nend\n";
    if (doc.code_behind.find("function " + handlerName) == std::string::npos) {
        if (!doc.code_behind.empty() && doc.code_behind.back() != '\n') {
            doc.code_behind.push_back('\n');
        }
        doc.code_behind += stub;
    }

    doc.dirty = true;
    return handlerName;
}

bool RemoveNode(DesignDocument& doc, const std::string& nodeId) {
    if (!doc.tree) return false;
    auto* root = doc.tree->Root();
    if (!root) return false;
    if (root->NodeId() == nodeId) return false;

    auto* node = FindNodeById(root, nodeId);
    if (!node) return false;

    auto* parent = FindParentOf(root, node);
    if (!parent) return false;

    parent->RemoveChild(node);
    doc.tree->DestroyComponent(node->Id());
    doc.authored_properties.erase(nodeId);

    doc.dirty = true;
    return true;
}

std::string AddComponentNode(DesignDocument& doc, const std::string& parentId, const std::string& type,
                              const std::string& id, const std::vector<PropertyRow>& properties) {
    if (!doc.tree) return "";
    auto* root = doc.tree->Root();
    if (!root) return "";

    avalang::ui::IComponent* parent = nullptr;
    if (parentId.empty()) {
        parent = root;
    } else {
        parent = FindNodeById(root, parentId);
    }
    if (!parent) return "";

    auto* node = doc.tree->CreateComponent(avalang::ui::parser::CanonicalTypeName(type));
    if (!id.empty()) {
        node->SetProperty("id", avalang::ui::PropertyValue(id));
        MarkPropertyAuthored(doc, node->NodeId(), "id");
    }
    for (const auto& prop : properties) {
        node->SetProperty(prop.key, avalang::ui::PropertyValue(prop.value));
        MarkPropertyAuthored(doc, node->NodeId(), prop.key);
    }
    parent->AddChild(node);

    doc.dirty = true;
    return node->NodeId();
}

bool EditComponentNode(DesignDocument& doc, const std::string& nodeId, const std::vector<PropertyRow>& properties,
                        const std::string* newId) {
    if (!doc.tree) return false;
    auto* node = FindNodeById(doc.tree->Root(), nodeId);
    if (!node) return false;

    for (const auto& prop : properties) {
        node->SetProperty(prop.key, avalang::ui::PropertyValue(prop.value));
        MarkPropertyAuthored(doc, nodeId, prop.key);
    }
    if (newId) {
        node->SetProperty("id", avalang::ui::PropertyValue(*newId));
        MarkPropertyAuthored(doc, nodeId, "id");
    }

    doc.dirty = true;
    return true;
}

}