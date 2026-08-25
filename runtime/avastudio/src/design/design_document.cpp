#include "design/design_document.h"

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <functional>
#include <unordered_set>

#include "parser/AvauiParser.h"
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
        out_doc.imports = parsed.imports;
        out_doc.extends = parsed.extends;

        if (parsed.tree && parsed.tree->Root()) {
            out_doc.tree = std::move(parsed.tree);
        }
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

bool SaveAvauiFile(const DesignDocument& doc, const std::string& path) {
    if (!doc.tree || !doc.tree->Root()) return false;

    std::string text = [&] {
        avalang::ui::parser::AvauiWriteOptions opts;
        opts.code_behind = doc.code_behind;
        opts.imports = doc.imports;
        opts.initial_state.reserve(doc.initial_state.size());
        for (const auto& row : doc.initial_state) {
            opts.initial_state.push_back({row.key, row.value});
        }
        return avalang::ui::parser::WriteAvaui(doc.tree->Root(), opts);
    }();

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

bool MoveNode(DesignDocument& doc, const std::string& movedNodeId, const std::string& targetNodeId,
              DropZone zone) {
    if (!doc.tree) return false;
    auto* root = doc.tree->Root();
    if (!root) return false;
    if (movedNodeId == targetNodeId) return false;
    if (root->NodeId() == movedNodeId) return false;

    auto* moved = FindNodeById(root, movedNodeId);
    if (!moved) return false;
    auto* target = FindNodeById(root, targetNodeId);
    if (!target) return false;
    if (NodeContains(moved, target)) return false;

    auto* oldParent = FindParentOf(root, moved);
    if (oldParent) oldParent->RemoveChild(moved);

    if (zone == DropZone::kInto) {
        target->AddChild(moved);
    } else {
        auto* targetParent = FindParentOf(root, target);
        if (!targetParent) {
            if (oldParent) oldParent->AddChild(moved);
            return false;
        }
        auto siblings = targetParent->Children();
        auto it = std::find(siblings.begin(), siblings.end(), target);
        if (it == siblings.end()) {
            targetParent->AddChild(moved);
            return true;
        }
        size_t idx = std::distance(siblings.begin(), it);
        if (zone == DropZone::kBefore) {
            targetParent->RemoveChild(target);
            targetParent->AddChild(moved);
            targetParent->AddChild(target);
        } else {
            targetParent->RemoveChild(target);
            targetParent->AddChild(target);
            targetParent->AddChild(moved);
        }
    }

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

    if (!doc.selected_node_id.empty()) {
        auto* selected = FindNodeById(root, doc.selected_node_id);
        if (!selected || selected == node || NodeContains(node, selected)) {
            doc.selected_node_id.clear();
        }
    }

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

    auto* node = doc.tree->CreateComponent(type);
    if (!id.empty()) node->SetProperty("id", avalang::ui::PropertyValue(id));
    for (const auto& prop : properties) {
        node->SetProperty(prop.key, avalang::ui::PropertyValue(prop.value));
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
    }
    if (newId) {
        node->SetProperty("id", avalang::ui::PropertyValue(*newId));
    }

    doc.dirty = true;
    return true;
}

}
