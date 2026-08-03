#pragma once

#include <optional>
#include <string>
#include <vector>

#include "design/design_document.h"
#include "imgui.h"
#include "panels/properties_panel.h"

namespace studio {

// ImGui payload type for dragging an ALREADY-PLACED node to move/
// reorder it (Fase 4) -- as opposed to kToolboxDragDropId
// (toolbox_panel.h), which is for dropping a brand-new node from the
// Toolbox. Payload data is the moved node's DesignNode::node_uid,
// NUL-terminated, same convention as kToolboxDragDropId. Both the drag
// source and every drop target for this live entirely inside
// designer_canvas.cpp, so unlike kToolboxDragDropId this doesn't need
// to be shared with another panel's .cpp -- it's declared here (rather
// than file-local) purely so it's documented next to
// DrawDesignerCanvas instead of buried in the .cpp.
constexpr const char* kNodeMoveDragDropId = "AVAUI_NODE_MOVE";

// Draws the Design canvas for one open .avaui document: computes
// layout via the real avaui pipeline (LayoutEngine, see
// design/live_render_bridge.h -- design::ComputeLayout is now only the
// fallback, see designer_canvas.cpp's DrawNode), draws each DesignNode
// with its real widget look (SceneCommandWalker::Walk, Fase 4.2) plus
// a selection/hover/drop-zone overlay, and handles three interactions:
//
//   - Click a rectangle -> doc.selected_uid is updated and a
//     PropertiesState is returned (same struct/shape DrawPreviewPanel
//     already returns for the read-only Preview tree, so whatever
//     wires this into main.cpp later can reuse that exact call site --
//     see 08_DESIGNER_VIEW_PLAN.md section 5.6 point 3). For a real
//     (non-synthetic) node this PropertiesState now has `editable =
//     true` plus `source_tab_id`/`selected_node_uid` filled in (see
//     `tab_id` param below and properties_panel.h's PropertyEdit) so
//     main.cpp can write edits back into `doc` -- Fase 3.
//   - Drop a Toolbox payload (kToolboxDragDropId, see toolbox_panel.h)
//     onto a container node -> a new DesignNode is appended to that
//     node's children (design::MakeNode, seeded with the catalog's
//     default_properties) and doc.dirty is set.
//   - A `Componente()` call-site node (PascalCase type, see
//     design::ComponentResolver::IsComponentCall) is drawn as the
//     REAL resolved subtree of the imported component instead of an
//     empty box, when `project_root` is non-empty (see below).
//   - Drag an already-placed (real, non-synthetic) node and drop it
//     onto another node (kNodeMoveDragDropId, see above) -> Fase 4:
//     design::MoveNode reparents/reorders it. Dropping on the
//     top/bottom band of the target inserts as a sibling before/after
//     it; dropping on the middle band of a CONTAINER target moves it
//     inside as a new last child instead (see designer_canvas.cpp's
//     HandleDropTarget for the exact band math). The page root can't
//     be dragged (no parent to remove it from) and a synthetic
//     (resolved-import) node can't be dragged or dropped onto, same
//     restriction as Toolbox drops -- see the `synthetic` note below.
//   - Ctrl+Click a real node that has a "click" event bound (Anexo
//     9.17/9.18, 08_DESIGNER_VIEW_PLAN.md Fase 6) -> runs that handler
//     against the tab's cached state VM (design::InvokeHandler), which
//     already has `state` AND `doc.code_behind`'s functions bound
//     (design::BuildStateVM + design::BindCodeBehind). A plain click
//     (no Ctrl) still only selects, same as before -- this is
//     additive, not a separate Run mode/view. Mutating `state` this
//     way clears the tab's display-prop eval_cache so the canvas shows
//     the new values next frame; it does NOT touch `doc` itself (no
//     write-back, nothing to save/undo) and does NOT work on a
//     synthetic (resolved-import) node, same restriction as Fase 4/5.
//
// Wired into DrawEditorPanel/main.cpp since Fase 2 (view_mode
// dispatch, Toolbox conditional visibility, Ctrl+S -> SaveAvauiFile --
// all already done, verified against the actual repo in the 08 plan
// doc's session notes).
//
// `project_root`: the same fixed base directory every `import
// "components/x"` in `doc` is resolved against (see
// design/component_resolver.h's constructor comment on why this must
// be a single project root, not derived from wherever the file being
// edited happens to live) -- pass EditorState::project_root. Empty
// string (the default) disables resolution entirely: every component
// call-site node just draws as today's empty labeled box, same
// behavior as before this parameter existed -- callers that don't
// care about resolved components (tests, etc.) don't need to change.
//
// Resolution here is READ-ONLY / for display purposes only: the
// resolved tree is a throwaway copy (fresh node_uids on every replaced
// subtree, see component_resolver.h), rebuilt from `doc.root` fresh
// every call -- never written back into `doc.root` itself. Clicking a
// node inside a resolved component still sets `doc.selected_uid` and
// returns its PropertiesState for inspection, but it does NOT accept
// Toolbox drops or Properties edits (there's nowhere real in `doc` to
// write those -- see designer_canvas.cpp's `synthetic` check). Editing
// inside an imported component's own file is how you'd change it, same
// as the .NET prototype.
//
// Fixed in 08_DESIGNER_VIEW_PLAN.md section 9.10 (previously a known
// limitation, section 9.8): resolution now happens ONCE, for the whole
// tree, before ComputeLayout runs -- not node-by-node while drawing.
// That means a call-site's parent lays out against the component's
// REAL resolved height, not a fixed one-leaf-row placeholder; a tall
// Navbar() correctly pushes its siblings down instead of overflowing a
// slot sized before resolution.
//
// `tab_id`: EditorTab::id of the tab `doc` belongs to (see
// editor_panel.h) -- stamped into any PropertiesState this call
// returns (PropertiesState::source_tab_id) so main.cpp can find `doc`
// again by tab id when it gets a PropertyEdit back from
// DrawPropertiesPanel, without this function needing to know anything
// about EditorState itself. Default -1 (no caller other than
// editor_panel.cpp exists today, but a hypothetical one that doesn't
// care about write-back just gets PropertiesState::editable = false
// for every selection, same "safe default" pattern as `project_root`).
//
// `out_generated_handler`: Fase 5 (08_DESIGNER_VIEW_PLAN.md section 6)
// -- double-clicking a real, non-synthetic "button" node calls
// design::EnsureClickHandler(doc, ...) directly (this function mutates
// `doc` itself, same as a Toolbox drop or Fase 4's move), and when
// that happens this frame, `*out_generated_handler` is set to the
// handler's function name so the caller (editor_panel.cpp's
// DrawEditorPanel) can switch the tab to Code view and jump the caret
// to the generated stub -- same "double-click a Button on the form"
// flow VS6 had, documented as "Workflow Futuro" in AGENTS_STUDIO.md.
// Cleared to "" at the top of every call when non-null, so a caller
// can tell "nothing generated this frame" from "still holds a stale
// name from three frames ago" without maintaining that itself. Ignored
// entirely when null (the default) -- a caller that doesn't pass this
// just doesn't get the Fase 5 jump-to-code behavior, same safe-default
// spirit as `project_root`/`tab_id` above.
// `tab_id` also keys the Fase 6 state-VM cache AND (9.16) the
// ComponentResolver/resolved-tree cache (08_DESIGNER_VIEW_PLAN.md
// 9.14's pendiente 1 / "cachear state_vm/evaluación" and 9.8 punto
// 3's "cachear ComponentResolver/resolved_root"): before these passes
// DrawDesignerCanvas built a fresh AvaVM (design::BuildStateVM),
// re-resolved every `import`/`Componente()` call from disk, and
// compiled every visible display-prop expression from scratch on every
// single frame, discarding it all at the end of the same call. Now all
// of that is kept in a module-local cache in designer_canvas.cpp,
// looked up by `tab_id`, and only rebuilt together when `doc.dirty`
// flips or `project_root` changes (see designer_canvas.cpp's
// DrawDesignerCanvas body for the exact invalidation rule -- and its
// DesignerVmCacheEntry comment for the known cross-tab staleness
// trade-off this introduces for imported components). Only real for
// `tab_id >= 0` -- a caller that passes the default -1 (no real caller
// today besides editor_panel.cpp) gets the old per-call
// build-and-destroy behavior instead, since -1 isn't a safe cache key
// (every such caller would collide on the same slot).
std::optional<PropertiesState> DrawDesignerCanvas(design::DesignDocument& doc, ImVec2 size,
                                                   const std::string& project_root = "",
                                                   int tab_id = -1,
                                                   std::string* out_generated_handler = nullptr);

// Frees the cached Fase 6 state VM AND the 9.16 ComponentResolver/
// resolved-tree cache (see DrawDesignerCanvas's `tab_id` note above)
// for a tab that's about to disappear. Must be called once, with the
// closing tab's EditorTab::id, right before it's actually removed from
// EditorState::tabs -- otherwise that tab's AvaVM (created by
// design::BuildStateVM inside the cache, never destroyed by
// DrawDesignerCanvas itself anymore once cached) leaks for the rest of
// the process's lifetime, since nothing else ever revisits that
// tab_id's cache slot again. (The resolver/resolved-tree half of the
// cache doesn't need explicit freeing -- it's plain C++ objects, not a
// raw handle like the VM -- but it's dropped here too so a closed
// tab's slot doesn't linger holding memory for no reason.) Safe to
// call for any tab_id, including one that was never a .avaui tab /
// never cached anything (a no-op lookup miss) -- callers don't need to
// check EditorTab::is_avaui first.
void InvalidateDesignerVmCache(int tab_id);

// Fase 10.1 (09_DESIGNER_CANVAS_UX_PLAN.md): el modo Preview dedicado
// que vivia aca (SetDesignerPreviewActive/IsDesignerPreviewActive/
// ResetDesignerPreviewState/GetDesignerPreviewLog/ClearDesignerPreviewLog
// + PreviewLogLine, Anexo 9.19) fue REMOVIDO -- perdio su proposito una
// vez que Fase 10 hace que el canvas dibuje los widgets reales SIEMPRE,
// sin depender de ningun toggle. Probar un handler `click` mientras se
// edita sigue siendo Ctrl+Click (Anexo 9.17/9.18, sin cambios, ver el
// comentario de DrawDesignerCanvas arriba) -- eso no era parte del modo
// Preview y no se toco. La infraestructura de bajo nivel que el modo
// Preview usaba para instalar sinks (VM::AlertSink/NavigateSink/
// PrintSink, ava_vm_set_alert_callback/ava_vm_set_navigate_callback,
// core/src/vm/vm.h) sigue existiendo en el core sin cambios -- lo que
// se elimino es la consola de UI que los mostraba, no el mecanismo en
// si; un futuro host puede seguir instalando esos sinks si hace falta
// mostrar ese output en otro lado.

} // namespace studio
