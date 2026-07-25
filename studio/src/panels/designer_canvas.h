#pragma once

#include <optional>
#include <string>

#include "design/design_document.h"
#include "imgui.h"
#include "panels/properties_panel.h"

namespace studio {

// Draws the Design canvas for one open .avaui document: computes
// layout via design::ComputeLayout, draws each DesignNode as a
// wireframe rectangle (type/id label, no real per-control styling
// yet -- see 08_DESIGNER_VIEW_PLAN.md section 8, question 2), and
// handles three interactions:
//
//   - Click a rectangle -> doc.selected_uid is updated and a
//     PropertiesState is returned (same struct/shape DrawPreviewPanel
//     already returns for the read-only Preview tree, so whatever
//     wires this into main.cpp later can reuse that exact call site --
//     see 08_DESIGNER_VIEW_PLAN.md section 5.6 point 3).
//   - Drop a Toolbox payload (kToolboxDragDropId, see toolbox_panel.h)
//     onto a container node -> a new DesignNode is appended to that
//     node's children (design::MakeNode, seeded with the catalog's
//     default_properties) and doc.dirty is set.
//   - A `Componente()` call-site node (PascalCase type, see
//     design::ComponentResolver::IsComponentCall) is drawn as the
//     REAL resolved subtree of the imported component instead of an
//     empty box, when `project_root` is non-empty (see below).
//
// Wired into DrawEditorPanel/main.cpp since Fase 2 (view_mode
// dispatch, Toolbox conditional visibility, Ctrl+S -> SaveAvauiFile --
// all already done, verified against the actual repo in the 08 plan
// doc's session notes). Reordering/moving already-placed nodes (Fase
// 4) is not implemented here.
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
// Resolution here is READ-ONLY / for display purposes only: a
// resolved subtree is a throwaway copy (fresh node_uids, see
// component_resolver.h), never written into `doc.root` -- clicking a
// node inside it still sets `doc.selected_uid` and returns its
// PropertiesState for inspection, but it does NOT accept Toolbox
// drops (there's nowhere real in `doc` to append to -- see
// designer_canvas.cpp's DrawNode `synthetic` parameter). Editing
// inside an imported component's own file is how you'd change it,
// same as the .NET prototype.
//
// Known limitation (not blocking, see 08_DESIGNER_VIEW_PLAN.md section
// 9.8): the call-site node's OWN reserved space in its parent's layout
// is still computed by design::ComputeLayout against the unresolved
// tree (a childless node -> one default leaf-height slot, see
// layout_engine.cpp), *before* resolution happens -- resolution here
// only fills that slot with the resolved subtree's own independent
// layout pass. A resolved component taller than one leaf row will
// overflow/clip its slot visually instead of the parent reflowing
// around its real height. Fixing that needs layout_engine.cpp itself
// to know about resolution, which is a bigger change than this pass
// -- out of scope here, same "no bloqueante" spirit as the rest of
// this plan's incremental phases.
std::optional<PropertiesState> DrawDesignerCanvas(design::DesignDocument& doc, ImVec2 size,
                                                   const std::string& project_root = "");

} // namespace studio
