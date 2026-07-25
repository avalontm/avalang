#include "panels/designer_canvas.h"

#include <algorithm>
#include <optional>
#include <string>

#include "design/component_catalog.h"
#include "design/component_resolver.h"
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
//
// `synthetic`: true when `node` is part of a resolved-component copy
// (see the recursion into resolved subtrees below) rather than a real
// node of `doc.root` -- HandleDropTarget is skipped for these (there's
// nowhere in `doc` to actually append the dropped child to) but click
// selection still works, for inspection. Always false for doc.root's
// own tree.
//
// `resolver`: non-null enables resolving `Componente()` call-site
// children into their real subtree for display (see designer_canvas.h
// on why this is read-only/display-only). Null preserves the old
// behavior exactly (call-site nodes draw as an empty labeled box, no
// resolution attempted) -- see DrawDesignerCanvas's `project_root`
// parameter.
void DrawNode(design::DesignNode& node, const design::LayoutResult& layout, ImVec2 origin,
              design::DesignDocument& doc, std::optional<PropertiesState>& out_selected,
              const design::ComponentResolver* resolver, bool synthetic) {
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
    // Synthetic (resolved-component) nodes get a small marker so it's
    // visually obvious this subtree came from an import, not from
    // doc.root directly -- otherwise a resolved Navbar() would look
    // indistinguishable from a hand-placed one, which matters here
    // since (unlike the real tree) it silently discards drops.
    if (synthetic) label += " [import]";
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
    // Resolved-component subtrees aren't part of doc.root -- nothing
    // to append a Toolbox drop to, so skip the drop target entirely
    // rather than silently accepting a drop that vanishes next frame
    // (see designer_canvas.h's header comment on this being
    // read-only/display-only for now).
    if (!synthetic) {
        HandleDropTarget(node, doc);
    }
    ImGui::PopID();

    for (design::DesignNode& child : node.children) {
        if (!synthetic && resolver != nullptr && design::ComponentResolver::IsComponentCall(child.type)) {
            const design::DesignNode* cached = resolver->GetComponentNode(child.type);
            if (cached != nullptr) {
                // Resolve into a throwaway copy (fresh node_uids, see
                // component_resolver.h) and lay it out fresh within
                // the call-site node's own already-computed rect --
                // see designer_canvas.h's "Known limitation" note on
                // why that rect's SIZE still comes from the
                // unresolved tree.
                const auto child_rect_it = layout.rects.find(child.node_uid);
                if (child_rect_it != layout.rects.end()) {
                    design::DesignNode resolved = resolver->ResolveComponentCall(child);
                    const design::LayoutResult inner_layout = design::ComputeLayout(resolved, child_rect_it->second);
                    DrawNode(resolved, inner_layout, origin, doc, out_selected, resolver, /*synthetic=*/true);
                    continue; // don't also draw the unresolved call-site node below
                }
            }
        }
        DrawNode(child, layout, origin, doc, out_selected, resolver, synthetic);
    }
}

} // namespace

std::optional<PropertiesState> DrawDesignerCanvas(design::DesignDocument& doc, ImVec2 size,
                                                   const std::string& project_root) {
    std::optional<PropertiesState> selected;

    // Built fresh every call (every frame, in practice) -- cheap by
    // design (see component_resolver.h's class comment), though that
    // does mean re-reading/re-parsing every imported .avaui off disk
    // once per frame while a Design tab with imports is visible. Fine
    // for the handful of components a real project has today; if that
    // ever shows up as an actual perf problem, the fix is caching a
    // resolver (and invalidating it on save) at the EditorTab level
    // instead of here -- not done now, no evidence yet it's needed.
    std::optional<design::ComponentResolver> resolver;
    if (!project_root.empty()) {
        resolver.emplace(project_root);
        resolver->ResolveImports(doc);
    }

    ImGui::BeginChild("##DesignerCanvas", size, true, ImGuiWindowFlags_HorizontalScrollbar);

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const design::Rect canvas_rect{0.0f, 0.0f, std::max(avail.x, 1.0f), std::max(avail.y, 1.0f)};

    const design::LayoutResult layout = design::ComputeLayout(doc.root, canvas_rect);
    DrawNode(doc.root, layout, origin, doc, selected, resolver ? &*resolver : nullptr, /*synthetic=*/false);

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
