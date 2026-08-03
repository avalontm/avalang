#include "panels/designer_canvas.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "design/component_catalog.h"
#include "design/component_resolver.h"
#include "design/imgui_renderer.h"
#include "design/layout_engine.h"
#include "design/live_render_bridge.h"
#include "design/state_eval.h"
#include "commands/RenderCommandSink.h"
#include "commands/SceneCommandWalker.h"
#include "imgui.h"
#include "palette.h"
#include "panels/toolbox_panel.h"

// Fase 10 (image widget preview): raw GL symbols for glGenTextures/
// glTexImage2D (GLFW's header brings in the platform's OpenGL header,
// same reasoning as branding/logo_texture.cpp) and stb_image's
// DECLARATIONS only -- the actual implementation (STB_IMAGE_IMPLEMENTATION)
// lives in logo_texture.cpp, the one TU that defines it for the whole
// ava_studio binary; including it again here without that macro just
// gets the function prototypes so this file can call stbi_load itself.
#include "GLFW/glfw3.h"
#include "stb_image.h"

namespace studio {

namespace {

// Fase 6 caching pass (08_DESIGNER_VIEW_PLAN.md 9.14 pendiente 1 --
// "cachear state_vm/evaluación", the first of the two performance
// limitations 9.14 called out). One entry per open .avaui tab, keyed by
// EditorTab::id (never reused within a process -- see editor_panel.h's
// comment on EditorState::next_tab_id -- so a stale slot from a closed
// tab can never collide with a later tab reusing the same id).
struct DesignerVmCacheEntry {
    // Owned here once cached -- DrawDesignerCanvas no longer destroys
    // this at the end of every call the way it did before this pass
    // (see BuildStateVM's header comment in state_eval.h, still
    // accurate about ownership for the tab_id < 0 / uncached path).
    AvaVM* vm = nullptr;

    // doc.dirty as of the last time `vm` was (re)built. Rebuilding on
    // every transition (false->true from a fresh edit, true->false
    // after SaveTab clears it) rather than "whenever doc.dirty is
    // true" is a deliberate cheap approximation, not a full
    // dependency check on doc.initial_state itself: it means a SECOND
    // edit made before the first one is saved (dirty already true, so
    // no transition) reuses the stale VM. That's fine for `state`
    // itself (initial_state values, once loaded, essentially never
    // change from inside the Designer today -- there's no UI to edit
    // them, see design_document.h's initial_state comment), which is
    // the only thing actually bound INTO the VM. Display-prop text
    // edits (a button's `text`, etc.) don't need a VM rebuild at all
    // -- see eval_cache below, which is keyed to notice those without
    // needing this flag.
    bool last_dirty = false;

    // node_uid + '\x1f' + raw PropertyRow::value -> the string
    // EvalPropertyExpr already returned for that exact pair. Keying on
    // the raw value itself (not just node_uid) means an edited
    // display-prop's text is automatically a cache miss (new key) and
    // gets re-evaluated on the very next frame it's drawn, WITHOUT
    // needing to know "did doc.dirty change" -- sidesteps the
    // last_dirty staleness note above entirely for this half of the
    // cache. Cleared whenever `vm` is rebuilt (a new/changed `state`
    // binding can change what an old raw value now evaluates to, so
    // stale entries can't be trusted across a rebuild). Not otherwise
    // pruned -- bounded by "how many distinct raw values a display prop
    // has cycled through since the last dirty-transition", a handful
    // of keystrokes' worth in practice, not an unbounded leak.
    std::unordered_map<std::string, std::string> eval_cache;

    // 9.8 punto 3 / 9.16: the ComponentResolver + its resolved-tree
    // deep copy, cached alongside the VM under the SAME invalidation
    // rule (rebuilt together, see the `needs_rebuild` check in
    // DrawDesignerCanvas) since both depend on the same inputs --
    // doc.root's structure and doc.imports. `resolver` is only
    // engaged (has_value()) when `project_root` was non-empty on the
    // last rebuild; `resolved_root`/`real_uids` are only populated when
    // `resolver` is engaged AND doc.imports was non-empty (mirrors the
    // old per-frame "skipped entirely when nothing to resolve" case,
    // see DrawDesignerCanvas's header comment on that below).
    //
    // Known limitation, carried over unchanged from
    // component_resolver.h's class comment ("nothing here watches the
    // filesystem for changes, so a resolver shouldn't be kept around
    // across edits to the imported files"): caching this across frames
    // means editing an IMPORTED component's own .avaui file in a
    // DIFFERENT tab no longer refreshes this tab's canvas until this
    // tab's own doc.dirty transitions (e.g. any edit here, or a fresh
    // load). Before this pass every frame re-read/re-parsed the
    // imported file from disk, so that cross-tab edit showed up
    // immediately; now it's stale until this tab is touched. Accepted
    // trade-off, same "non-blocking, no evidence it matters in
    // practice" spirit as every other item on 9.8 punto 3's list.
    std::string cached_project_root;
    std::optional<design::ComponentResolver> resolver;
    std::optional<design::DesignNode> resolved_root;
    std::unordered_set<std::string> real_uids;

    // Fase 4.1 (AVAUI_DESIGNER_REAL_RENDER_PLAN.md): the real avaui
    // pipeline output for this tab's `root_to_draw`, rebuilt under the
    // SAME `needs_rebuild` condition as everything above PLUS whenever
    // the canvas viewport size changes (layout depends on it, unlike
    // the VM/resolver). `imgui_renderer` is reconstructed alongside
    // `live_render` since BaseRenderer stores width/height at
    // construction time; it has no other state worth preserving across
    // rebuilds.
    studio::design::LiveRenderResult live_render;
    std::unique_ptr<avalang::ui::ImGuiRenderer> imgui_renderer;
    int live_render_w = -1;
    int live_render_h = -1;
};

std::unordered_map<int, DesignerVmCacheEntry> g_designer_vm_cache;

// Fase 8 (09_DESIGNER_CANVAS_UX_PLAN.md): pending "delete this node"
// request -- set either by the Delete/Supr key (DrawDesignerCanvas) or
// the node's right-click context menu (DrawNode), consumed by
// DrawCanvasDeleteConfirmPopup below. Single global slot, same pattern
// as explorer_panel.cpp's g_delete_request -- only one confirmation
// can be on screen at a time app-wide, and `tab_id` is what lets the
// right tab's DrawDesignerCanvas call (and only that one) actually
// open/service the popup even though every open .avaui tab's canvas
// runs through the same DrawDesignerCanvas function.
struct CanvasDeleteRequest {
    bool open = false;
    int tab_id = -1;
    std::string node_uid;
};
CanvasDeleteRequest g_canvas_delete_request;

// "poder redimensionar arrastrando el handle" -- tracks an in-progress
// resize drag started from one of the selection frame's handles (see
// the resize-handle block in DrawNode). Same single-global-slot
// pattern as CanvasDeleteRequest just above: only one resize can be
// happening anywhere in the app at a time, so there's nothing to key
// by tab -- ImGui's own active-item tracking already guarantees only
// one handle can be the drag source in a given frame. `start_width`/
// `start_height` are captured once, the frame the drag begins (via
// ImGui::IsItemActivated()), from whatever the control's width/height
// was at that instant -- either an existing explicit "width"/"height"
// property, or (nothing set yet) its current live/estimated size --
// so the very first drag frame doesn't jump the control to some
// default size before the user has moved the mouse at all. Every
// following frame while active just re-applies start + the total
// mouse delta since activation (ImGui::GetMouseDragDelta), so the
// control tracks the cursor exactly rather than drifting from
// re-basing off a per-frame delta.
struct ResizeDragState {
    bool active = false;
    std::string node_uid;
    bool resize_x = false; // adjust "width"
    bool resize_y = false; // adjust "height"
    float start_width = 0.0f;
    float start_height = 0.0f;
};
ResizeDragState g_resize_drag;

// Reads `key` (e.g. "width") off `properties` as a number, same
// convention PropertyRow already uses everywhere else (display-ready
// string, parsed on demand) -- returns false (leaving `out`
// untouched) if the key isn't present or isn't a valid number, so
// callers can fall back to whatever size the control would otherwise
// have.
bool TryGetNumericProperty(const std::vector<PropertyRow>& properties, const char* key, float* out) {
    for (const PropertyRow& row : properties) {
        if (row.key != key) continue;
        try {
            size_t consumed = 0;
            const float value = std::stof(row.value, &consumed);
            if (consumed == 0) return false;
            *out = value;
            return true;
        } catch (...) {
            return false;
        }
    }
    return false;
}

// Writes `value` into `properties` under `key`, updating the existing
// PropertyRow if one's already there (so resizing an already-sized
// control doesn't leave a stale duplicate) or appending a new one.
// Rounded to whole pixels -- avaui's own LayoutEngine reads "width"/
// "height" as plain numbers (see LayoutEngineImpl.cpp), and whole
// pixels is what the Properties panel's own numeric fields show for
// every other size-like value, so a dragged size looks the same
// whether it came from the handle or was typed in by hand.
void SetSizeProperty(std::vector<PropertyRow>& properties, const char* key, float value) {
    const std::string text = std::to_string(static_cast<int>(std::lround(value)));
    for (PropertyRow& row : properties) {
        if (row.key == key) {
            row.value = text;
            return;
        }
    }
    properties.push_back(PropertyRow{key, text});
}

// Minimum size a drag can shrink a control to -- purely to stop a fast
// drag from collapsing a control to 0/negative px (which would then
// clamp oddly in avaui's own LayoutEngine, or vanish from the canvas
// entirely with no visible handle left to drag back out from).
constexpr float kMinResizeDimension = 12.0f;


// Fase 10.1: los trampolines de la consola de Preview que vivian aca
// (PreviewLogTrampoline/PreviewAlertTrampoline/PreviewNavigateTrampoline,
// instalados con ava_vm_set_print_callback/set_alert_callback/
// set_navigate_callback sobre `entry.vm`) fueron removidos junto con la
// consola misma -- ver designer_canvas.h. El mecanismo de sinks en si
// (VM::AlertSink/NavigateSink/PrintSink, core/src/vm/vm.h) sigue
// existiendo sin cambios; el Designer simplemente no instala ninguno
// hoy, ya que no le queda donde mostrar ese output.

ImVec2 ToImVec2(const design::Rect& r, ImVec2 origin) {
    return ImVec2(origin.x + r.x, origin.y + r.y);
}

// Visual gap (px) subtracted from every side of a node's layout rect
// before drawing/hit-testing it -- purely cosmetic, does NOT touch
// layout_engine.cpp's math (every rect layout hands out is still
// full-bleed, edge-to-edge, no gaps reserved between siblings). Without
// this, adjacent rects (e.g. two children of the same column, or a
// child against its container's own edge) share an exact border line,
// which reads as one solid block instead of legible nesting. Applied
// uniformly to fill/border/label/hit-area so the clickable area always
// matches what's actually drawn (important groundwork for Fase 4 --
// distinct, non-touching hit areas make drag-and-reorder's own
// hit-testing much less ambiguous at shared edges).
constexpr float kNodeMargin = 3.0f;

// Fase 10 (09_DESIGNER_CANVAS_UX_PLAN.md diagnóstico punto 4: "kNodeMargin
// fijo en 3px... a cualquier nivel de anidamiento razonable la separación
// visual entre padre e hijo prácticamente desaparece"). Margen efectivo =
// kNodeMargin + depth * kNodeMarginPerDepth, clamp a kNodeMarginMax --
// cada nivel de anidamiento gana un poco más de "aire" en vez de quedar
// fijo, sin volverse absurdo a 8-10 niveles de profundidad. Igual que
// kNodeMargin, puramente cosmético (designer_canvas.cpp), no toca
// layout_engine.cpp.
constexpr float kNodeMarginPerDepth = 1.5f;
constexpr float kNodeMarginMax = 12.0f;

// Fase 7 (09_DESIGNER_CANVAS_UX_PLAN.md): fixed-height header strip
// reserved at the top of every CONTAINER node's rect. This is the
// container's OWN exclusive hit-area (InvisibleButton -- see DrawNode)
// for selection/drag, so a container with children stays selectable
// and draggable by its header even though its children still cover
// its full remaining rect (same full-bleed layout math as before,
// layout_engine.cpp untouched -- see this constant's use in DrawNode).
// Purely a designer_canvas.cpp concern, same "cosmetic, doesn't touch
// layout_engine.cpp's math" spirit as kNodeMargin above.
constexpr float kHeaderHeight = 20.0f;

// The always-on corner "type chip" (see is_container's chip-drawing
// branch in DrawNode) -- hoisted to file scope so the real-content
// container-rect padding below (kRealContainerPadTop) can share the
// exact same numbers instead of a second, driftable copy.
constexpr float kChipPadX = 5.0f;
constexpr float kChipPadY = 2.0f;
constexpr float kChipHeight = 15.0f;

// BUGFIX (reported: the corner chip -- and the container's own top
// edge -- painting over its real content at deeper nesting levels,
// e.g. a "row" chip sitting on top of the first button in that row).
// Root cause: once a container's rect comes from real content
// (BoundsFromRealLeafRects, or the empty-container own-rect
// fallback -- see DrawNode's rect resolution), the box is TIGHT
// around its children, with none of the slack design::ComputeLayout's
// old, coarser math used to leave above them. The margin logic just
// below (kNodeMargin et al) then insets INWARD from whatever rect
// it's given -- and that inset GROWS with depth (kNodeMarginPerDepth),
// so a fixed pad here that looked fine for a shallow "column" still
// got eaten away to nothing (or negative, i.e. actual overlap) a
// couple of levels deeper, e.g. a "row" nested inside that "column".
// kRealContainerPadSide is sized so that even at the worst case --
// kNodeMarginMax eating into it -- there's still a few px of visible
// gap left on every side; kRealContainerPadTop is just that same
// margin plus room for the chip itself, so the two stay in lockstep
// as margin grows/shrinks with depth instead of drifting apart. Only
// containers get padded outward this way -- leaves stay exactly on
// their real rect, unpadded.
constexpr float kRealContainerPadSide = kNodeMarginMax + 4.0f;
constexpr float kRealContainerPadTop = kChipHeight + kRealContainerPadSide;

// BUGFIX (reported: selection frame reads as glued to/inside the
// control, confusable with its own label text at small sizes -- e.g.
// a "Clear" button). Root cause: for a real-rendered leaf
// (skip_leaf_wireframe below), p0/p1 is the control's true rect
// SHRUNK inward by kNodeMargin -- that inset exists to separate
// adjacent wireframe siblings, not to describe where a selection
// outline should sit. Drawing the selection border/handles at that
// inset rect put the frame a few px INSIDE the real button's edge
// instead of around it. kSelectionPad is the classic VB6/Delphi-style
// gap between a selected control's true bounds and the selection
// chrome drawn around it -- applied on top of the control's actual
// (unshrunk) rect, never the wireframe-inset one, so the frame always
// reads as clearly outside the control with visible breathing room.
constexpr float kSelectionPad = 4.0f;

// Builds the PropertiesState the rest of the app already knows how to
// display (properties_panel.h) from a selected DesignNode -- same
// shape DrawPreviewPanel produces for the read-only demo tree, see
// designer_canvas.h's header comment for why that's deliberate.
//
// `editable`: false for a synthetic (resolved-import) node -- there's
// no real DesignNode in any doc.root for those, so Properties shows
// them read-only (see properties_panel.cpp). `tab_id`: threaded
// straight from DrawDesignerCanvas's own `tab_id` param, stamped in
// as-is regardless of `editable` (harmless when unused, main.cpp only
// reads it off a PropertyEdit that DrawPropertiesPanel only emits when
// state.editable was true in the first place).
PropertiesState ToPropertiesState(const design::DesignNode& node, bool editable, int tab_id) {
    PropertiesState state;
    state.selected_component_type = node.type;
    state.selected_component_id = node.id;
    state.properties = node.properties;
    state.events = node.events;
    state.editable = editable;
    state.source_tab_id = tab_id;
    state.selected_node_uid = node.node_uid;
    return state;
}

// Fills `out` with every node_uid under (and including) `node` -- used
// once per frame to know which node_uids in the RESOLVED tree actually
// belong to `doc.root` (see DrawDesignerCanvas: ComponentResolver::
// ResolveTree preserves node_uid on every node it doesn't replace, and
// only mints fresh ones for a resolved component's copied subtree, see
// component_resolver.cpp's RegenerateUidsRecursive) -- so "is this uid
// in the set" is exactly "is this a real, editable/droppable node" vs.
// "this came from an imported component's own file".
void CollectUids(const design::DesignNode& node, std::unordered_set<std::string>& out) {
    out.insert(node.node_uid);
    for (const design::DesignNode& child : node.children) {
        CollectUids(child, out);
    }
}

// Fase 10: linear scan for a property's raw (unevaluated) value by key --
// used by the real-widget rendering below for props that aren't the
// type's single "display" prop (GetDisplayPropertyKey) but still affect
// how the widget looks (button's `enabled`, checkbox/radiobutton's
// `checked`). Returns `fallback` verbatim when the key isn't present at
// all -- same "never worse than before" spirit as EvalPropertyExpr.
std::string FindPropValue(const std::vector<PropertyRow>& props, const std::string& key,
                           const std::string& fallback) {
    for (const PropertyRow& p : props) {
        if (p.key == key) return p.value;
    }
    return fallback;
}

// Same lookup as FindPropValue but parsed as a float, for numeric
// visual props (e.g. `borderRadius`) consumed directly by ImGui style
// vars rather than displayed as text. strtof (not std::stof, which
// throws) so a missing/malformed value quietly falls back instead of
// needing a try/catch at every call site -- same "never worse than
// before" spirit as FindPropValue itself.
float FindPropValueF(const std::vector<PropertyRow>& props, const std::string& key,
                      float fallback) {
    const std::string raw = FindPropValue(props, key, std::string());
    if (raw.empty()) return fallback;
    char* end = nullptr;
    const float parsed = std::strtof(raw.c_str(), &end);
    return (end != raw.c_str()) ? parsed : fallback;
}

// Drop target logic shared by every rectangle. Two independent payload
// kinds are checked inside the SAME BeginDragDropTarget block (ImGui's
// intended pattern for "one item, multiple acceptable payload types"):
//
//   - kToolboxDragDropId (Fase 2, unchanged): a brand-new node from the
//     Toolbox. Only accepted when `is_container` (matches
//     ComponentTypeInfo::is_container -- dropping a Button onto
//     another Button doesn't make sense and layout_engine.cpp has no
//     notion of a leaf gaining children anyway). Appends a fresh
//     design::MakeNode(type) to the REAL node's children.
//   - kNodeMoveDragDropId (Fase 4, new): reorder/move an
//     already-placed node. Unlike the Toolbox payload this is accepted
//     on EVERY real node, container or leaf -- a leaf still needs to
//     be a valid drop target so you can reorder around it (drop it as
//     a sibling before/after). Which of "before" / "into" / "after"
//     happens depends on where in the target's rect the drop landed:
//     the top and bottom bands (each `kEdgeBandFrac` of the rect's
//     height) always mean "insert as a sibling"; the middle band means
//     "become a child" but only for a container -- on a leaf the
//     middle band just falls back to "after" instead of doing nothing
//     (a leaf has nowhere to put a child). See design::MoveNode for
//     the actual tree surgery.
//
// `node` here may be part of this frame's throwaway resolved-tree copy
// (see DrawDesignerCanvas) rather than doc.root itself -- but this is
// only ever called for non-synthetic nodes (see DrawNode), and
// ComponentResolver::ResolveTree preserves node_uid unchanged on every
// node it doesn't replace (see CollectUids above), so `node.node_uid`
// still matches its counterpart in doc.root exactly. Every mutation
// below goes through design:: lookups by that node_uid against
// doc.root (never through `node`/`real`/`target` pointers held across
// a mutation -- see design::MoveNode's own comments on why), which is
// what makes this correct regardless of whether resolution ran this
// frame and regardless of which vector either node happens to live in.
//
// `p0`/`p1`: the node's own drawn (already inset) rect in screen
// space, from DrawNode -- used only to turn the drop's mouse position
// into a band fraction for the move payload; the Toolbox payload
// doesn't care where in the rect it landed.
constexpr float kEdgeBandFrac = 0.25f; // top/bottom 25% each = "sibling", middle 50% = "into"

// Shared by the real (consuming) accept below AND the peek-only
// preview -- so the highlight drawn while still dragging always
// matches exactly what dropping right now would actually do.
design::DropZone ComputeDropZone(float mouse_y, ImVec2 p0, ImVec2 p1, bool is_container) {
    // Fase 7: with the header strip (kHeaderHeight) now being a
    // container's ONLY hit/drop rect (see DrawNode/HandleDropTarget
    // callers below), `p0`/`p1` here for a container IS the header --
    // there's no room left in it for a meaningful top/25%-bottom/25%
    // band split, so a drop anywhere on it always means "become the
    // last child" (kInto), per the plan's "Nota para Fase 9". Leaf
    // targets are unaffected -- they still use their full rect and the
    // original band math, so dropping near a leaf's own top/bottom
    // still means "insert as sibling before/after".
    if (is_container) return design::DropZone::kInto;
    const float height = std::max(p1.y - p0.y, 1.0f);
    const float frac = (mouse_y - p0.y) / height;
    return frac <= 0.5f ? design::DropZone::kBefore : design::DropZone::kAfter;
}

void HandleDropTarget(const design::DesignNode& node, design::DesignDocument& doc, bool is_container,
                       ImVec2 p0, ImVec2 p1) {
    if (ImGui::BeginDragDropTarget()) {
        // Visual-only preview: peek at a kNodeMoveDragDropId payload
        // (ImGuiDragDropFlags_AcceptPeekOnly does NOT consume it, so
        // this runs every single frame the mouse hovers this target
        // mid-drag, purely to draw which zone the drop would land in
        // right now) -- the actual, consuming AcceptDragDropPayload
        // further down is what performs the move on release.
        if (ImGui::AcceptDragDropPayload(kNodeMoveDragDropId, ImGuiDragDropFlags_AcceptPeekOnly)) {
            const design::DropZone zone = ComputeDropZone(ImGui::GetMousePos().y, p0, p1, is_container);
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            const ImU32 highlight = palette::U32FromHex(palette::kPrimary);
            switch (zone) {
                case design::DropZone::kInto:
                    draw_list->AddRect(p0, p1, highlight, 2.0f, 0, 3.0f);
                    break;
                case design::DropZone::kBefore:
                    draw_list->AddLine(ImVec2(p0.x, p0.y), ImVec2(p1.x, p0.y), highlight, 3.0f);
                    break;
                case design::DropZone::kAfter:
                    draw_list->AddLine(ImVec2(p0.x, p1.y), ImVec2(p1.x, p1.y), highlight, 3.0f);
                    break;
            }
        }

        if (is_container) {
            // Visual-only preview for a brand-new Toolbox component,
            // same idea as the kNodeMoveDragDropId peek above -- without
            // this, dragging e.g. "Text" in from the Toolbox gave no
            // hint at all about where it would land; a container always
            // accepts it as a new last child, so the preview is just
            // the same "kInto" highlight used for that zone above.
            if (ImGui::AcceptDragDropPayload(kToolboxDragDropId, ImGuiDragDropFlags_AcceptPeekOnly)) {
                ImGui::GetWindowDrawList()->AddRect(p0, p1, palette::U32FromHex(palette::kPrimary), 2.0f, 0, 3.0f);
            }
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kToolboxDragDropId)) {
                // Payload data is the type string including its NUL
                // terminator (see toolbox_panel.cpp's
                // SetDragDropPayload call, `info.type.size() + 1`), so
                // constructing a std::string straight from the char*
                // buffer is safe and doesn't need payload->DataSize.
                const std::string dropped_type(static_cast<const char*>(payload->Data));
                if (design::DesignNode* real = design::FindNodeByUid(doc.root, node.node_uid)) {
                    real->children.push_back(design::MakeNode(dropped_type));
                    doc.dirty = true;
                }
            }
        } else {
            // BUGFIX (reported: "after the first control lands inside a
            // row/column, dropping any more from the Toolbox stops
            // working"). Root cause: a container's own drop-catching
            // area is EITHER its header strip OR the leftover empty
            // body below its real children (see the
            // "##node_body_drop_area" InvisibleButton further down) --
            // once a container has a child, that child's OWN hit-area
            // (a leaf, is_container == false here) sits on top of and
            // usually covers most/all of the container's real-content
            // bounding box (see BoundsFromRealLeafRects above), leaving
            // little or no genuinely empty pixel left for the
            // body-filler to catch a second drop on. A leaf target used
            // to just silently ignore a kToolboxDragDropId payload
            // entirely (only kNodeMoveDragDropId, for reordering an
            // already-placed node, was accepted here) -- so hovering
            // the existing child while dragging a NEW component from
            // the Toolbox did nothing at all.
            //
            // Fix: a leaf now also accepts a fresh Toolbox drop,
            // inserting the new node as its SIBLING (before/after,
            // same top/bottom-half band as the kNodeMoveDragDropId
            // reorder case just above) instead of discarding it --
            // dropping on the top half of an existing child places the
            // new control right before it, the bottom half right after,
            // so repeated drops onto/around the same child keep growing
            // the row/column instead of stalling after one.
            if (ImGui::AcceptDragDropPayload(kToolboxDragDropId, ImGuiDragDropFlags_AcceptPeekOnly)) {
                const design::DropZone zone = ComputeDropZone(ImGui::GetMousePos().y, p0, p1, is_container);
                ImDrawList* draw_list = ImGui::GetWindowDrawList();
                const ImU32 highlight = palette::U32FromHex(palette::kPrimary);
                if (zone == design::DropZone::kAfter) {
                    draw_list->AddLine(ImVec2(p0.x, p1.y), ImVec2(p1.x, p1.y), highlight, 3.0f);
                } else {
                    draw_list->AddLine(ImVec2(p0.x, p0.y), ImVec2(p1.x, p0.y), highlight, 3.0f);
                }
            }
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kToolboxDragDropId)) {
                const std::string dropped_type(static_cast<const char*>(payload->Data));
                if (design::DesignNode* parent = design::FindParentOfUid(doc.root, node.node_uid)) {
                    std::vector<design::DesignNode>& siblings = parent->children;
                    const auto it = std::find_if(siblings.begin(), siblings.end(),
                                                  [&](const design::DesignNode& n) {
                                                      return n.node_uid == node.node_uid;
                                                  });
                    if (it != siblings.end()) {
                        const design::DropZone zone = ComputeDropZone(ImGui::GetMousePos().y, p0, p1, is_container);
                        const auto index = (it - siblings.begin()) + (zone == design::DropZone::kAfter ? 1 : 0);
                        siblings.insert(siblings.begin() + index, design::MakeNode(dropped_type));
                        doc.dirty = true;
                    }
                }
            }
        }

        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kNodeMoveDragDropId)) {
            // Same NUL-terminated-string convention as the Toolbox
            // payload above -- see the drag source in DrawNode below
            // for the matching SetDragDropPayload call.
            const std::string moved_uid(static_cast<const char*>(payload->Data));
            const design::DropZone zone = ComputeDropZone(ImGui::GetMousePos().y, p0, p1, is_container);
            // design::MoveNode does its own validation (self-drop,
            // root, stale uid, cycle) and sets doc.dirty itself on
            // success -- nothing else to do with its return value
            // here, a rejected move is simply a no-op this frame.
            design::MoveNode(doc, moved_uid, node.node_uid, zone);
        }

        ImGui::EndDragDropTarget();
    }
}

// Fase 10, Paso B (09_DESIGNER_CANVAS_UX_PLAN.md, sección "Desvío de
// diseño"): resultado confirmado -- MODE 1 fallaba justo como este
// comentario anticipaba: un ImGui::Button real cubre TODO el rect del
// nodo, así que le robaba el ActiveId/click a la capa de selección
// (InvisibleButton, más abajo) y el botón dejaba de poder seleccionarse
// con un click. Checkbox/radiobutton tienen una caja de click más chica
// que el rect completo, así que el mismo problema pasaba desapercibido
// para ellos. MODE 2 (ImGui::SetNextItemAllowOverlap() antes de dibujar
// el widget real) resuelve esto -- confirmado que existe en el ImGui
// 1.86+ vendorizado (1.92.9) -- y es el modo activo desde acá en
// adelante.
//   0 = comportamiento anterior (más seguro pero atenuado): BeginDisabled(true)
//       SIEMPRE, sin importar el prop `enabled` del nodo.
//   1 = descartado -- ver arriba, un click sobre un button real no
//       seleccionaba el nodo.
//   2 = activo. Como 1 (sin BeginDisabled(true) de seguridad, solo
//       disablea si el prop real del nodo lo pide), más
//       SetNextItemAllowOverlap() sobre el widget real antes de
//       dibujarlo, para que el InvisibleButton de selección dibujado
//       después siga recibiendo el hover/click aunque el widget real
//       ocupe el mismo rect completo.
#define AVA_FASE10_PASO_B_MODE 2

// Fase 10 (09_DESIGNER_CANVAS_UX_PLAN.md, diagnóstico punto 4 / plan de
// Fase 10): dibuja el look REAL de `node` con el widget nativo de ImGui
// que le corresponde (component_catalog.cpp: button/textbox/checkbox/
// radiobutton/text/link/divider/spacer/image), en vez del rect+label
// genérico de antes -- un button pasa a verse como un button de verdad,
// no una caja con "button (id)" adentro. Dibuja dentro de [p0, p1]
// (rect ya inseteado por margen -- ver DrawNode). Devuelve false para
// cualquier tipo sin mapeo nativo (containers -- resueltos aparte por su
// franja de header, no acá -- y cualquier leaf no listado), para que el
// caller sepa que no dibujó nada y decida un fallback.
//
// Todo el bloque corre bajo BeginDisabled(true): el plan original de
// esta fase asumía que alcanzaba con dibujar el widget real ANTES del
// InvisibleButton (Fase 7) para que el orden de inserción de ImGui le
// diera el hit-test siempre a la capa de selección de encima -- pero
// eso solo garantiza quién queda con el HOVER al final del frame, no
// evita que ButtonBehavior() le asigne el ActiveId a ESTE widget en el
// mismo frame en que el mouse baja sobre él (se procesa antes que el
// InvisibleButton, en el mismo instante). Sin poder compilar/probar
// esto en el momento, la opción segura es forzar `Disabled` acá --
// ButtonBehavior/ItemHoverable saltean por completo la lógica de
// active-id para un item disabled, así que el widget de abajo NUNCA
// puede robarle un click a la capa de selección.
//
// Fase 10, Paso A (09_DESIGNER_CANVAS_UX_PLAN.md): ese BeginDisabled(true)
// de seguridad tiene un efecto colateral no buscado -- también dispara el
// atenuado automático de ImGui (DisabledAlpha), así que un nodo con
// `enabled: true` en el catálogo se veía atenuado igual que uno con
// `enabled: false` (nunca se leía ese prop acá). Son dos cosas
// independientes: (1) input-disabled por seguridad, que SIEMPRE debe
// estar activo pase lo que pase con el prop; (2) atenuado visual, que
// SOLO debería reflejar el prop `enabled` real del nodo. El
// PushStyleVar(DisabledAlpha, 1.0f) de abajo anula el atenuado
// automático de (1) sin tocar su garantía de seguridad; el bloque
// siguiente aplica el atenuado real leyendo `enabled` del nodo (props
// sin ese key, ej. text/link/divider, caen en el fallback "true" =
// nunca atenuado, que es el comportamiento correcto para ellos).
// Fase 10 (classic widget style): renders button/textbox/checkbox/
// radiobutton with a plain classic gray look (closer to a legacy VB6
// control) instead of the IDE's own dark/orange ImGui theme, so the
// Designer Canvas previews a control close to how it will actually
// look once exported, rather than tinted by Ava Studio's own chrome.
//
// Split into two pushes because button/textbox paint their own opaque
// frame behind their label (so forcing the text to black is safe and
// reads clearly there), while checkbox/radiobutton draw their label
// directly on whatever is behind them -- here, the canvas's own dark
// background. Forcing ImGuiCol_Text to black for those would make the
// label disappear against it. PushClassicFrameStyle() covers the frame/
// border/interaction-state colors shared by all four; the caller pushes
// PushClassicTextStyle() in addition only where there is a light
// background under the text (button, textbox).
int PushClassicFrameStyle() {
    ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0xE0, 0xE0, 0xE0, 0xFF));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(0xE8, 0xE8, 0xE8, 0xFF));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(0xD0, 0xD0, 0xD0, 0xFF));
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0xE0, 0xE0, 0xE0, 0xFF));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(0xE8, 0xE8, 0xE8, 0xFF));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(0xD0, 0xD0, 0xD0, 0xFF));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0x80, 0x80, 0x80, 0xFF));
    ImGui::PushStyleColor(ImGuiCol_CheckMark, IM_COL32(0x00, 0x00, 0x00, 0xFF));
    return 8; // colors pushed above -- pass to the matching PopStyleColor
}

// Additional override on top of PushClassicFrameStyle(), only for
// widgets whose label sits on their own light frame (button, textbox).
void PushClassicTextStyle() {
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0x00, 0x00, 0x00, 0xFF));
}

// Fase 10 (image widget preview): resolves the `src` prop of an Image
// node to an actual loadable path. An absolute path (or one already
// resolvable from the process's own working directory) is used as-is;
// otherwise, when a project is open, it's resolved relative to
// `project_root` -- the same base a user picking "myimage.png" from
// their project folder would expect. Returns empty for an empty `src`
// (nothing to load, not an error).
std::string ResolveImageSrcPath(const std::string& src, const std::string& project_root) {
    if (src.empty()) return {};
    const std::filesystem::path p(src);
    if (p.is_absolute() || project_root.empty()) return src;
    return (std::filesystem::path(project_root) / p).string();
}

// One decoded texture per resolved path, kept for the life of the
// process -- an Image node's `src` rarely changes mid-session, and
// re-decoding from disk on every single frame would be wasteful. A
// failed load (missing file, unsupported/corrupt data) is cached too
// (kFailed) so a bad path only touches disk once, not every frame the
// canvas redraws.
struct ImagePreviewEntry {
    unsigned int texture_id = 0;
    int width = 0;
    int height = 0;
};

std::unordered_map<std::string, ImagePreviewEntry> g_image_preview_cache;
std::unordered_set<std::string> g_image_preview_failed;

// Returns nullptr when `resolved_path` is empty, couldn't be decoded,
// or failed on a previous attempt this session.
const ImagePreviewEntry* GetOrLoadImagePreview(const std::string& resolved_path) {
    if (resolved_path.empty()) return nullptr;
    if (g_image_preview_failed.count(resolved_path) != 0) return nullptr;

    const auto cached = g_image_preview_cache.find(resolved_path);
    if (cached != g_image_preview_cache.end()) return &cached->second;

    int width = 0, height = 0, channels = 0;
    unsigned char* pixels = stbi_load(resolved_path.c_str(), &width, &height, &channels, 4 /* force RGBA */);
    if (pixels == nullptr) {
        g_image_preview_failed.insert(resolved_path);
        return nullptr;
    }

    GLuint texture_id = 0;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    stbi_image_free(pixels);

    ImagePreviewEntry entry;
    entry.texture_id = texture_id;
    entry.width = width;
    entry.height = height;
    const auto [it, inserted] = g_image_preview_cache.emplace(resolved_path, entry);
    return &it->second;
}

bool DrawRealWidget(const design::DesignNode& node, const std::string& evaluated_display, ImVec2 p0, ImVec2 p1,
                    const std::string& project_root) {
    const ImVec2 size(std::max(p1.x - p0.x, 1.0f), std::max(p1.y - p0.y, 1.0f));
    ImGui::SetCursorScreenPos(p0);
    ImGui::PushItemWidth(size.x);

    const bool node_enabled = FindPropValue(node.properties, "enabled", "true") == "true";
    // Mismo valor por defecto que ImGuiStyleVar_DisabledAlpha en ImGui
    // (ver imgui.cpp: style.DisabledAlpha = 0.60f) -- así un nodo
    // `enabled: false` se ve exactamente tan atenuado como cualquier
    // otro widget disabled del proyecto, sea cual sea el MODE de abajo.
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, node_enabled ? 1.0f : 0.60f);

#if AVA_FASE10_PASO_B_MODE == 0
    // Modo entregado/documentado: BeginDisabled(true) de seguridad
    // SIEMPRE activo (garantiza que este widget nunca le robe el
    // ActiveId a la capa de selección), con su propio atenuado anulado
    // (Paso A) para no pisar el ImGuiStyleVar_Alpha ya empujado arriba.
    ImGui::BeginDisabled(true);
    ImGui::PushStyleVar(ImGuiStyleVar_DisabledAlpha, 1.0f);
#elif AVA_FASE10_PASO_B_MODE == 1
    // Experimento 1: SIN BeginDisabled(true) de seguridad. Solo se
    // disablea si el prop real del nodo lo pide -- eso es fidelidad
    // visual/funcional normal, no la mecánica de seguridad que se está
    // probando acá.
    if (!node_enabled) ImGui::BeginDisabled(true);
#elif AVA_FASE10_PASO_B_MODE == 2
    // Experimento 2: como 1, pero con SetNextItemAllowOverlap() sobre
    // el próximo item (el widget real que se dibuja a continuación) --
    // si esta línea no compila, el ImGui vendorizado no trae la
    // función; reportar el error tal cual, no seguir con este modo.
    ImGui::SetNextItemAllowOverlap();
    if (!node_enabled) ImGui::BeginDisabled(true);
#else
#error "AVA_FASE10_PASO_B_MODE debe ser 0, 1 o 2 -- ver comentario arriba de DrawRealWidget"
#endif

    bool handled = true;
    if (node.type == "button") {
        const int frame_colors = PushClassicFrameStyle();
        PushClassicTextStyle();
        // `borderRadius` (px) drives ImGui's per-widget FrameRounding,
        // scoped to just this Button call -- same property the real
        // avaui pipeline already honors end-to-end for Button
        // (RenderTree::BuildComponent -> RenderNode::BorderRadius() ->
        // DrawButton -> HTMLRenderer's `border-radius` / GdiRenderer's
        // RoundRect). Default 4 matches DefaultTheme::borderRadiusPx so
        // an unset button previews the same corner as the exported
        // one. A fully round/pill button is just width == height with
        // borderRadius >= height / 2 -- ImGui (like GDI's RoundRect)
        // clamps the rounding to the frame's own size, so no separate
        // "circle" shape is needed.
        const float border_radius = FindPropValueF(node.properties, "borderRadius", 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, border_radius);
        ImGui::Button(evaluated_display.c_str(), size);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(frame_colors + 1);
    } else if (node.type == "textbox") {
        // Read-only display surface -- an edit here has nowhere real to
        // go (Properties is the actual place to change `value`, see
        // properties_panel.cpp); ImGuiInputTextFlags_ReadOnly below
        // blocks editing on its own, independent of whichever
        // AVA_FASE10_PASO_B_MODE is active above.
        const int frame_colors = PushClassicFrameStyle();
        PushClassicTextStyle();
        std::string buf = evaluated_display;
        buf.resize(std::max<size_t>(buf.size() + 1, 256), '\0');
        ImGui::InputText("##textbox_preview", buf.data(), buf.size(), ImGuiInputTextFlags_ReadOnly);
        ImGui::PopStyleColor(frame_colors + 1);
    } else if (node.type == "checkbox") {
        // No PushClassicTextStyle() here -- the label is drawn straight
        // onto the canvas's dark background (no light frame behind the
        // text like button/textbox have), so forcing it to black would
        // make it unreadable. Only the check frame/mark get the classic
        // treatment.
        const int frame_colors = PushClassicFrameStyle();
        bool checked = FindPropValue(node.properties, "checked", "false") == "true";
        ImGui::Checkbox(evaluated_display.c_str(), &checked);
        ImGui::PopStyleColor(frame_colors);
    } else if (node.type == "radiobutton") {
        // Same reasoning as checkbox above -- frame only, text stays on
        // the current theme's color so the label remains legible.
        const int frame_colors = PushClassicFrameStyle();
        const bool checked = FindPropValue(node.properties, "checked", "false") == "true";
        ImGui::RadioButton(evaluated_display.c_str(), checked);
        ImGui::PopStyleColor(frame_colors);
    } else if (node.type == "text") {
        // Fase 10 (classic widget style): a label's real background is
        // whatever it's sitting on -- in a classic Win32/VB6-style app
        // that's the form's own light gray, not the IDE's dark canvas.
        // Filling the same light gray used for button/textbox above and
        // forcing the text to black previews that accurately, instead of
        // showing plain white text that only reads correctly against
        // Ava Studio's own dark theme.
        ImGui::GetWindowDrawList()->AddRectFilled(p0, p1, IM_COL32(0xE0, 0xE0, 0xE0, 0xFF));
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0x00, 0x00, 0x00, 0xFF));
        ImGui::TextUnformatted(evaluated_display.c_str());
        ImGui::PopStyleColor();
    } else if (node.type == "link") {
        ImGui::PushStyleColor(ImGuiCol_Text, palette::U32FromHex(palette::kPrimary));
        ImGui::TextUnformatted(evaluated_display.c_str());
        ImGui::PopStyleColor();
        const ImVec2 text_size = ImGui::CalcTextSize(evaluated_display.c_str());
        ImGui::GetWindowDrawList()->AddLine(ImVec2(p0.x, p0.y + text_size.y),
                                             ImVec2(p0.x + text_size.x, p0.y + text_size.y),
                                             palette::U32FromHex(palette::kPrimary), 1.0f);
    } else if (node.type == "divider") {
        ImGui::GetWindowDrawList()->AddLine(ImVec2(p0.x, p0.y + size.y * 0.5f), ImVec2(p1.x, p0.y + size.y * 0.5f),
                                             palette::U32FromHex(palette::kBorder), 1.0f);
    } else if (node.type == "spacer") {
        // Deliberadamente en blanco -- un spacer no tiene contenido
        // visible por definición; el fill/border tenue que ya dibuja el
        // caller alcanza como affordance de "esto es un nodo real,
        // seleccionable".
    } else if (node.type == "image") {
        // Fase 10 (image widget preview): actually decode and show
        // whatever `src` points at (see GetOrLoadImagePreview/
        // ResolveImageSrcPath above) instead of only ever drawing the
        // placeholder. The placeholder -- marco + diagonal cruzada, same
        // icon any editor uses for a broken/unloaded asset -- is now the
        // FALLBACK for an empty `src`, a path that doesn't resolve to a
        // real file, or image data stb_image can't decode, not the only
        // behavior.
        const std::string src = FindPropValue(node.properties, "src", "");
        const std::string resolved_path = ResolveImageSrcPath(src, project_root);
        const ImagePreviewEntry* preview = GetOrLoadImagePreview(resolved_path);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        if (preview != nullptr) {
            dl->AddImage(static_cast<ImTextureID>(preview->texture_id), p0, p1);
        } else {
            const ImU32 line = palette::U32FromHex(palette::kTextSecondary, 0.6f);
            dl->AddRect(p0, p1, line, 2.0f);
            dl->AddLine(p0, p1, line, 1.0f);
            dl->AddLine(ImVec2(p0.x, p1.y), ImVec2(p1.x, p0.y), line, 1.0f);
        }
    } else {
        handled = false;
    }

#if AVA_FASE10_PASO_B_MODE == 0
    ImGui::PopStyleVar(2); // Alpha (siempre), DisabledAlpha (LIFO)
    ImGui::EndDisabled();
#else
    ImGui::PopStyleVar(1); // Alpha (siempre)
    if (!node_enabled) ImGui::EndDisabled();
#endif
    ImGui::PopItemWidth();
    return handled;
}

// The bounding box, in the REAL avaui coordinate space, of a
// container's subtree -- i.e. what its border/chrome/hit-area SHOULD
// be if it's meant to line up with the real widgets SceneCommandWalker
// already painted this frame.
//
// VERTICAL extent comes purely from unioning LEAF entries in
// `uid_to_rect` (never a container's own entry): per
// LayoutEngineImpl::Compute, a container whose cross/main axis
// resolved to Stretch with nothing intrinsic to size against on that
// axis gets a rect that fills its entire parent slot -- e.g. a
// "column" with no explicit height, the sole child of a "page", fills
// the page's FULL remaining height. Real and correct for the running
// app, but exactly the "balloons to the parent's full remaining
// height" case the old BUGFIX comment (git blame) worked around: an
// invisible rect covering everything below/around the container's
// actual visible content, silently eating clicks meant for a sibling
// or nested node underneath. Unioning real LEAVES instead sidesteps
// that on the Y axis -- the box's height becomes the actual footprint
// of its real content.
//
// HORIZONTAL extent instead unions the container's OWN uid_to_rect
// entry (when present) together with its leaves' X range. Unlike
// height, a Row/Column's WIDTH normally comes from real horizontal
// Stretch against its parent (LayoutProperties.h's ReadAlignment
// default) -- e.g. a "row" of buttons inside a "column" really does
// span the column's full content width in the running app, not just
// the width its buttons happen to occupy. Dropping that (leaf-only
// bounds on X too, same as Y) made every container's box only as wide
// as its tightest child, at odds with both the real rendered app AND
// with "a row/column should span 100% horizontally" -- so X trusts
// the engine's own answer, Y doesn't. The union (rather than a flat
// override) is defensive: if a container's own width somehow doesn't
// fully cover its leaves' X range (e.g. horizontally-overflowing
// content), the box still never clips tighter than the real content.
//
// Returns std::nullopt for a container with no real leaf descendants
// yet (nothing painted to bound around -- caller falls back to the
// container's own uid_to_rect entry, and failing that to
// design::ComputeLayout, same as an empty container always needed).
std::optional<design::Rect> BoundsFromRealLeafRects(
        const design::DesignNode& node,
        const std::unordered_map<std::string, avalang::ui::LayoutRect>& uid_to_rect) {
    float min_x = std::numeric_limits<float>::max();
    float min_y = std::numeric_limits<float>::max();
    float max_x = std::numeric_limits<float>::lowest();
    float max_y = std::numeric_limits<float>::lowest();
    bool found = false;

    std::function<void(const design::DesignNode&)> visit = [&](const design::DesignNode& n) {
        if (n.children.empty()) {
            const auto it = uid_to_rect.find(n.node_uid);
            if (it != uid_to_rect.end()) {
                const avalang::ui::LayoutRect& lr = it->second;
                min_x = std::min(min_x, static_cast<float>(lr.x));
                min_y = std::min(min_y, static_cast<float>(lr.y));
                max_x = std::max(max_x, static_cast<float>(lr.x + lr.width));
                max_y = std::max(max_y, static_cast<float>(lr.y + lr.height));
                found = true;
            }
            return;
        }
        for (const design::DesignNode& child : n.children) visit(child);
    };
    visit(node);

    if (!found) return std::nullopt;

    const auto own_it = uid_to_rect.find(node.node_uid);
    if (own_it != uid_to_rect.end()) {
        const avalang::ui::LayoutRect& own = own_it->second;
        min_x = std::min(min_x, static_cast<float>(own.x));
        max_x = std::max(max_x, static_cast<float>(own.x + own.width));
    }

    return design::Rect{min_x, min_y, max_x - min_x, max_y - min_y};
}

// Recursively draws one node's rect (using the already-computed
// layout) plus its children, updates `out_selected` on click, and
// wires up HandleDropTarget for containers. `origin` is the canvas's
// screen-space top-left, since layout_engine's Rects are relative to
// whatever `available_space` ComputeLayout was called with (see
// layout_engine.h).
//
// `node`/`layout` describe the (possibly fully-resolved) tree being
// drawn this frame -- see DrawDesignerCanvas for how/when that's a
// throwaway copy of doc.root with every Componente() call already
// expanded in place, vs. doc.root directly when there's nothing to
// resolve. Either way this function itself doesn't care: it just walks
// whatever tree it's given and uses `real_uids` to tell which nodes are
// real (editable, droppable) vs. part of an imported component's own
// file (`real_uids.empty()` means "nothing was resolved this frame,
// treat every node as real" -- see CollectUids's caller).
// A Dialog is design-time non-visual: it never sits "in place" in the
// canvas the way a Button or Column does (see the comment where this is
// used, in DrawNode's children loop, and DrawDialogTray below). `node.type`
// holds the as-written AvaLang type name (lowercase, matches DrawRealWidget's
// checks above), not the CanonicalTypeName() the real avaui pipeline uses.
bool IsDialogNode(const design::DesignNode& node) { return node.type == "dialog"; }

void DrawNode(design::DesignNode& node, const design::LayoutResult& layout, ImVec2 origin,
              design::DesignDocument& doc, const std::unordered_set<std::string>& real_uids,
              std::optional<PropertiesState>& out_selected, int tab_id,
              std::string* out_generated_handler, AvaVM* state_vm,
              std::unordered_map<std::string, std::string>* eval_cache,
              const std::string& project_root, bool live_render_painted,
              const std::unordered_map<std::string, avalang::ui::LayoutRect>* uid_to_rect = nullptr,
              float extra_offset_y = 0.0f, int depth = 0) {
    // Moved up from further below (was right before the fill/border
    // draw) -- the rect-resolution fix right after this needs to know
    // is_container BEFORE picking a rect source, see that comment.
    const design::ComponentTypeInfo* info = design::FindComponentType(node.type);
    const bool is_container = info != nullptr && info->is_container;

    // Fase 4.3 (AVAUI_DESIGNER_REAL_RENDER_PLAN.md): `uid_to_rect` --
    // live_render.uidToRect, from the SAME real avaui LayoutEngine run
    // that SceneCommandWalker::Walk already painted from in 4.2 -- is
    // the primary source of a LEAF's rect, so its overlay/hit-area
    // lines up with the real widget underneath instead of disagreeing
    // with it (the known, documented gap 4.2 left open).
    //
    // (4.3) Leaves use uid_to_rect directly: a leaf's real widget rect
    // IS what should be clickable/selectable, no separate box to keep
    // in sync.
    //
    // Containers used to skip uid_to_rect entirely (see git blame for
    // the original "BUGFIX after 4.3 shipped" note) because avaui's
    // LayoutEngine sizes an unconstrained container to FILL its parent
    // -- using that raw rect for a container's hit-area made it
    // balloon to the parent's full remaining height, silently eating
    // clicks meant for a sibling/nested node underneath. The fix back
    // then was to fall back to design::ComputeLayout (Studio's own,
    // separately-implemented layout engine) for every container. That
    // traded one bug for another: ComputeLayout doesn't know this
    // component's real padding/spacing/text-measured intrinsic sizes,
    // so its container rects routinely disagreed with where the real
    // children actually got painted -- controls appearing offset from
    // or spilling outside their own container's border.
    //
    // BoundsFromRealLeafRects (below) fixes both at once: a container's
    // box is now the bounding box of its own real LEAF descendants'
    // uid_to_rect entries -- same coordinate system leaves already
    // draw in, so it can't disagree with them, and it auto-fits to
    // content instead of ballooning (since it's built from leaves, not
    // the container's own possibly-Stretched rect). `layout`
    // (design::ComputeLayout) remains only the last-resort fallback
    // for what neither uid_to_rect source covers: an empty container
    // with no real leaves yet and no rect of its own, the tab_id < 0
    // uncached path (4.1 never builds a live_render there), and a
    // live_render build that failed this frame -- same "still show
    // SOMETHING" spirit as DrawRealWidget's fallback role in 4.2.
    design::Rect r{};
    bool have_rect = false;
    if (uid_to_rect != nullptr && !is_container) {
        const auto lr_it = uid_to_rect->find(node.node_uid);
        if (lr_it != uid_to_rect->end()) {
            r = design::Rect{static_cast<float>(lr_it->second.x), static_cast<float>(lr_it->second.y),
                              static_cast<float>(lr_it->second.width), static_cast<float>(lr_it->second.height)};
            have_rect = true;
        }
    }
    // Container rect, see BoundsFromRealLeafRects and the comment
    // block above for the full reasoning.
    if (!have_rect && uid_to_rect != nullptr && is_container) {
        if (const auto bounds = BoundsFromRealLeafRects(node, *uid_to_rect)) {
            r = *bounds;
            have_rect = true;
        } else {
            // Empty container (nothing real painted under it yet) --
            // still needs a visible/droppable box, so fall back to its
            // own real rect if avaui produced one for it.
            const auto own_it = uid_to_rect->find(node.node_uid);
            if (own_it != uid_to_rect->end()) {
                r = design::Rect{static_cast<float>(own_it->second.x), static_cast<float>(own_it->second.y),
                                  static_cast<float>(own_it->second.width), static_cast<float>(own_it->second.height)};
                have_rect = true;
            }
        }
        if (have_rect) {
            // See kRealContainerPadTop/kRealContainerPadSide's comment:
            // pad this real-content rect outward so the corner chip and
            // border below have room to sit without cutting into the
            // real children this box wraps.
            r.x -= kRealContainerPadSide;
            r.y -= kRealContainerPadTop;
            r.w += kRealContainerPadSide * 2.0f;
            r.h += kRealContainerPadTop + kRealContainerPadSide;
        }
    }
    if (!have_rect) {
        // Last-resort fallback: no live_render this frame (tab_id < 0
        // uncached path, or a build that failed -- see uid_to_rect's
        // header comment), so Studio's own approximate layout is all
        // there is to draw from.
        const auto it = layout.rects.find(node.node_uid);
        if (it == layout.rects.end()) return; // shouldn't happen -- ComputeLayout visits every node
        r = it->second;
    }

    // Fase 7: `extra_offset_y` accumulates one kHeaderHeight per
    // CONTAINER ancestor between this node and the canvas root (see the
    // recursive call at the bottom of this function) -- it's how a
    // container's header strip carves out real screen space for itself
    // without layout_engine.cpp knowing anything about headers (its
    // Rects stay full-bleed/edge-to-edge exactly as before; this just
    // shifts where DrawNode PAINTS them). A node's own rect is shifted
    // by its ANCESTORS' headers only, never its own -- its own header
    // (if it's a container) only affects where ITS children get drawn,
    // added when recursing below.
    const ImVec2 base_p0 = ToImVec2(r, origin);
    const ImVec2 raw_p0 = ImVec2(base_p0.x, base_p0.y + extra_offset_y);
    const ImVec2 raw_p1 = ImVec2(raw_p0.x + r.w, raw_p0.y + r.h);
    // Inset on every side, clamped so a rect smaller than 2*margin
    // (shouldn't happen at kDefaultLeafHeight's scale, but cheap to
    // guard) never flips p0/p1. Fase 10 (diagnóstico punto 4): margin
    // scales with `depth` instead of staying fixed at kNodeMargin, so
    // deeply-nested children keep a legible gap from their ancestors
    // (see kNodeMarginPerDepth/kNodeMarginMax above) instead of the
    // separation collapsing after 3-4 levels.
    const float margin = std::min(kNodeMargin + static_cast<float>(depth) * kNodeMarginPerDepth, kNodeMarginMax);
    const ImVec2 p0(raw_p0.x + margin, raw_p0.y + margin);
    const ImVec2 p1(std::max(p0.x, raw_p1.x - margin), std::max(p0.y, raw_p1.y - margin));

    const bool selected = (node.node_uid == doc.selected_uid);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // synthetic = "this node came from an imported component's own
    // file, not doc.root" -- see real_uids's header comment above.
    const bool synthetic = !real_uids.empty() && real_uids.find(node.node_uid) == real_uids.end();

    // Fase 10: pushed here (rather than right before the InvisibleButton
    // further down, where this used to live) so it also scopes the real
    // ImGui widgets DrawRealWidget submits below -- without this, two
    // sibling buttons with the same generated label (e.g. both showing
    // "Button") would collide on the same ImGui ID. Popped once, at the
    // very end of this function's own scope (see the matching PopID
    // near the bottom, unchanged in position).
    ImGui::PushID(node.node_uid.c_str());

    // Fill so empty containers (no visible children yet) are still
    // clickable/droppable, not just an invisible zero-content area --
    // slightly tinted for containers vs leaves so nesting reads at a
    // glance in the wireframe. (`info`/`is_container` computed at the
    // top of this function now, see the rect-resolution bugfix there.)
    const ImU32 fill = is_container ? palette::U32FromHex(palette::kSurface, 0.6f)
                                     : palette::U32FromHex(palette::kCard, 0.9f);
    // BUGFIX (reported: container chrome -- border, corner chip -- as
    // visually loud as an actual control, when it should read as
    // secondary/structural). A container's unselected border now gets
    // its own, more transparent color instead of sharing `kBorder` at
    // full opacity with leaf wireframes -- see kSelectionPad/the
    // is_container branch below for the matching change to a
    // SELECTED container's outline.
    const ImU32 border = selected ? palette::U32FromHex(palette::kPrimary)
                                   : (is_container ? palette::U32FromHex(palette::kBorder, 0.45f)
                                                    : palette::U32FromHex(palette::kBorder));

    // Fase 4.2 (AVAUI_DESIGNER_REAL_RENDER_PLAN.md): once the real
    // avaui pipeline painted this frame (SceneCommandWalker::Walk, see
    // DrawDesignerCanvas), a LEAF's own visual comes from that real
    // widget, painted UNDER this overlay pass -- the old opaque
    // wireframe fill+border would just cover it, so it's skipped for
    // leaves in that case. Containers keep their border/header
    // regardless (see skip_body_fill just below for why the FILL part
    // is different) -- same reasoning `is_container` already used
    // above. When `live_render_painted` is false (build failed, or the
    // tab_id < 0 uncached path -- see cache_entry_for_live_render's
    // comment), every node keeps the old wireframe so the canvas still
    // shows something instead of blank leaves.
    const bool skip_leaf_wireframe = live_render_painted && !is_container;
    // BUGFIX (reported: container chrome painting over its own real
    // children): once live_render_painted, a container's children were
    // already painted for real by SceneCommandWalker::Walk BEFORE this
    // overlay pass runs (see DrawDesignerCanvas). The container's own
    // AddRectFilled body-fill below used to still be drawn unconditionally
    // for every container (only leaves were exempted, via
    // skip_leaf_wireframe above) -- painted using Studio's OWN layout
    // (design::ComputeLayout), which sizes an unconstrained container/root
    // to fill its full available rect. That opaque/semi-opaque fill then
    // sat ON TOP of the real widgets underneath (e.g. a `row` of buttons
    // inside a `column` inside `page`), washing them out or hiding them
    // entirely -- worst for the root `page`, whose fill always spans the
    // entire canvas. The border+header strip are just outlines/thin bands
    // and don't obscure content, so those stay for every container
    // regardless; only the full-body fill is skipped once real pixels are
    // already there underneath it.
    const bool skip_body_fill = live_render_painted && is_container;
    if (!skip_leaf_wireframe) {
        if (!skip_body_fill) {
            draw_list->AddRectFilled(p0, p1, fill, 2.0f);
        }
        // The plain (unselected-style) border only -- once selected,
        // the padded selection frame drawn below replaces it, so we
        // don't end up with two borders (a tight one here plus a
        // padded one further out) fighting for attention.
        if (!selected) {
            draw_list->AddRect(p0, p1, border, 2.0f, 0, 1.0f);
        }
    }

    // Selection chrome, VB6-style: small solid square handles directly
    // on a frame drawn AROUND the control -- 4 corners + 4 edge
    // midpoints, 8 total -- the classic "selected control" look from
    // VB6/Delphi-era visual editors.
    //
    // BUGFIX (reported: frame reads as glued to/inside the control,
    // confusable with its own text at small sizes): this used to draw
    // directly on p0/p1 -- for a real-rendered leaf, that's the
    // control's true rect shrunk inward by kNodeMargin (meant to
    // separate wireframe siblings, not to place a selection outline),
    // so the frame sat a few px INSIDE the real button instead of
    // around it. Fix: compute a dedicated selection rect from the
    // control's actual (unshrunk) bounds -- raw_p0/raw_p1 for a
    // real-rendered leaf, since that's its true painted edge, or the
    // already-legible p0/p1 wireframe rect otherwise -- then pad it
    // outward by kSelectionPad so the frame always sits clearly
    // OUTSIDE the control with a visible gap, never touching it.
    if (selected) {
        const ImVec2 base_sel_p0 = skip_leaf_wireframe ? raw_p0 : p0;
        const ImVec2 base_sel_p1 = skip_leaf_wireframe ? raw_p1 : p1;
        const ImVec2 sel_p0(base_sel_p0.x - kSelectionPad, base_sel_p0.y - kSelectionPad);
        const ImVec2 sel_p1(base_sel_p1.x + kSelectionPad, base_sel_p1.y + kSelectionPad);

        // BUGFIX (reported: a selected CONTAINER reads as loud/heavy as
        // a selected CONTROL -- same thick frame, same 8 resize-looking
        // handles -- when it's really just "this is the active node in
        // the tree", not "here's an editable widget"). A container was
        // already never resizable this way (see the `!is_container`
        // guard further down, unchanged), so drawing the full handle
        // set on one was actively misleading on top of being visually
        // loud. Containers now get a plain, lower-alpha, no-handle
        // outline instead -- still clearly "this one's selected", just
        // subordinate to however a selected CONTROL looks.
        if (is_container) {
            const ImU32 container_selected_border = palette::U32FromHex(palette::kPrimary, 0.5f);
            draw_list->AddRect(sel_p0, sel_p1, container_selected_border, 3.0f, 0, 1.5f);
        } else {
            draw_list->AddRect(sel_p0, sel_p1, border, 2.0f, 0, 2.0f);
    
            constexpr float kHandle = 6.0f;
            constexpr float kHandleHalf = kHandle * 0.5f;
            const float mid_x = (sel_p0.x + sel_p1.x) * 0.5f;
            const float mid_y = (sel_p0.y + sel_p1.y) * 0.5f;
            // Skip the edge-midpoint handles (keep only the 4 corners) once
            // the node is too small for 3 handles to fit along a side
            // without overlapping -- same idea VB6 itself used for tiny
            // controls, rather than letting handles collide into a blob.
            const bool wide_enough = (sel_p1.x - sel_p0.x) >= kHandle * 3.0f;
            const bool tall_enough = (sel_p1.y - sel_p0.y) >= kHandle * 3.0f;
            std::vector<ImVec2> handles = {sel_p0, sel_p1, ImVec2(sel_p1.x, sel_p0.y), ImVec2(sel_p0.x, sel_p1.y)};
            if (wide_enough) {
                handles.push_back(ImVec2(mid_x, sel_p0.y));
                handles.push_back(ImVec2(mid_x, sel_p1.y));
            }
            if (tall_enough) {
                handles.push_back(ImVec2(sel_p0.x, mid_y));
                handles.push_back(ImVec2(sel_p1.x, mid_y));
            }
            const ImU32 handle_fill = palette::U32FromHex(palette::kBackground);
            const ImU32 handle_border = palette::U32FromHex(palette::kPrimary);
            for (const ImVec2& c : handles) {
                const ImVec2 hp0(c.x - kHandleHalf, c.y - kHandleHalf);
                const ImVec2 hp1(c.x + kHandleHalf, c.y + kHandleHalf);
                draw_list->AddRectFilled(hp0, hp1, handle_fill);
                draw_list->AddRect(hp0, hp1, handle_border, 0.0f, 0, 1.0f);
            }
    
            // "poder redimencionar un control": the bottom-right corner and
            // the right/bottom edge-midpoint handles double as real
            // drag-to-resize controls -- corner adjusts width+height
            // together, the two edges adjust just one dimension, same
            // split VB6 itself used. Only for a real, editable leaf: a
            // container auto-fits to its children (an explicit size here
            // would just be silently ignored/fought over by that), and a
            // synthetic node (an imported component's own file, see
            // `synthetic` above) has no DesignNode in this doc to write
            // into. Top/left handles stay purely visual, same as before --
            // this is a flow layout (each node's top-left corner comes
            // from its parent/siblings, not from the node itself), so
            // there's no "anchor" a top/left drag could resize away from
            // without also repositioning the control, which is a
            // meaningfully bigger feature than resize.
            if (!synthetic) {
                const auto ResizeHandle = [&](const char* str_id, ImVec2 center, ImGuiMouseCursor cursor, bool adjust_x,
                                              bool adjust_y) {
                    // Hit-zone a couple px larger than the drawn glyph --
                    // easier to land the drag on without the cursor
                    // needing pixel-perfect precision.
                    constexpr float kHitHalf = kHandleHalf + 3.0f;
                    ImGui::SetCursorScreenPos(ImVec2(center.x - kHitHalf, center.y - kHitHalf));
                    ImGui::InvisibleButton(str_id, ImVec2(kHitHalf * 2.0f, kHitHalf * 2.0f));
                    if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
                        ImGui::SetMouseCursor(cursor);
                    }
                    if (ImGui::IsItemActivated()) {
                        // Baseline for this drag: whatever explicit
                        // width/height the node already has, or (nothing
                        // set yet) its current on-screen size -- so the
                        // control doesn't jump to some default the instant
                        // the drag starts, only what the user drags it to.
                        float start_w = base_sel_p1.x - base_sel_p0.x;
                        float start_h = base_sel_p1.y - base_sel_p0.y;
                        TryGetNumericProperty(node.properties, "width", &start_w);
                        TryGetNumericProperty(node.properties, "height", &start_h);
                        g_resize_drag = ResizeDragState{true, node.node_uid, adjust_x, adjust_y, start_w, start_h};
                    }
                    if (ImGui::IsItemActive() && g_resize_drag.active && g_resize_drag.node_uid == node.node_uid) {
                        const ImVec2 total_delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0f);
                        if (adjust_x) {
                            SetSizeProperty(node.properties, "width",
                                            std::max(kMinResizeDimension, g_resize_drag.start_width + total_delta.x));
                        }
                        if (adjust_y) {
                            SetSizeProperty(node.properties, "height",
                                            std::max(kMinResizeDimension, g_resize_drag.start_height + total_delta.y));
                        }
                        doc.dirty = true;
                    }
                    if (ImGui::IsItemDeactivated() && g_resize_drag.node_uid == node.node_uid) {
                        g_resize_drag.active = false;
                    }
                };
                ResizeHandle("##resize_se", sel_p1, ImGuiMouseCursor_ResizeNWSE, true, true);
                ResizeHandle("##resize_e", ImVec2(sel_p1.x, mid_y), ImGuiMouseCursor_ResizeEW, true, false);
                ResizeHandle("##resize_s", ImVec2(mid_x, sel_p1.y), ImGuiMouseCursor_ResizeNS, false, true);
            }
        }
    }



    // BUGFIX (reported: header band overlapping real controls, and only
    // containers being selectable -- never buttons/leaves inside them):
    // the Fase 7 header strip below reserves kHeaderHeight px of layout
    // space per container ANCESTOR (via child_offset_y at the bottom of
    // this function) and shifts every descendant down by that much for
    // BOTH drawing and hit-testing. That's self-consistent in the old
    // wireframe-only world, where a container's own rect (design::
    // ComputeLayout) and its children's rects come from that exact same
    // offset-aware coordinate space.
    //
    // But once live_render_painted is true, a LEAF's rect instead comes
    // from uid_to_rect -- real, absolute coordinates from avaui's own
    // layout engine, which reserves NO header gap at all (Studio's
    // header strip doesn't exist in a real running app). Piling the
    // Fase 7 header offset on top of those already-correct real
    // coordinates pushes each leaf further and further from where it
    // was actually painted (one kHeaderHeight per container ancestor,
    // compounding with nesting depth) -- which is exactly what the
    // screenshot showed: the header band visually overlapping the real
    // content above it, AND every leaf's invisible-button hit area
    // drifting down into empty space below where it's actually drawn,
    // so clicks over a real button/text never landed on that leaf's
    // hit area at all -- only a container's own (undrifted, still
    // full-width) header band happened to catch them, making
    // containers look like the only selectable thing on the canvas.
    //
    // Fix: once real content is painted underneath, a container's own
    // "always clickable, never covered by a child" affordance becomes a
    // small corner CHIP instead of a full-width space-reserving band --
    // it draws on top of a small top-left corner of the container
    // (same idea as the selection tag above, just always-on rather than
    // selected-only), and nothing below this function reserves any
    // space for it. Children keep their real, undrifted positions.
    const bool header_reserves_space = is_container && !live_render_painted;
    const float header_bottom = header_reserves_space ? std::min(p1.y, p0.y + kHeaderHeight) : p0.y;
    ImVec2 chip_p0{}, chip_p1{};
    if (is_container) {
        if (header_reserves_space) {
            draw_list->AddRectFilled(p0, ImVec2(p1.x, header_bottom), palette::U32FromHex(palette::kBorder, 0.55f),
                                      2.0f, ImDrawFlags_RoundCornersTop);
            draw_list->AddLine(ImVec2(p0.x, header_bottom), ImVec2(p1.x, header_bottom),
                                palette::U32FromHex(palette::kBorder), 1.0f);
        } else {
            const ImVec2 chip_text_size = ImGui::CalcTextSize(node.type.c_str());
            const float chip_w = std::min(chip_text_size.x + kChipPadX * 2.0f, std::max(p1.x - p0.x, 1.0f));
            chip_p0 = p0;
            chip_p1 = ImVec2(p0.x + chip_w, std::min(p1.y, p0.y + kChipHeight));
            draw_list->AddRectFilled(chip_p0, chip_p1, palette::U32FromHex(palette::kBorder, 0.45f), 2.0f,
                                      ImDrawFlags_RoundCornersBottomRight);
            draw_list->AddText(ImVec2(chip_p0.x + kChipPadX, chip_p0.y + kChipPadY),
                                palette::U32FromHex(palette::kTextDisabled), node.type.c_str());
        }
    }

    // Empty-state hint: a blank container (no children yet) used to be
    // an unlabeled void with no clue it was even droppable -- now that
    // its whole body accepts drops (see the body-drop-area fix below),
    // it gets a small centered "+" and a short label so a new/empty
    // page or container doesn't just look broken. Skipped for synthetic
    // (resolved-import) nodes -- those can't accept drops at all (see
    // HandleDropTarget's `synthetic` guard), so the hint would be a lie.
    if (is_container && !synthetic && node.children.empty() && header_bottom < p1.y - 4.0f) {
        const ImVec2 body_center((p0.x + p1.x) * 0.5f, (header_bottom + p1.y) * 0.5f);
        const ImU32 hint_color = palette::U32FromHex(palette::kTextDisabled);
        const float half = 6.0f;
        // Only draw the "+" (and its label) when there's room for it --
        // a very short/collapsed container just gets the header, same
        // as before.
        if (p1.y - header_bottom > half * 4.0f) {
            draw_list->AddLine(ImVec2(body_center.x - half, body_center.y - 12.0f),
                                ImVec2(body_center.x + half, body_center.y - 12.0f), hint_color, 1.5f);
            draw_list->AddLine(ImVec2(body_center.x, body_center.y - 12.0f - half),
                                ImVec2(body_center.x, body_center.y - 12.0f + half), hint_color, 1.5f);
            const char* hint_text = "Arrastrá un control acá";
            const ImVec2 text_size = ImGui::CalcTextSize(hint_text);
            draw_list->AddText(ImVec2(body_center.x - text_size.x * 0.5f, body_center.y), hint_color, hint_text);
        }
    }

    std::string label = node.type;
    if (!node.id.empty()) label += " (" + node.id + ")";
    // Synthetic (resolved-component) nodes get a small marker so it's
    // visually obvious this subtree came from an import, not from
    // doc.root directly -- otherwise a resolved Navbar() would look
    // indistinguishable from a hand-placed one, which matters here
    // since (unlike the real tree) it silently discards drops.
    if (synthetic) label += " [import]";
    // Containers no longer draw their type/id label into the header
    // strip -- only real components (leaves) identify themselves (via
    // their actual rendered widget, e.g. a button's text), containers
    // stay unlabeled, just the header strip itself as a plain
    // selection/drag affordance. `label` (with node.id) is still
    // computed above and used below as the drag-payload tooltip and as
    // the plain-text fallback for any leaf type DrawRealWidget doesn't
    // recognize.

    // Fase 6 (08_DESIGNER_VIEW_PLAN.md, "state sin evaluar/bindear
    // contra la VM" -- ver design/state_eval.h): un segundo renglón
    // debajo del label de type/id con el valor EVALUADO de la
    // display-prop del tipo (GetDisplayPropertyKey -- "value" para
    // text/textbox, "text" para button/link, "label" para checkbox/
    // radiobutton; "" para containers y demás, que no dibujan nada
    // acá). `state_vm` es la misma VM para todo el árbol este frame
    // (ver DrawDesignerCanvas) con doc.initial_state ya bindeado como
    // globals -- así una expresión como `"Contador: " + counter` (si
    // algún día se escribe a mano en el .avaui) se muestra evaluada en
    // vez del texto fuente crudo; un literal simple como "Guardar" cae
    // de vuelta a mostrarse tal cual (ver EvalPropertyExpr). Nodos
    // sintéticos (resueltos de un import) también lo hacen -- no hay
    // motivo para negarles esto, a diferencia de edición/drag&drop que
    // sí lo necesitan (no hay un DesignNode real ahí para escribir).
    const std::string display_key = design::GetDisplayPropertyKey(node.type);
    std::string evaluated_display;
    if (!display_key.empty()) {
        for (const PropertyRow& prop : node.properties) {
            if (prop.key == display_key) {
                // Caching pass (see DesignerVmCacheEntry::eval_cache
                // above): compile+run only on a miss (first time this
                // exact node_uid+raw-value pair is drawn since the last
                // VM rebuild); every other frame just looks the string
                // back up. `eval_cache` is null for the tab_id < 0
                // uncached path (see DrawDesignerCanvas), which falls
                // straight back to evaluating every frame like before
                // this pass.
                if (eval_cache != nullptr) {
                    const std::string cache_key = node.node_uid + '\x1f' + prop.value;
                    auto cached = eval_cache->find(cache_key);
                    if (cached != eval_cache->end()) {
                        evaluated_display = cached->second;
                    } else {
                        evaluated_display = design::EvalPropertyExpr(state_vm, prop.value);
                        (*eval_cache)[cache_key] = evaluated_display;
                    }
                } else {
                    evaluated_display = design::EvalPropertyExpr(state_vm, prop.value);
                }
                break;
            }
        }
    }

    // Fase 10 (09_DESIGNER_CANVAS_UX_PLAN.md): leaves render their real
    // ImGui widget look (DrawRealWidget above), filling almost their
    // WHOLE rect (just a couple px of padding, no reserved label line --
    // see the comment above on why), using the SAME evaluated
    // display-prop value Fase 6 already computed above -- a live state
    // edit still shows up in the real control the very next frame, no
    // extra plumbing. Containers are untouched here (`is_container`):
    // their own affordance is the header strip, not a widget.
    // Fase 4.2: when the real avaui pipeline already painted this frame
    // (`live_render_painted`), a leaf's actual look comes from that --
    // SceneCommandWalker::Walk ran once for the whole tree in
    // DrawDesignerCanvas, BEFORE this overlay recursion, so its output
    // is already on the draw list under everything drawn from here on.
    // DrawRealWidget (the old per-node ImGui-widget approximation) is
    // now only the FALLBACK for when there's no live_render to paint
    // from (build failed, or the tab_id < 0 uncached path -- see
    // DrawDesignerCanvas). Known gap either way until Fase 5
    // (AVAUI_DESIGNER_REAL_RENDER_PLAN.md): `evaluated_display` (the
    // state-bound value Fase 6 computed above) is NOT what the real
    // pipeline shows today -- BuildLiveRender copies `node.properties`
    // literally, it doesn't resolve `state` bindings yet, so a node
    // like `text = counter` shows the literal string "counter" via the
    // Walk, same documented gap as InferValue's "no evalúa
    // expresiones".
    bool widget_drawn = live_render_painted && !is_container;
    if (!widget_drawn && !is_container) {
        const ImVec2 widget_p0(p0.x + 2.0f, p0.y + 2.0f);
        const ImVec2 widget_p1(std::max(widget_p0.x, p1.x - 2.0f), std::max(widget_p0.y, p1.y - 2.0f));
        widget_drawn = DrawRealWidget(node, evaluated_display, widget_p0, widget_p1, project_root);
    }
    // Fallback for any leaf type without a native mapping in
    // DrawRealWidget (shouldn't happen against today's
    // component_catalog.cpp, but a future/unknown type -- e.g. loaded
    // from a newer .avaui -- degrades to the old two-line plain text
    // (label, then evaluated value) rather than silently showing
    // nothing -- there's no widget here to carry the label at all in
    // this path, unlike the normal case above.
    if (!widget_drawn) {
        draw_list->AddText(ImVec2(p0.x + 4.0f, p0.y + 4.0f), palette::U32FromHex(palette::kTextPrimary),
                            label.c_str());
        if (!evaluated_display.empty()) {
            draw_list->AddText(ImVec2(p0.x + 4.0f, p0.y + 20.0f),
                                palette::U32FromHex(palette::kTextSecondary), evaluated_display.c_str());
        }
    }

    // Invisible button over the rect gives us click + drag-drop-target
    // hit-testing without fighting ImGui's normal widget/window input
    // routing (a raw draw_list rect has no input of its own).
    //
    // Fase 7 fix (09_DESIGNER_CANVAS_UX_PLAN.md diagnóstico punto 1):
    // before this pass, a container's hit area was its FULL rect, and
    // children were drawn (and hit-tested) on top of it in the same
    // pass -- since ImGui resolves overlapping items to whichever was
    // added last, and children are always added after their parent,
    // ANY click inside a container with at least one child landed on
    // that child, never on the container itself -- there was no pixel
    // of the container left to claim it. The fix: a container's hit
    // area is exclusive screen space no child ever occupies -- the
    // header strip when it reserves layout space (`header_reserves_
    // space`, the non-live-rendered wireframe fallback), or just the
    // small corner chip when it doesn't (see the chip/header-space
    // split above) -- so a container's own affordance never competes
    // with a child's hit area for the same pixels the way the
    // pre-Fase-7 full-rect InvisibleButton did. A leaf keeps its full
    // rect as its hit area (unchanged) -- it has no children to
    // compete with in the first place.
    const ImVec2 hit_p1 = !is_container ? p1 : (header_reserves_space ? ImVec2(p1.x, header_bottom) : chip_p1);
    ImGui::SetCursorScreenPos(p0);
    ImGui::InvisibleButton("##node_hit_area",
                            ImVec2(std::max(hit_p1.x - p0.x, 1.0f), std::max(hit_p1.y - p0.y, 1.0f)));
    // Affordance: hovering ANY selectable/draggable node's hit-area
    // (a container's header, or a leaf's full rect) shows the same
    // cursor a drag handle would. Fase 10 widens this from
    // container-only (Fase 7) to every real node -- a leaf is just as
    // selectable/draggable as a container, it just didn't have this
    // hint before.
    const bool node_hovered = ImGui::IsItemHovered();
    if (!synthetic && node_hovered) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        // Beyond the cursor swap, a visible highlight on the hit area
        // itself -- easy to miss a cursor-only cue, especially on a
        // container's thin header strip. Drawn now (not back when the
        // fill/border were painted above) because it depends on this
        // frame's hover result, which only exists once the item itself
        // has been submitted.
        if (!selected) {
            draw_list->AddRect(p0, hit_p1, palette::U32FromHex(palette::kPrimary, 0.7f), 2.0f, 0, 1.5f);
        }
    }
    if (ImGui::IsItemClicked()) {
        doc.selected_uid = node.node_uid;
        out_selected = ToPropertiesState(node, /*editable=*/!synthetic, tab_id);

        // Anexo 9.17/9.18: corre el handler `click` del nodo (si tiene
        // uno real, ver design_document.cpp's "click" event key) contra
        // `state_vm` -- la misma VM cacheada por tab_id que ya tiene
        // `state` + code_behind bindeados (ver DrawDesignerCanvas). Se
        // dispara con Ctrl+Click, sin cambios respecto a Anexo 9.17/9.18
        // (el modo Preview dedicado que antes también disparaba esto con
        // un click normal fue removido, Fase 10.1 -- ver
        // designer_canvas.h). `synthetic` excluido por el mismo motivo
        // que double-click/drag: no hay VM/estado propio para un nodo de
        // un import resuelto acá (su `state`/`methods` ni se cargan hoy,
        // ver 9.14 limitación 2).
        const bool should_invoke_click = !synthetic && state_vm && ImGui::GetIO().KeyCtrl;
        if (should_invoke_click) {
            for (const PropertyRow& ev : node.events) {
                if (ev.key == "click" && !ev.value.empty()) {
                    std::string handler_error;
                    const bool ok = design::InvokeHandler(state_vm, ev.value, &handler_error);
                    if (ok && eval_cache != nullptr) {
                        // El handler puede haber mutado `state`
                        // (globals de la vm) -- el eval_cache de
                        // display-props está keyeado por node_uid +
                        // raw_value, no por valor de state, así que un
                        // hit viejo seguiría devolviendo el string
                        // evaluado ANTES del click. Limpiar todo el
                        // cache (no solo las entries de este nodo) es
                        // la forma simple y correcta: cualquier prop
                        // en el árbol pudo depender del mismo global
                        // que el handler tocó.
                        eval_cache->clear();
                        // Fase 4.4 (AVAUI_DESIGNER_REAL_RENDER_PLAN.md):
                        // verified -- doesn't ALSO need to invalidate
                        // cache_entry_for_live_render's `live_render`
                        // here. BuildLiveRender (Fase 2) copies
                        // node.properties literally into the
                        // ComponentTree; it doesn't bind `state` at
                        // all yet (same gap InferValue already had for
                        // display-prop expressions), so nothing a
                        // handler mutates in `state_vm`'s globals can
                        // change what live_render would produce even on
                        // a rebuild. Revisit together with Fase 5
                        // ("State bindings en el preview real") when
                        // BuildLiveRender actually reads `state` --
                        // that's the point this eval_cache->clear()
                        // needs a live_render invalidation right next
                        // to it, not before.
                    }
                    // Anexo 9.18 limitación 2 ("un error de runtime en el
                    // handler no tiene dónde mostrarse") -- sigue sin
                    // resolver tras Fase 10.1: la consola de Preview que
                    // mostraba esto fue removida junto con el resto del
                    // modo (ver designer_canvas.h); `handler_error` queda
                    // sin usar acá a propósito, no silenciado por
                    // descuido -- si vuelve a hacer falta un lugar donde
                    // mostrarlo, se retoma aparte (no bloqueante).
                    (void)handler_error;
                    break;
                }
            }
        }
    }

    // Fase 5 (08_DESIGNER_VIEW_PLAN.md section 6): double-click a real
    // Button to generate (or reuse) its "click" handler stub, VS6-
    // style. Scoped to "button" specifically -- the same scope
    // AGENTS_STUDIO.md's "Workflow Futuro" and the plan doc describe;
    // other interactive types (checkbox/textbox/...) don't have an
    // obvious single default event the way a button's click does, so
    // they're left for a future pass instead of guessing one here. A
    // synthetic (resolved-import) node is excluded for the same reason
    // Toolbox drops/moves are: there's no real DesignNode in doc.root
    // to attach the event to. ImGui's own idiom for "double-click this
    // item" -- IsItemHovered() + IsMouseDoubleClicked() checked right
    // after the item, same InvisibleButton IsItemClicked() above
    // already used for selection (a double-click's second click still
    // fires that too, so the node ends up selected AND jumped-to).
    if (!synthetic && node.type == "button" && ImGui::IsItemHovered() &&
        ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        const std::string handler = design::EnsureClickHandler(doc, node.node_uid);
        if (!handler.empty() && out_generated_handler) {
            *out_generated_handler = handler;
        }
    }

    // Fase 4: drag this already-placed node to move/reorder it.
    // Attached to the SAME InvisibleButton used for click/select above
    // -- ImGui's normal pattern for "this item is also a drag source",
    // a plain click (no drag) still selects as before. Root (the page)
    // can't be dragged -- it has no parent to remove it from, matching
    // design::MoveNode's own refusal to move doc.root.node_uid -- and
    // a synthetic (resolved-import) node can't be dragged either,
    // same restriction as Toolbox drops below, since there's no real
    // DesignNode in doc.root for it to correspond to.
    const bool movable = !synthetic && node.node_uid != doc.root.node_uid;
    if (movable && ImGui::BeginDragDropSource()) {
        ImGui::SetDragDropPayload(kNodeMoveDragDropId, node.node_uid.c_str(), node.node_uid.size() + 1);
        ImGui::TextUnformatted(label.c_str());
        ImGui::EndDragDropSource();
    }

    // Fase 8 (09_DESIGNER_CANVAS_UX_PLAN.md): right-click context menu
    // with "Delete" on this node's own hit-area (the header for a
    // container, thanks to Fase 7 -- previously there was no stable,
    // exclusive area to hang a context menu off of for a container
    // with children). Same restriction as dragging: root can't be
    // deleted (no parent to splice it out of) and a synthetic
    // (resolved-import) node isn't a real DesignNode in doc.root to
    // delete. BeginPopupContextItem() with no id attaches to the last
    // item, i.e. the InvisibleButton above -- opened by
    // ImGuiPopupFlags_MouseButtonRight (the default), so this doesn't
    // interfere with the plain-click-to-select handling above it.
    // Actual deletion goes through a confirmation popup (same
    // Yes/Cancel pattern as explorer_panel.cpp's DrawDeleteConfirmPopup)
    // drawn once at the end of DrawDesignerCanvas -- see
    // g_canvas_delete_request / DrawCanvasDeleteConfirmPopup below.
    if (movable && ImGui::BeginPopupContextItem("##node_context_menu")) {
        if (ImGui::MenuItem("Delete")) {
            g_canvas_delete_request = {true, tab_id, node.node_uid};
        }
        ImGui::EndPopup();
    }

    // Resolved-component subtrees aren't part of doc.root -- nothing
    // to append a Toolbox drop to (or move payload to reparent into),
    // so skip the drop target entirely rather than silently accepting
    // a drop that vanishes next frame (see designer_canvas.h's header
    // comment on this being read-only/display-only for now).
    if (!synthetic) {
        // p0/hit_p1 -- the exact same rect just given to InvisibleButton
        // above, so the drop-zone preview highlight (drawn inside
        // HandleDropTarget) always matches the real, clickable hit
        // area: the header strip for a container, the full rect for a
        // leaf.
        HandleDropTarget(node, doc, is_container, p0, hit_p1);
    }

    // Fix: a container's OWN rect can be much taller than the sum of
    // its children's stacked heights -- "page" in particular always
    // gets the full remaining canvas height (see layout_engine.cpp's
    // ComputeLayout), while its children only take up their own
    // natural height. Before this block, the only drop target inside a
    // container was its header strip (hit_p1 above) plus whatever
    // rects its children individually claim -- any leftover empty
    // space below the last child (or the entire body of an empty
    // container) had NO drop target at all, so a component could only
    // be dropped by aiming exactly at that thin header strip, never
    // anywhere else in what visually reads as "inside the page". This
    // second InvisibleButton covers that leftover body (from the
    // bottom of the header down to the container's real bottom edge)
    // and accepts the same drops as the header (always "become the
    // last child" -- see HandleDropTarget/ComputeDropZone's
    // is_container case). Added BEFORE the children recurse below, so
    // -- same z-order rule as every other hit-area in this file -- a
    // child's own InvisibleButton (added after, over its own rect)
    // still wins hover wherever a child actually sits; this filler
    // only ever catches drops in genuinely empty space.
    if (is_container && !synthetic && header_bottom < p1.y) {
        ImGui::SetCursorScreenPos(ImVec2(p0.x, header_bottom));
        // Real bug fix: this body-filler button is submitted BEFORE its
        // children recurse below, so without this call it's the first
        // item to claim ImGui's HoveredId over any pixel a child later
        // occupies -- and ImGui's overlap rule is "first claim wins,
        // blocking every later item at the same spot, unless the first
        // one opts out." That's exactly why a click on e.g. a nested
        // `Text` always ended up selecting the parent `page` instead:
        // this button always got there first and never let go.
        // SetNextItemAllowOverlap() (the current, non-obsolete API --
        // SetItemAllowOverlap() called AFTER the item never reliably
        // worked for this, see github.com/ocornut/imgui/issues/6512)
        // marks the item about to be submitted (this InvisibleButton)
        // as one a later-drawn, overlapping item is allowed to steal
        // hover/click away from -- so each real child's own
        // "##node_hit_area" (added afterwards, during the recursive
        // DrawNode calls below) correctly wins over this filler
        // wherever the two overlap, and only genuinely empty space (no
        // child underneath) is left for this button to actually catch.
        ImGui::SetNextItemAllowOverlap();
        ImGui::InvisibleButton("##node_body_drop_area",
                                ImVec2(std::max(p1.x - p0.x, 1.0f), std::max(p1.y - header_bottom, 1.0f)));
        // Same hover cue as the header/leaf hit area above (drawn here
        // rather than earlier for the same reason: needs this frame's
        // hover result) -- only while something is actually being
        // dragged, so idle mouse movement over a page's mostly-empty
        // body doesn't light up for no reason the way a real click
        // target would.
        if (ImGui::IsItemHovered()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            if (ImGui::GetDragDropPayload() != nullptr) {
                draw_list->AddRectFilled(ImVec2(p0.x, header_bottom), p1, palette::U32FromHex(palette::kPrimary, 0.06f));
            }
        }
        // Selecting the container by clicking its empty body (not just
        // its header strip) -- without this, clicking anywhere in a
        // mostly-empty container (e.g. a blank page) did nothing at
        // all, since this whole area used to only be a drop target.
        // Same selection side effect as the header's own
        // IsItemClicked() above, minus the Ctrl+Click handler-invoke
        // path (a container's own "click" handler, if it ever has one,
        // already fires from the header click -- no need to duplicate
        // that here too).
        if (ImGui::IsItemClicked()) {
            doc.selected_uid = node.node_uid;
            out_selected = ToPropertiesState(node, /*editable=*/true, tab_id);
        }
        HandleDropTarget(node, doc, is_container, ImVec2(p0.x, header_bottom), p1);
    }

    ImGui::PopID();

    // Fase 7: children of a container start below its header strip --
    // `child_offset_y` is `extra_offset_y` (this node's own accumulated
    // shift from ITS ancestors) plus kHeaderHeight when THIS node is a
    // container whose header actually reserves layout space (its
    // header claims that space from its children, not from itself --
    // see the header comment on `extra_offset_y` at the top of this
    // function). Once live_render_painted, `header_reserves_space` is
    // false (see the chip/header-space split above), so this adds
    // nothing -- children's real (uid_to_rect) coordinates already
    // don't leave any such gap, and offsetting them here would just
    // drift them away from where they were actually painted. A leaf
    // has no children to recurse into, so this is a no-op for it
    // either way.
    const float child_offset_y = extra_offset_y + (header_reserves_space ? kHeaderHeight : 0.0f);
    for (design::DesignNode& child : node.children) {
        // A Dialog is a non-visual-at-design-time component: LayoutEngine
        // (LayoutEngineImpl.cpp's IsDialog special case) always centers
        // it on the whole ROOT viewport regardless of isOpen, because
        // that's the correct place for it once it's actually open at
        // runtime. Drawing it inline here with that same rect would mean
        // every Dialog in the tree -- open or (far more often) closed --
        // renders as a big box centered over the entire canvas, on top of
        // everything else. Dialogs are pulled into DrawDialogTray's
        // separate strip below the canvas instead; see IsDialogNode.
        if (IsDialogNode(child)) continue;
        DrawNode(child, layout, origin, doc, real_uids, out_selected, tab_id, out_generated_handler, state_vm,
                 eval_cache, project_root, live_render_painted, uid_to_rect, child_offset_y, depth + 1);
    }
}

// Fase 7: one entry per ancestor from `root` down to (and including)
// the node with node_uid == `target_uid`, in that order -- root first.
// `root` here is whatever tree is actually being drawn this frame
// (`root_to_draw` in DrawDesignerCanvas: doc.root itself, or the
// throwaway fully-resolved copy when there are imports -- see that
// function's own comment), so this also works when the selection
// landed inside a resolved/synthetic subtree; `real_uids` (same set
// DrawNode already uses) is what tells each segment apart as
// real/editable vs. synthetic when a breadcrumb segment is clicked.
// Returns an empty vector if `target_uid` isn't found anywhere under
// `root` (e.g. nothing selected, or a stale uid from a just-deleted
// node) -- callers treat that as "don't draw a breadcrumb this frame".
struct BreadcrumbSegment {
    std::string label;
    std::string node_uid;
};

bool CollectBreadcrumbPath(design::DesignNode& node, const std::string& target_uid,
                            std::vector<BreadcrumbSegment>& out) {
    std::string label = node.type;
    if (!node.id.empty()) label += " (" + node.id + ")";
    out.push_back({label, node.node_uid});
    if (node.node_uid == target_uid) return true;
    for (design::DesignNode& child : node.children) {
        if (CollectBreadcrumbPath(child, target_uid, out)) return true;
    }
    out.pop_back();
    return false;
}

// Fase 7 ("breadcrumb de jerarquía"): draws "Page > column > row >
// button" as a thin row of clickable segments above the canvas, so a
// child can jump straight to any ancestor (including a container
// that -- pre Fase 7 -- had no free pixel of its own to click in the
// canvas) without depending on finding it in the tree by hand. Returns
// the height it actually used (0 when there's nothing selected / the
// selection isn't found in `root_to_draw` this frame, e.g. right after
// a delete) so the caller can size the canvas child below it
// accordingly. Updates `doc.selected_uid` and `*out_selected` in place
// when a segment is clicked -- same shape/values DrawNode's own click
// handling produces, so main.cpp's Properties write-back sees no
// difference between the two.
float DrawBreadcrumbBar(design::DesignNode& root_to_draw, design::DesignDocument& doc,
                         const std::unordered_set<std::string>& real_uids, int tab_id,
                         std::optional<PropertiesState>& out_selected) {
    if (doc.selected_uid.empty()) return 0.0f;
    std::vector<BreadcrumbSegment> path;
    if (!CollectBreadcrumbPath(root_to_draw, doc.selected_uid, path)) return 0.0f;

    for (size_t i = 0; i < path.size(); ++i) {
        if (i > 0) {
            ImGui::SameLine(0.0f, 4.0f);
            ImGui::TextDisabled(">");
            ImGui::SameLine(0.0f, 4.0f);
        }
        ImGui::PushID(static_cast<int>(i));
        const bool is_last = (i + 1 == path.size());
        // The current selection itself is shown disabled (nothing to
        // jump TO from itself) -- every ancestor above it is a real
        // button.
        ImGui::BeginDisabled(is_last);
        if (ImGui::SmallButton(path[i].label.c_str())) {
            doc.selected_uid = path[i].node_uid;
            if (design::DesignNode* real = design::FindNodeByUid(doc.root, path[i].node_uid)) {
                const bool synthetic = !real_uids.empty() && real_uids.find(path[i].node_uid) == real_uids.end();
                out_selected = ToPropertiesState(*real, /*editable=*/!synthetic, tab_id);
            }
        }
        ImGui::EndDisabled();
        ImGui::PopID();
    }
    return ImGui::GetFrameHeightWithSpacing();
}

// Dialogos ocultos (fix): every Dialog anywhere in `node`'s subtree,
// however deep -- collected as a flat list (label + node_uid) rather
// than raw DesignNode pointers so it stays valid even if the caller
// mutates the tree between collecting and drawing (same "look it up
// again by uid on click" caution DrawBreadcrumbBar already uses).
// Recurses into every node (dialog or not) so a Dialog nested inside a
// Container, and even a Dialog nested inside another Dialog, both
// still show up in the tray.
struct DialogTrayEntry {
    std::string label;
    std::string node_uid;
};

void CollectDialogNodes(design::DesignNode& node, std::vector<DialogTrayEntry>& out) {
    for (design::DesignNode& child : node.children) {
        if (IsDialogNode(child)) {
            std::string label = child.id.empty() ? child.type : (child.type + " (" + child.id + ")");
            out.push_back({label, child.node_uid});
        }
        CollectDialogNodes(child, out);
    }
}

// Dialogos ocultos (fix), Fase 1: a Dialog never sits "in place" in the
// canvas the way a visible control does -- LayoutEngine always centers
// it on the whole page viewport regardless of isOpen (see IsDialogNode's
// comment), so there's no single sensible spot to draw it inline. This
// draws a VB6/WinForms-style "non-visual components" tray instead: one
// clickable chip per Dialog in the tree, below the canvas, completely
// out of the layout flow. Clicking a chip selects that Dialog (feeds
// Properties, same shape as a normal canvas click). Opening a Dialog's
// own content in an isolated full-canvas edit mode (double-click a
// chip -> breadcrumb into it, like `Componente()` imports already let
// you inspect a resolved subtree) is the planned Fase 2 -- not in this
// pass. Returns the height used, 0 when the tree has no Dialog at all
// (same zero-reservation convention as DrawBreadcrumbBar).
float DrawDialogTray(design::DesignNode& root_to_draw, design::DesignDocument& doc, int tab_id,
                      std::optional<PropertiesState>& out_selected) {
    std::vector<DialogTrayEntry> dialogs;
    CollectDialogNodes(root_to_draw, dialogs);
    if (dialogs.empty()) return 0.0f;

    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("Dialogs (non-visual):");
    for (size_t i = 0; i < dialogs.size(); ++i) {
        ImGui::SameLine();
        ImGui::PushID(static_cast<int>(i));
        const bool is_selected = (doc.selected_uid == dialogs[i].node_uid);
        if (is_selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        }
        if (ImGui::SmallButton(dialogs[i].label.c_str())) {
            doc.selected_uid = dialogs[i].node_uid;
            if (design::DesignNode* real = design::FindNodeByUid(doc.root, dialogs[i].node_uid)) {
                out_selected = ToPropertiesState(*real, /*editable=*/true, tab_id);
            }
        }
        if (is_selected) {
            ImGui::PopStyleColor();
        }
        ImGui::PopID();
    }
    return ImGui::GetFrameHeightWithSpacing();
}

// Fase 8: confirmation popup for deleting a canvas node (Yes/Cancel,
// same pattern as explorer_panel.cpp's DrawDeleteConfirmPopup for
// files). Drawn once per DrawDesignerCanvas call, but only actually
// opens/acts when `g_canvas_delete_request.tab_id == tab_id` -- i.e.
// only the tab whose node/context-menu/Delete-key actually raised the
// request services it, even though every open .avaui tab's canvas
// runs through this same function every frame.
void DrawCanvasDeleteConfirmPopup(design::DesignDocument& doc, int tab_id,
                                   std::optional<PropertiesState>& out_selected) {
    if (g_canvas_delete_request.tab_id != tab_id) return;
    if (g_canvas_delete_request.open) {
        ImGui::OpenPopup("Delete node?");
        g_canvas_delete_request.open = false;
    }
    ImGui::SetNextWindowSize(ImVec2(360.0f, 0.0f));
    if (ImGui::BeginPopupModal("Delete node?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        design::DesignNode* node = design::FindNodeByUid(doc.root, g_canvas_delete_request.node_uid);
        std::string label = "this node";
        if (node) {
            label = node->type;
            if (!node->id.empty()) label += " (" + node->id + ")";
        }
        ImGui::TextWrapped("Delete \"%s\" and everything inside it?", label.c_str());
        ImGui::TextDisabled("This can't be undone.");
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 8.0f));

        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float button_w = (ImGui::GetContentRegionAvail().x - spacing) / 2.0f;

        ImGui::PushStyleColor(ImGuiCol_Button, palette::FromHex(palette::kError, 0.75f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, palette::FromHex(palette::kError, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, palette::FromHex(0xc93b3b));
        if (ImGui::Button("Delete", ImVec2(button_w, 0.0f))) {
            if (design::RemoveNode(doc, g_canvas_delete_request.node_uid)) {
                // The deleted node (or an ancestor of it) may be the
                // one currently shown in Properties -- RemoveNode
                // itself already clears doc.selected_uid in that case
                // (see design_document.cpp), but `out_selected` is a
                // separate copy already returned to the caller earlier
                // this frame (or a previous one) and needs clearing
                // too, or Properties would keep showing a node that no
                // longer exists.
                if (out_selected && (out_selected->selected_node_uid == g_canvas_delete_request.node_uid ||
                                      doc.selected_uid.empty())) {
                    out_selected.reset();
                }
            }
            g_canvas_delete_request = {};
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(3);
        ImGui::SameLine(0.0f, spacing);

        if (ImGui::Button("Cancel", ImVec2(button_w, 0.0f))) {
            g_canvas_delete_request = {};
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

} // namespace

std::optional<PropertiesState> DrawDesignerCanvas(design::DesignDocument& doc, ImVec2 size,
                                                   const std::string& project_root, int tab_id,
                                                   std::string* out_generated_handler) {
    std::optional<PropertiesState> selected;
    if (out_generated_handler) out_generated_handler->clear();

    // Fase 6 (design/state_eval.h) + 9.8 punto 3, unified caching pass
    // (9.15/9.16 -- see DesignerVmCacheEntry above and
    // designer_canvas.h's `tab_id` note): for `tab_id >= 0`, everything
    // that depends on doc's current tree/state (the AvaVM, its
    // eval_cache, the ComponentResolver, and the resolved-tree deep
    // copy) is rebuilt TOGETHER, only when needed, instead of every
    // single frame. `tab_id < 0` (no real caller today besides
    // editor_panel.cpp) keeps the old always-rebuild-every-call
    // behavior for all of it, since there's no safe per-caller key to
    // cache under in that case.
    AvaVM* state_vm = nullptr;
    std::unordered_map<std::string, std::string>* eval_cache = nullptr;
    design::DesignNode* root_to_draw = &doc.root;
    // Fase 4.1: only set for tab_id >= 0 -- the live_render rebuild
    // below needs the canvas viewport size, which isn't known until
    // after BeginChild/GetContentRegionAvail further down, so it can't
    // happen inside the tab_id>=0 block above like the VM/resolver do.
    DesignerVmCacheEntry* cache_entry_for_live_render = nullptr;
    bool tree_state_rebuilt_this_frame = false;
    std::unordered_set<std::string> local_real_uids; // only used on the tab_id < 0 path
    const std::unordered_set<std::string>* real_uids = &local_real_uids;
    // Only engaged on the tab_id < 0 path -- see the comment above
    // `resolver` reads it below in that branch.
    std::optional<design::ComponentResolver> local_resolver;
    std::optional<design::DesignNode> local_resolved_root; // ditto, tab_id < 0 only

    if (tab_id >= 0) {
        DesignerVmCacheEntry& entry = g_designer_vm_cache[tab_id];
        const bool needs_rebuild =
            entry.vm == nullptr || entry.last_dirty != doc.dirty || entry.cached_project_root != project_root;
        tree_state_rebuilt_this_frame = needs_rebuild;
        if (needs_rebuild) {
            if (entry.vm) ava_vm_destroy(entry.vm);
            entry.vm = design::BuildStateVM(doc);
            // Anexo 9.17/9.18: registra doc.code_behind's `func ... end`
            // stubs como globals invocables en la MISMA vm que ya tiene
            // `state` bindeado -- ver state_eval.h::BindCodeBehind. Debe
            // ir DESPUÉS de BuildStateVM (necesita la vm ya creada) y
            // bajo la misma condición de rebuild que el resto de esta
            // entry, no en cada click -- ver InvokeHandler más abajo
            // para el lado que sí corre por click.
            design::BindCodeBehind(entry.vm, doc);
            // Fase 10.1: la vm ya no tiene ningun sink de Preview para
            // re-bindear aca -- ver el comentario donde vivian los
            // trampolines, arriba de g_designer_vm_cache.
            entry.last_dirty = doc.dirty;
            entry.eval_cache.clear();

            entry.resolver.reset();
            entry.resolved_root.reset();
            entry.real_uids.clear();
            entry.cached_project_root = project_root;
            if (!project_root.empty()) {
                entry.resolver.emplace(project_root);
                entry.resolver->ResolveImports(doc);
                // Resolve the WHOLE tree once per rebuild -- a full
                // copy of doc.root with every Componente() call already
                // replaced by its real subtree (see the (now stale,
                // pre-caching) comment this replaced for why the whole
                // tree is resolved upfront rather than per-call-site).
                // Skipped when there's nothing to resolve (no imports),
                // same as before this pass -- `real_uids` stays empty,
                // which DrawNode treats as "every node is real".
                if (!doc.imports.empty()) {
                    CollectUids(doc.root, entry.real_uids);
                    entry.resolved_root = doc.root; // deep copy
                    entry.resolver->ResolveTree(*entry.resolved_root);
                }
            }
        }
        state_vm = entry.vm;
        eval_cache = &entry.eval_cache;
        root_to_draw = entry.resolved_root ? &*entry.resolved_root : &doc.root;
        real_uids = &entry.real_uids;
        cache_entry_for_live_render = &entry;
    } else {
        if (!project_root.empty()) {
            local_resolver.emplace(project_root);
            local_resolver->ResolveImports(doc);
        }
        if (local_resolver && !doc.imports.empty()) {
            CollectUids(doc.root, local_real_uids);
            local_resolved_root = doc.root; // deep copy
            local_resolver->ResolveTree(*local_resolved_root);
            root_to_draw = &*local_resolved_root;
        }
        state_vm = design::BuildStateVM(doc);
        design::BindCodeBehind(state_vm, doc);
    }

    // Fase 7: breadcrumb bar ("Page > column > row > button") drawn
    // ABOVE the canvas child, not inside its scrolling area -- it
    // needs to stay visible/pinned regardless of scroll position, and
    // it's cheap: 0px (no reservation at all) whenever nothing is
    // selected -- "don't reserve space you're not using".
    const float breadcrumb_height = DrawBreadcrumbBar(*root_to_draw, doc, *real_uids, tab_id, selected);
    // Dialogos ocultos (fix): drawn right after the breadcrumb, above the
    // scrolling canvas child -- a fixed strip, not part of the
    // scrollable design surface, same reasoning as the breadcrumb bar
    // itself (see DrawDialogTray's own comment for why Dialogs live
    // here instead of inline in the canvas).
    const float dialog_tray_height = DrawDialogTray(*root_to_draw, doc, tab_id, selected);
    ImVec2 canvas_size = size;
    const float reserved_height = breadcrumb_height + dialog_tray_height;
    if (reserved_height > 0.0f) {
        canvas_size.y = std::max(size.y - reserved_height, 1.0f);
    }

    ImGui::BeginChild("##DesignerCanvas", canvas_size, true, ImGuiWindowFlags_HorizontalScrollbar);

    // Fase 8: Delete/Supr removes doc.selected_uid, but only while this
    // canvas child actually has focus -- otherwise pressing Delete
    // while e.g. typing in Properties or the Code Editor would also
    // try to delete whatever's selected on the canvas underneath. Same
    // "gate hotkeys on window focus" pattern as explorer_panel.cpp's F2/
    // Del handling. Root can't be deleted (design::RemoveNode already
    // refuses it, but checked here too so this doesn't even open a
    // confirmation popup for a no-op), and a synthetic selection
    // (`real_uids` non-empty and not containing selected_uid) isn't a
    // real DesignNode in doc.root to delete either -- same restriction
    // as the context menu above.
    if (ImGui::IsWindowFocused() && !doc.selected_uid.empty() && doc.selected_uid != doc.root.node_uid &&
        (real_uids->empty() || real_uids->find(doc.selected_uid) != real_uids->end()) &&
        ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
        g_canvas_delete_request = {true, tab_id, doc.selected_uid};
    }

    // Fase 10.1: el borde/banner naranja "PREVIEW -- los clicks ejecutan
    // handlers reales" que vivia aca fue removido junto con el resto del
    // modo Preview -- ver designer_canvas.h. Ya no hace falta distinguir
    // visualmente "modo Preview" de edicion normal porque los widgets
    // reales de Fase 10 se ven asi siempre, no solo bajo un toggle.

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const design::Rect canvas_rect{0.0f, 0.0f, std::max(avail.x, 1.0f), std::max(avail.y, 1.0f)};

    if (cache_entry_for_live_render != nullptr) {
        DesignerVmCacheEntry& entry = *cache_entry_for_live_render;
        const int vw = static_cast<int>(canvas_rect.w);
        const int vh = static_cast<int>(canvas_rect.h);
        const bool live_render_stale = !entry.live_render.ok || entry.live_render_w != vw ||
                                        entry.live_render_h != vh || tree_state_rebuilt_this_frame;
        if (live_render_stale) {
            entry.live_render = studio::design::BuildLiveRender(*root_to_draw, vw, vh);
            entry.live_render_w = vw;
            entry.live_render_h = vh;
            entry.imgui_renderer = std::make_unique<avalang::ui::ImGuiRenderer>(vw, vh);
        }
    }

    // Fase 4.2: paint the REAL widgets for the whole tree in one pass,
    // using the real avaui pipeline's own SceneGraph -- BEFORE DrawNode
    // below, which from here on only draws the selection/hover/drop-
    // zone overlay on top (see DrawNode's `live_render_painted`
    // handling). Fase 4.3: DrawNode's overlay rects now come from this
    // SAME live_render (uidToRect below), not design::ComputeLayout, so
    // the overlay lines up with what was just painted here -- the
    // temporary mismatch 4.2 documented is resolved as of 4.3.
    bool live_render_painted = false;
    if (cache_entry_for_live_render != nullptr && cache_entry_for_live_render->live_render.ok &&
        cache_entry_for_live_render->live_render.sceneGraph && cache_entry_for_live_render->imgui_renderer) {
        DesignerVmCacheEntry& entry = *cache_entry_for_live_render;
        entry.imgui_renderer->SetTarget(ImGui::GetWindowDrawList(), origin);
        avalang::ui::RenderCommandSink sink;
        avalang::ui::SceneCommandWalker::Walk(*entry.live_render.sceneGraph, sink, *entry.imgui_renderer);
        live_render_painted = true;
    }

    const design::LayoutResult layout = design::ComputeLayout(*root_to_draw, canvas_rect);
    const std::unordered_map<std::string, avalang::ui::LayoutRect>* uid_to_rect =
        live_render_painted ? &cache_entry_for_live_render->live_render.uidToRect : nullptr;
    DrawNode(*root_to_draw, layout, origin, doc, *real_uids, selected, tab_id, out_generated_handler, state_vm,
             eval_cache, project_root, live_render_painted, uid_to_rect);

    // Keep Properties showing the CURRENTLY selected node's live data
    // on every call, not just the one frame a click happened. Before
    // this, `selected` only got a value from DrawNode's own
    // IsItemClicked() handling above -- every OTHER frame (a plain
    // re-render, or switching back to this tab after a different node
    // was selected elsewhere) left it at nullopt, which the caller
    // (editor_panel.cpp) treats as "clear the Properties panel"... but
    // doc.selected_uid itself is sticky (it lives on the document, see
    // design_document.h), so the canvas kept showing this node as
    // selected (the halo/border) while Properties silently blanked or
    // -- worse -- kept showing whatever a DIFFERENT tab last had
    // selected, since main.cpp's own `properties_state` only updates
    // when it's handed a fresh value. Recomputing from
    // `doc.selected_uid` here, unconditionally, every call, makes the
    // two consistent: whatever the canvas visually shows as selected
    // is exactly what Properties reflects and what any edit/add-
    // property from that panel targets (PropertiesState::source_tab_id
    // + selected_node_uid, unchanged mechanism -- see
    // properties_panel.h). A uid that no longer resolves (its node was
    // deleted out from under it) clears the selection instead of
    // leaving a dangling one.
    if (!doc.selected_uid.empty()) {
        if (design::DesignNode* found = design::FindNodeByUid(*root_to_draw, doc.selected_uid)) {
            const bool found_synthetic = !real_uids->empty() && real_uids->find(doc.selected_uid) == real_uids->end();
            selected = ToPropertiesState(*found, /*editable=*/!found_synthetic, tab_id);
        } else {
            doc.selected_uid.clear();
            selected.reset();
        }
    } else {
        selected.reset();
    }

    // Root itself (the page) also accepts drops -- e.g. the very first
    // control on a blank page, before any container has been placed
    // inside it. DrawNode already ran HandleDropTarget for the root as
    // part of the recursion above, so nothing else is needed here;
    // this comment just documents that the empty-canvas case is
    // covered, not skipped.

    ImGui::EndChild();

    // Fase 8: drawn once per call, outside the scrolling child (a
    // modal popup doesn't need to live inside it) -- see
    // DrawCanvasDeleteConfirmPopup's own comment on why it's safe to
    // call this unconditionally for every open tab's canvas.
    DrawCanvasDeleteConfirmPopup(doc, tab_id, selected);

    // Only destroy the VM here for the uncached (`tab_id < 0`) path --
    // a cached one (tab_id >= 0) is owned by g_designer_vm_cache now
    // and outlives this call on purpose; it's freed by
    // InvalidateDesignerVmCache when the tab actually closes instead.
    if (tab_id < 0 && state_vm) ava_vm_destroy(state_vm);

    return selected;
}

void InvalidateDesignerVmCache(int tab_id) {
    auto it = g_designer_vm_cache.find(tab_id);
    if (it == g_designer_vm_cache.end()) return;
    if (it->second.vm) ava_vm_destroy(it->second.vm);
    g_designer_vm_cache.erase(it);
}

// Fase 10.1: SetDesignerPreviewActive/IsDesignerPreviewActive/
// ResetDesignerPreviewState/GetDesignerPreviewLog/ClearDesignerPreviewLog
// vivian aca -- removidas junto con el resto del modo Preview (Anexo
// 9.19), ver designer_canvas.h para el porque.

} // namespace studio