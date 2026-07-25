#pragma once

#include <optional>

#include "design/design_document.h"
#include "imgui.h"
#include "panels/properties_panel.h"

namespace studio {

// Draws the Design canvas for one open .avaui document: computes
// layout via design::ComputeLayout, draws each DesignNode as a
// wireframe rectangle (type/id label, no real per-control styling
// yet -- see 08_DESIGNER_VIEW_PLAN.md section 8, question 2), and
// handles two interactions:
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
//
// Deliberately does NOT call ImGui::Begin/End itself -- unlike
// DrawToolboxPanel/DrawPreviewPanel (self-contained dock panels), this
// is meant to be called from *inside* the Code Editor tab's content
// area once Fase 1 wires EditorTab::view_mode in (see plan section 4:
// "DrawEditorPanel: si tab.is_avaui ... llama a DrawDesignerCanvas(...)
// en vez de editor.Render(...)"). It draws into a child window sized
// `size` at the caller's current cursor position, so the caller
// controls placement/sizing exactly like it would for editor.Render().
//
// Not wired into DrawEditorPanel/main.cpp yet -- this is Fase 2 as a
// standalone, compiled-but-unused base (same approach Fase 0 used for
// avaui_text/layout_engine/component_catalog), so it doesn't disturb
// anything that already works. Reordering/moving already-placed nodes
// (Fase 4) is not implemented here.
std::optional<PropertiesState> DrawDesignerCanvas(design::DesignDocument& doc, ImVec2 size);

} // namespace studio
