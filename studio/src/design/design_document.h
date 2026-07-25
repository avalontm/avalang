#pragma once

#include <string>
#include <vector>

#include "panels/properties_panel.h" // reuse PropertyRow

namespace studio::design {

// Host-side, MUTABLE mirror of an AvaComponent tree -- this is the
// in-memory model the Designer canvas edits directly. Deliberately NOT
// the same struct as EngineBridge::PreviewNode (that one stays a
// read-only snapshot used by the existing demo-tree Preview panel);
// see 08_DESIGNER_VIEW_PLAN.md section 5.2 for why they're kept
// separate instead of merged.
struct DesignNode {
    // Stable per-node identity used for selection and (later) drag/
    // drop hit-testing -- independent of position in the tree (which
    // shifts as siblings are added/removed/reordered) and independent
    // of DesignNode::id (the user-facing, user-editable "btnGuardar"
    // name, which can be blank or duplicated while editing). Generated
    // by GenerateNodeUid(), never persisted to the .avaui file -- a
    // freshly loaded document gets fresh uids assigned on load.
    std::string node_uid;

    std::string type; // matches ComponentTypeInfo::type, e.g. "button", "column"
    std::string id;   // user-facing id, e.g. "btnGuardar" -- empty is valid

    std::vector<PropertyRow> properties;
    std::vector<PropertyRow> events; // key = event name ("on_click"), value = handler
                                      // function name expected in DesignDocument::code_behind

    std::vector<DesignNode> children;
};

// One open .avaui document. Owned by the EditorTab that has it open
// (see 08_DESIGNER_VIEW_PLAN.md section 4) -- there's one of these per
// tab, same as one TextEditor per code tab.
struct DesignDocument {
    DesignNode root;

    // The file's "code" section (event handlers) -- see
    // 08_DESIGNER_VIEW_PLAN.md section 3 for the .avaui format. Shown
    // in the Code Editor's TextEditor widget when the tab's view mode
    // is toggled to Code (F7), same as any .ava buffer.
    std::string code_behind;

    // The file's top-level `state` block -- initial values for the
    // document's reactive state (see 08_DESIGNER_VIEW_PLAN.md section
    // 3), e.g. `counter = 0`. Not evaluated/bound to anything by the
    // Designer yet (no `ui.*` builtins wired up -- see plan section 2
    // point 3); this just needs somewhere to round-trip through
    // load/save without being silently dropped.
    std::vector<PropertyRow> initial_state;

    // `import "..."` lines from the file, preserved verbatim in the
    // order they appeared. Not resolved into actual component
    // subtrees yet (that needs multi-file Explorer wiring for
    // .avaui-to-.avaui imports -- see plan section 3's note on
    // ComponentResolver.cs) -- kept here purely so a file that already
    // has imports doesn't lose them on the next save.
    std::vector<std::string> imports;

    // DesignNode::node_uid of the currently-selected node in the
    // canvas, or empty for "nothing selected". Lives on the document
    // (not on some separate selection-state struct) so switching tabs
    // naturally preserves each document's own selection.
    std::string selected_uid;

    bool dirty = false;
};

// Generates a fresh node_uid, unique within this process's lifetime.
// Not persisted, not meant to be stable across app restarts -- purely
// an in-memory handle (see DesignNode::node_uid above).
std::string GenerateNodeUid();

// Builds a new DesignNode of `type`, seeded with that type's
// ComponentTypeInfo::default_properties from the catalog (empty
// properties if `type` isn't in the catalog) and a fresh node_uid.
DesignNode MakeNode(const std::string& type);

// A brand-new, empty .avaui document: a single root "page" node, no
// children, no code-behind. What File > New .avaui File starts from.
DesignDocument NewBlankAvauiDocument();

// Loads and parses `path` (see design/avaui_text.h for the actual file
// format -- AvaLang UI text, NOT JSON, see
// 08_DESIGNER_VIEW_PLAN.md section 3). Returns false and leaves `out_doc` untouched if the file
// can't be read or fails to parse -- `out_error` gets a human-readable
// reason either way. Every node in the loaded tree gets a fresh
// node_uid (see DesignNode::node_uid) since those are never persisted.
bool LoadAvauiFile(const std::string& path, DesignDocument& out_doc, std::string& out_error);

// Writes `doc` to `path` in the .avaui format. Returns false if the
// file can't be opened for writing. Does not touch `doc.dirty` --
// same convention as editor_panel.h's SaveTab(), the caller clears it.
bool SaveAvauiFile(const DesignDocument& doc, const std::string& path);

// Depth-first search for the node with node_uid == `uid`, starting at
// `root` (root itself is checked first). Returns nullptr if not found.
// Non-const overload only -- callers needing a read-only lookup can
// still call this and just not mutate through the pointer.
DesignNode* FindNodeByUid(DesignNode& root, const std::string& uid);

} // namespace studio::design
