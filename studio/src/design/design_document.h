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
    // Stable per-node identity used for selection and drag/drop
    // hit-testing (including Fase 4's move/reorder, see MoveNode
    // below) -- independent of position in the tree (which
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

// Depth-first search for the PARENT of the node with node_uid == `uid`
// -- i.e. the node whose `children` vector directly contains it.
// Returns nullptr if `uid` is `root.node_uid` itself (root has no
// parent) or if no node with that uid exists anywhere under `root`.
// Used by MoveNode (Fase 4, see below) to splice a node out of / into
// a sibling list without needing every caller to hand-roll the same
// recursive search.
DesignNode* FindParentOfUid(DesignNode& root, const std::string& uid);

// True if `uid` is `node.node_uid` itself, or belongs to any node in
// `node`'s subtree. Used by MoveNode to refuse turning a node into its
// own descendant (dragging a container on top of one of its own
// children/grandchildren) -- doing that would detach the subtree from
// the document entirely, so it's rejected as a no-op instead.
bool NodeContainsUid(const DesignNode& node, const std::string& uid);

// Where a dragged node lands relative to a drop-target node -- see
// MoveNode below. `kInto` means "become the target's last child"
// (only meaningful when the target is a container; designer_canvas.cpp
// only ever passes this when that's already been checked). `kBefore`/
// `kAfter` mean "become the target's sibling, immediately before/after
// it" -- these work for a leaf or container target alike, since they
// never touch the target's own children.
enum class DropZone { kBefore, kInto, kAfter };

// Fase 4 (08_DESIGNER_VIEW_PLAN.md section 6): moves the node with
// node_uid == `moved_uid` to a new position relative to the node with
// node_uid == `target_uid`, per `zone`. Returns false (and leaves
// `doc` untouched) for every case that isn't a real, safe move:
//   - `moved_uid == target_uid` (dropped on itself),
//   - `moved_uid == doc.root.node_uid` (the page root can't be moved
//     -- it has no parent to remove it from),
//   - `moved_uid` not found anywhere in `doc.root`,
//   - moving would make a node its own descendant (see
//     NodeContainsUid above).
// On success, the moved node (with its whole subtree, and its
// node_uid/id/properties/children all unchanged) ends up in its new
// spot, `doc.dirty` is set to true, and this returns true.
bool MoveNode(DesignDocument& doc, const std::string& moved_uid, const std::string& target_uid,
              DropZone zone);

// Fase 5 (08_DESIGNER_VIEW_PLAN.md section 6 / AGENTS_STUDIO.md's
// "Workflow Futuro"): ensures the node with node_uid == `uid` has a
// "click" event bound to a handler function name, and that a stub for
// that function (`func <name>(sender, e) ... end`) exists in
// `doc.code_behind`. Meant to be called from a double-click on a
// canvas node (designer_canvas.cpp) -- VS6's "double-click a Button on
// the form to jump to its Click handler", generating the handler the
// first time instead of erroring.
//
// Behavior:
//   - Returns "" (no-op, `doc` untouched) if `uid` doesn't resolve to
//     a real node under `doc.root` -- e.g. a synthetic resolved-import
//     node, or a stale uid.
//   - If the node's `id` is blank, assigns a fresh one first (e.g.
//     "button1", next free `type` + number across the whole tree --
//     see NextAutoId in the .cpp), same spirit as VS6 auto-naming an
//     unnamed control the first time you touch its code.
//   - If the node already has a "click" event with a handler name,
//     that exact name is reused (never renamed/overwritten here --
//     Properties' own event editor is what changes a handler's name
//     on purpose, see PropertyEditKind::kEvent) and only its stub gets
//     (re-)added to `code_behind` if missing (e.g. someone deleted the
//     func by hand in Code view but left the binding).
//   - Otherwise, generates "<id>_Click" (id sanitized to a valid
//     AvaLang identifier), stores it as the node's "click" event, and
//     appends its stub to `code_behind`.
// Sets `doc.dirty = true` whenever it actually changes anything.
std::string EnsureClickHandler(DesignDocument& doc, const std::string& uid);

// Fase 8 (09_DESIGNER_CANVAS_UX_PLAN.md): removes the node with
// node_uid == `node_uid` (and its whole subtree) from `doc`. Same
// family as MoveNode above -- a splice out of the parent's `children`
// vector, no reinsertion. Returns false (and leaves `doc` untouched)
// for every case that isn't a real, safe delete:
//   - `node_uid == doc.root.node_uid` (the page root can't be deleted
//     -- there'd be nothing left to draw/save),
//   - `node_uid` not found anywhere in `doc.root` (already gone, or a
//     synthetic/resolved-import uid that was never really in
//     `doc.root` to begin with -- callers are expected to check that
//     themselves before calling, same as MoveNode's callers do, but
//     this also fails safe if one doesn't).
// On success, sets `doc.dirty = true` and returns true. Also clears
// `doc.selected_uid` if it was the removed node itself OR any node
// inside its now-deleted subtree (a selection pointing at a node that
// no longer exists would otherwise dangle until something else
// happened to overwrite it) -- callers (designer_canvas.cpp) don't
// need to duplicate that check themselves.
bool RemoveNode(DesignDocument& doc, const std::string& node_uid);

} // namespace studio::design
