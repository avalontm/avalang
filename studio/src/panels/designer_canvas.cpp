#include "panels/designer_canvas.h"

#include <algorithm>
#include <string>

#include "design/component_catalog.h"
#include "design/layout_engine.h"
#include "imgui.h"
#include "palette.h"
#include "panels/toolbox_panel.h"

namespace studio {

namespace {

ImVec2 ToImVec2(const design::Rect& r, ImVec2 origin) {
    return ImVec2(origin.x + r.x, origin.y + r.y);
}

// Builds the PropertiesState the rest of the app already knows how to
// display (properties_panel.h) from a selected DesignNode -- same
// shape DrawPreviewPanel produces for the read-only demo tree, see
// designer_canvas.h's header comment for why that's deliberate.
PropertiesState ToPropertiesState(const design::DesignNode& node) {
    PropertiesState state;
    state.selected_component_type = node.type;
    state.selected_component_id = node.id;
    state.properties = node.properties;
    return state;
}

// Drop target logic shared by every rectangle: only containers accept
// drops (matches ComponentTypeInfo::is_container -- dropping a Button
// onto another Button doesn't make sense and layout_engine.cpp has no
// notion of a leaf gaining children anyway). On accept, appends a
// fresh design::MakeNode(type) to `node.children` and marks the
// document dirty so a future Ctrl+S wiring (Fase 1/main.cpp) knows to
// call design::SaveAvauiFile.
void HandleDropTarget(design::DesignNode& node, design::DesignDocument& doc) {
    const design::ComponentTypeInfo* info = design::FindComponentType(node.type);
    const bool is_container = info != nullptr && info->is_container;
    if (!is_container) return;

    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kToolboxDragDropId)) {
            // Payload data is the type string including its NUL
            // terminator (see toolbox_panel.cpp's SetDragDropPayload
            // call, `info.type.size() + 1`), so constructing a
            // std::string straight from the char* buffer is safe and
            // doesn't need payload->DataSize.
            const std::string dropped_type(static_cast<const char*>(payload->Data));
            node.children.push_back(design::MakeNode(dropped_type));
            doc.dirty = true;
        }
        ImGui::EndDragDropTarget();
    }
}

// Recursively draws one node's rect (using the already-computed
// layout) plus its children, updates `out_selected` on click, and
// wires up HandleDropTarget for containers. `origin` is the canvas's
// screen-space top-left, since layout_engine's Rects are relative to
// whatever `available_space` ComputeLayout was called with (see
// layout_engine.h).
void DrawNode(design::DesignNode& node, const design::LayoutResult& layout, ImVec2 origin,
              design::DesignDocument& doc, std::optional<PropertiesState>& out_selected) {
    const auto it = layout.rects.find(node.node_uid);
    if (it == layout.rects.end()) return; // shouldn't happen -- ComputeLayout visits every node
    const design::Rect& r = it->second;

    const ImVec2 p0 = ToImVec2(r, origin);
    const ImVec2 p1 = ImVec2(p0.x + r.w, p0.y + r.h);

    const bool selected = (node.node_uid == doc.selected_uid);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // Fill so empty containers (no visible children yet) are still
    // clickable/droppable, not just an invisible zero-content area --
    // slightly tinted for containers vs leaves so nesting reads at a
    // glance in the wireframe.
    const design::ComponentTypeInfo* info = design::FindComponentType(node.type);
    const bool is_container = info != nullptr && info->is_container;
    const ImU32 fill = is_container ? palette::U32FromHex(palette::kSurface, 0.6f)
                                     : palette::U32FromHex(palette::kCard, 0.9f);
    const ImU32 border = selected ? palette::U32FromHex(palette::kPrimary)
                                   : palette::U32FromHex(palette::kBorder);

    draw_list->AddRectFilled(p0, p1, fill, 2.0f);
    draw_list->AddRect(p0, p1, border, 2.0f, 0, selected ? 2.0f : 1.0f);

    std::string label = node.type;
    if (!node.id.empty()) label += " (" + node.id + ")";
    draw_list->AddText(ImVec2(p0.x + 4.0f, p0.y + 4.0f), palette::U32FromHex(palette::kTextPrimary),
                        label.c_str());

    // Invisible button over the rect gives us click + drag-drop-target
    // hit-testing without fighting ImGui's normal widget/window input
    // routing (a raw draw_list rect has no input of its own).
    //
    // Known base-phase limitation: a container's hit area is its full
    // rect, and children are drawn (and hit-tested) on top of it in
    // the same pass -- for a click that lands inside a child, both the
    // child's and the parent's InvisibleButton cover that point.
    // ImGui resolves overlapping items to whichever was added last, so
    // children (added after their parent below) correctly win clicks
    // over the parent. This wasn't stress-tested against deeply
    // nested/overlapping "stack" layouts; Fase 4 (move/reorder by
    // drag) is the point where this may need SetItemAllowOverlap or an
    // explicit topmost-hit-test pass instead of relying on add-order.
    ImGui::SetCursorScreenPos(p0);
    ImGui::PushID(node.node_uid.c_str());
    ImGui::InvisibleButton("##node_hit_area", ImVec2(std::max(r.w, 1.0f), std::max(r.h, 1.0f)));
    if (ImGui::IsItemClicked()) {
        doc.selected_uid = node.node_uid;
        out_selected = ToPropertiesState(node);
    }
    HandleDropTarget(node, doc);
    ImGui::PopID();

    for (design::DesignNode& child : node.children) {
        DrawNode(child, layout, origin, doc, out_selected);
    }
}

} // namespace

std::optional<PropertiesState> DrawDesignerCanvas(design::DesignDocument& doc, ImVec2 size) {
    std::optional<PropertiesState> selected;

    ImGui::BeginChild("##DesignerCanvas", size, true, ImGuiWindowFlags_HorizontalScrollbar);

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const design::Rect canvas_rect{0.0f, 0.0f, std::max(avail.x, 1.0f), std::max(avail.y, 1.0f)};

    const design::LayoutResult layout = design::ComputeLayout(doc.root, canvas_rect);
    DrawNode(doc.root, layout, origin, doc, selected);

    // Root itself (the page) also accepts drops -- e.g. the very first
    // control on a blank page, before any container has been placed
    // inside it. DrawNode already ran HandleDropTarget for doc.root
    // as part of the recursion above, so nothing else is needed here;
    // this comment just documents that the empty-canvas case is
    // covered, not skipped.

    ImGui::EndChild();
    return selected;
}

} // namespace studio
