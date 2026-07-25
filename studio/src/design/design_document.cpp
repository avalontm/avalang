#include "design/design_document.h"

#include <atomic>
#include <fstream>

#include "design/avaui_text.h"
#include "design/component_catalog.h"
#include "util/data_dir.h"

namespace studio::design {

std::string GenerateNodeUid() {
    // Process-lifetime counter is enough -- see the header comment on
    // DesignNode::node_uid, these are never persisted or compared
    // across runs. Atomic only so a future background load (if that
    // ever happens) can't race with the UI thread; today everything
    // calling this runs on the single UI thread anyway.
    static std::atomic<unsigned long long> counter{0};
    return "n" + std::to_string(counter.fetch_add(1, std::memory_order_relaxed));
}

DesignNode MakeNode(const std::string& type) {
    DesignNode node;
    node.node_uid = GenerateNodeUid();
    node.type = type;
    if (const ComponentTypeInfo* info = FindComponentType(type)) {
        node.properties = info->default_properties;
    }
    return node;
}

DesignDocument NewBlankAvauiDocument() {
    DesignDocument doc;
    doc.root = MakeNode("page");
    return doc;
}

bool LoadAvauiFile(const std::string& path, DesignDocument& out_doc, std::string& out_error) {
    std::string text;
    if (!util::ReadFileToString(path, text)) {
        out_error = "no se pudo abrir el archivo";
        return false;
    }

    DesignDocument doc;
    if (!ParseAvauiText(text, doc.root, doc.code_behind, doc.initial_state, doc.imports, out_error)) {
        return false;
    }
    doc.dirty = false;
    out_doc = std::move(doc);
    return true;
}

bool SaveAvauiFile(const DesignDocument& doc, const std::string& path) {
    std::ofstream file(path, std::ios::binary);
    if (!file) return false;
    file << WriteAvauiText(doc.root, doc.code_behind, doc.initial_state, doc.imports);
    return static_cast<bool>(file);
}

DesignNode* FindNodeByUid(DesignNode& root, const std::string& uid) {
    if (root.node_uid == uid) return &root;
    for (DesignNode& child : root.children) {
        if (DesignNode* found = FindNodeByUid(child, uid)) return found;
    }
    return nullptr;
}

} // namespace studio::design
