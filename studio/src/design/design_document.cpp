#include "design/design_document.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <fstream>
#include <unordered_set>

#include "design/avaui_text.h"
#include "design/component_catalog.h"
#include "util/data_dir.h"

namespace studio::design {

namespace {

// Strips anything that isn't a valid AvaLang identifier char and makes
// sure the result doesn't start with a digit -- used to turn a
// DesignNode::id (free text, could be anything the user typed into
// Properties) into a safe handler function name for EnsureClickHandler
// below. Falls back to "control" if nothing usable survives (e.g. an
// id that was entirely punctuation/whitespace).
std::string SanitizeIdentifier(const std::string& raw) {
    std::string out;
    for (unsigned char c : raw) {
        if (std::isalnum(c) || c == '_') out += static_cast<char>(c);
    }
    if (out.empty()) out = "control";
    if (std::isdigit(static_cast<unsigned char>(out[0]))) out = "_" + out;
    return out;
}

void CollectIds(const DesignNode& node, std::unordered_set<std::string>& out) {
    if (!node.id.empty()) out.insert(node.id);
    for (const DesignNode& child : node.children) CollectIds(child, out);
}

// Next free "<prefix><N>" id across the whole tree, starting at 1 --
// VS6-style auto-naming ("button1", "button2", ...) for a control that
// doesn't have one yet by the time EnsureClickHandler needs to name
// its handler after it.
std::string NextAutoId(const DesignNode& root, const std::string& prefix) {
    std::unordered_set<std::string> ids;
    CollectIds(root, ids);
    int n = 1;
    std::string candidate;
    do {
        candidate = prefix + std::to_string(n);
        ++n;
    } while (ids.find(candidate) != ids.end());
    return candidate;
}

bool CodeBehindHasFunc(const std::string& code_behind, const std::string& name) {
    // Crude substring search rather than parsing code_behind as
    // AvaLang -- good enough to answer "does a stub already exist"
    // without pulling the VM/parser into the Designer's edit path.
    // False positives (e.g. the name appearing inside a comment or
    // string) are harmless here: worst case is skipping a stub that
    // was needed, which just means double-clicking again re-adds it.
    return code_behind.find("func " + name + "(") != std::string::npos;
}

void AppendHandlerStub(DesignDocument& doc, const std::string& name) {
    if (!doc.code_behind.empty() && doc.code_behind.back() != '\n') doc.code_behind += "\n";
    if (!doc.code_behind.empty()) doc.code_behind += "\n"; // blank line between stubs
    doc.code_behind += "func " + name + "(sender, e)\n    -- TODO: " + name + "\nend\n";
}

} // namespace

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

DesignNode* FindParentOfUid(DesignNode& root, const std::string& uid) {
    for (DesignNode& child : root.children) {
        if (child.node_uid == uid) return &root;
        if (DesignNode* found = FindParentOfUid(child, uid)) return found;
    }
    return nullptr;
}

bool NodeContainsUid(const DesignNode& node, const std::string& uid) {
    if (node.node_uid == uid) return true;
    for (const DesignNode& child : node.children) {
        if (NodeContainsUid(child, uid)) return true;
    }
    return false;
}

bool MoveNode(DesignDocument& doc, const std::string& moved_uid, const std::string& target_uid,
              DropZone zone) {
    if (moved_uid == target_uid) return false; // dropped on itself -- no-op
    if (moved_uid == doc.root.node_uid) return false; // the page root can't be moved

    DesignNode* moved = FindNodeByUid(doc.root, moved_uid);
    if (!moved) return false; // stale payload (node no longer exists) -- tolerate silently

    // Refuse to make a node its own descendant (e.g. dragging a
    // container on top of one of its own children) -- checked BEFORE
    // any mutation, against the tree exactly as it stands right now.
    if (NodeContainsUid(*moved, target_uid)) return false;

    // Copy the subtree out before touching any vector. Erasing an
    // element from a std::vector<DesignNode> move-assigns every
    // element after it down by one slot in place -- a raw pointer held
    // across that erase (e.g. to a sibling that comes after the erased
    // node) would silently end up pointing at the wrong node's data.
    // `extracted` is a fully independent object from here on, so none
    // of that matters for it.
    DesignNode extracted = *moved;

    DesignNode* old_parent = FindParentOfUid(doc.root, moved_uid);
    if (!old_parent) return false; // shouldn't happen -- moved_uid != root.node_uid, checked above
    {
        std::vector<DesignNode>& siblings = old_parent->children;
        siblings.erase(std::remove_if(siblings.begin(), siblings.end(),
                                       [&](const DesignNode& n) { return n.node_uid == moved_uid; }),
                       siblings.end());
    }

    // Every lookup from here on is done FRESH, after the erase above --
    // if the target happened to live in the same sibling vector as the
    // moved node, a pointer/index taken before the erase could now be
    // stale (see the comment on `extracted`).
    if (zone == DropZone::kInto) {
        if (DesignNode* target = FindNodeByUid(doc.root, target_uid)) {
            target->children.push_back(std::move(extracted));
        } else {
            // Target vanished mid-drag (shouldn't happen in practice)
            // -- put the node back where it was rather than losing it.
            old_parent->children.push_back(std::move(extracted));
        }
        doc.dirty = true;
        return true;
    }

    // kBefore / kAfter: insert as a sibling of target, inside target's
    // OWN parent's children -- also looked up fresh.
    DesignNode* target_parent = FindParentOfUid(doc.root, target_uid);
    if (!target_parent) {
        // Target is the root itself -- root has no siblings, so fall
        // back to "into" instead of silently dropping the node.
        doc.root.children.push_back(std::move(extracted));
        doc.dirty = true;
        return true;
    }

    std::vector<DesignNode>& siblings = target_parent->children;
    auto it = std::find_if(siblings.begin(), siblings.end(),
                            [&](const DesignNode& n) { return n.node_uid == target_uid; });
    if (it == siblings.end()) {
        // Target vanished mid-drag -- same fallback as above.
        target_parent->children.push_back(std::move(extracted));
        doc.dirty = true;
        return true;
    }
    const auto index = (it - siblings.begin()) + (zone == DropZone::kAfter ? 1 : 0);
    siblings.insert(siblings.begin() + index, std::move(extracted));
    doc.dirty = true;
    return true;
}

bool RemoveNode(DesignDocument& doc, const std::string& node_uid) {
    if (node_uid == doc.root.node_uid) return false; // the page root can't be deleted

    DesignNode* target = FindNodeByUid(doc.root, node_uid);
    if (!target) return false; // stale/synthetic uid -- nothing real to remove

    // Whether the current selection needs clearing is decided BEFORE
    // the erase below (which invalidates `target`) -- NodeContainsUid
    // covers both "the removed node itself was selected" and "the
    // selection was a descendant of it".
    const bool clears_selection = !doc.selected_uid.empty() && NodeContainsUid(*target, doc.selected_uid);

    DesignNode* parent = FindParentOfUid(doc.root, node_uid);
    if (!parent) return false; // shouldn't happen -- node_uid != root.node_uid, checked above

    std::vector<DesignNode>& siblings = parent->children;
    const auto before = siblings.size();
    siblings.erase(std::remove_if(siblings.begin(), siblings.end(),
                                   [&](const DesignNode& n) { return n.node_uid == node_uid; }),
                   siblings.end());
    if (siblings.size() == before) return false; // shouldn't happen -- target was just found above

    if (clears_selection) doc.selected_uid.clear();
    doc.dirty = true;
    return true;
}

std::string EnsureClickHandler(DesignDocument& doc, const std::string& uid) {
    DesignNode* node = FindNodeByUid(doc.root, uid);
    if (!node) return ""; // synthetic (resolved-import) node or stale uid

    if (node->id.empty()) {
        node->id = NextAutoId(doc.root, node->type);
        doc.dirty = true;
    }

    // Reuse an existing "click" binding as-is (see header comment --
    // this never renames a handler someone already has).
    for (PropertyRow& ev : node->events) {
        if (ev.key != "click") continue;
        if (ev.value.empty()) {
            ev.value = SanitizeIdentifier(node->id) + "_Click";
            doc.dirty = true;
        }
        if (!CodeBehindHasFunc(doc.code_behind, ev.value)) {
            AppendHandlerStub(doc, ev.value);
            doc.dirty = true;
        }
        return ev.value;
    }

    // No "click" event yet -- create one and its stub.
    const std::string handler = SanitizeIdentifier(node->id) + "_Click";
    node->events.push_back(PropertyRow{"click", handler});
    AppendHandlerStub(doc, handler);
    doc.dirty = true;
    return handler;
}

} // namespace studio::design
