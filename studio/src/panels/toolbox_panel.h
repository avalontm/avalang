#pragma once

namespace studio {

// ImGui payload type shared between the drag source here and the drop
// targets in designer_canvas.cpp -- payload data is the component
// type string (e.g. "button"), NUL-terminated, exactly as
// ComponentTypeInfo::type stores it. Kept as one named constant so the
// two panels can't drift (a typo in either string would silently break
// drag&drop with no compile error).
constexpr const char* kToolboxDragDropId = "AVAUI_COMPONENT_TYPE";

// Draws the Toolbox panel (own ImGui::Begin/End, same self-contained
// style as DrawPreviewPanel/DrawPropertiesPanel): one row per entry in
// design::GetComponentCatalog(), each a BeginDragDropSource carrying
// its `type` string under kToolboxDragDropId.
//
// Per 08_DESIGNER_VIEW_PLAN.md section 5.5, this is only meaningful
// docked next to Explorer while a .avaui tab in Design view is active
// -- that visibility wiring is Fase 1/main.cpp's job (not done yet),
// so for now this always draws when called. It has no dependency on
// EditorTab/view_mode itself, so it's safe to call unconditionally
// once that wiring exists.
void DrawToolboxPanel();

} // namespace studio
