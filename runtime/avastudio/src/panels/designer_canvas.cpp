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

#include "animation/AnimationController.h"
#include "design/component_catalog.h"
#include "design/component_resolver.h"
#include "design/imgui_renderer.h"
#include "design/live_render_bridge.h"
#include "design/state_eval.h"
#include "commands/RenderCommandSink.h"
#include "commands/SceneCommandWalker.h"
#include "components/IComponent.h"
#include "designer/command.h"
#include "designer/document_commands.h"
#include "designer/drop_target.h"
#include "designer/guides.h"
#include "designer/hit_test.h"
#include "designer/layout_core.h"
#include "designer/overlay.h"
#include "designer/preview.h"
#include "designer/responsive.h"
#include "designer/property_editor.h"
#include "designer/property_grid.h"
#include "designer/selection_manager.h"
#include "designer/tools.h"
#include "designer/viewport.h"
#include "designer/visual_states.h"
#include "layout/LayoutProperties.h"
#include "theme/ProjectStyleOverrides.h"
#include "events/AutoBind.h"
#include "layout/LayoutEngine.h"
#include "imgui.h"
#include "palette.h"
#include "panels/toolbox_panel.h"
#include "shortcuts/shortcut_registry.h"
#include "util/i18n.h"

#include "GLFW/glfw3.h"
#include "stb_image.h"

namespace studio {

namespace {

std::string TrFormat(const std::string& key, std::initializer_list<std::string> args) {
    std::string result = util::Tr(key);
    for (const std::string& arg : args) {
        const size_t pos = result.find("%s");
        if (pos == std::string::npos) break;
        result = result.substr(0, pos) + arg + result.substr(pos + 2);
    }
    return result;
}

std::string TrFormat(const std::string& key, const std::string& arg) { return TrFormat(key, {arg}); }

std::string LowerAscii(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

std::string PropertyValueToDisplayString(const designer::PropertyValue& v) {
    switch (v.Type()) {
        case designer::PropertyType::Bool:
            return v.AsBool() ? "true" : "false";
        case designer::PropertyType::Number: {
            double n = v.AsNumber();
            if (n == static_cast<long long>(n)) return std::to_string(static_cast<long long>(n));
            return std::to_string(n);
        }
        case designer::PropertyType::String:
            return v.AsString();
        default:
            return "";
    }
}

std::string GetNodeIdProp(avalang::ui::IComponent* node) {
    const auto* idProp = node->GetProperty("id");
    if (idProp && idProp->Type() == designer::PropertyType::String) return idProp->AsString();
    return "";
}

void CollectPropertyRows(avalang::ui::IComponent* node, std::vector<PropertyRow>* out_properties,
                          std::vector<PropertyRow>* out_events) {
    for (const auto& name : node->PropertyNames()) {
        if (name == "id") continue;
        const auto* v = node->GetProperty(name);
        if (!v) continue;
        PropertyRow row{name, PropertyValueToDisplayString(*v)};
        if (avalang::ui::IsEventPropertyName(name)) {
            if (out_events) out_events->push_back(row);
        } else {
            if (out_properties) out_properties->push_back(row);
        }
    }
}

// Set when a click handler ran successfully (so it may have changed state).
// DrawDesignerCanvas consumes it after drawing the nodes and marks the live
// render dirty, so text bound to state (`"status: " + count`) is re-evaluated
// and repainted on the next frame instead of staying frozen.
bool g_state_changed_by_handler = false;

void InvokeNodeClickHandler(AvaVM* state_vm, avalang::ui::IComponent* clicked, avalang::ui::IComponent* node,
                             const std::vector<PropertyRow>& node_events,
                             std::unordered_map<std::string, std::string>* eval_cache) {
    if (!state_vm) return;

    std::vector<PropertyRow> picked_events;
    if (clicked != node) {
        CollectPropertyRows(clicked, nullptr, &picked_events);
    }
    const std::vector<PropertyRow>& click_events = (clicked != node) ? picked_events : node_events;
    for (const PropertyRow& ev : click_events) {
        if (ev.key == "click" && !ev.value.empty()) {
            std::string handler_error;
            const bool ok = design::InvokeHandler(state_vm, ev.value, &handler_error);
            if (ok) {
                g_state_changed_by_handler = true;
                if (eval_cache != nullptr) {
                    eval_cache->clear();
                }
            }
            (void)handler_error;
            break;
        }
    }
}

struct NodeDecoration {
    designer::LayoutRect rect;
    bool compact = false;
};

struct DesignerVmCacheEntry {
    AvaVM* vm = nullptr;
    bool last_dirty = false;
    int last_revision = -1;
    std::unordered_map<std::string, std::string> eval_cache;
    std::string cached_project_root;
    avalang::ui::theme::ProjectStyleSheet project_styles;

    studio::design::LiveRenderResult live_render;
    std::unique_ptr<avalang::ui::ImGuiRenderer> imgui_renderer;
    std::unique_ptr<avalang::ui::animation::AnimationController> animation_controller;
    std::unique_ptr<avalang::ui::ComponentTree> preview_resolved_tree;
    int live_render_w = -1;
    int live_render_h = -1;
    designer::CanvasMode live_render_mode = designer::CanvasMode::Design;
    bool live_render_dirty = false;

    int device_preset_index = 0;

    std::string last_logged_live_render_error;
    std::string last_logged_missing_rects;
    std::string last_logged_overlay_gap;
    std::string last_logged_click_reroute;
    std::string surface_hovered_node_id;
    std::unordered_map<std::string, NodeDecoration> decorations;

    designer::CommandManager command_manager;
    designer::SelectionManager selection_manager;
    designer::LayoutCore surface_layout;
    designer::DesignerViewport viewport;
    designer::CanvasMode canvas_mode = designer::CanvasMode::Design;
    designer::DesignerVisualState visual_state = designer::DesignerVisualState::Default;
};

const std::pair<designer::DesignerVisualState, const char*> kVisualStateOptions[] = {
    {designer::DesignerVisualState::Default, "canvas.visual_state.default"},
    {designer::DesignerVisualState::Hover, "canvas.visual_state.hover"},
    {designer::DesignerVisualState::Focus, "canvas.visual_state.focus"},
    {designer::DesignerVisualState::Active, "canvas.visual_state.active"},
    {designer::DesignerVisualState::Disabled, "canvas.visual_state.disabled"},
};

void DrawVisualStateCombo(DesignerVmCacheEntry* cache_entry) {
    if (cache_entry == nullptr) return;

    const auto current = std::find_if(
        std::begin(kVisualStateOptions), std::end(kVisualStateOptions),
        [&](const std::pair<designer::DesignerVisualState, const char*>& option) {
            return option.first == cache_entry->visual_state;
        });
    const std::string current_label =
        util::Tr(current != std::end(kVisualStateOptions) ? current->second : "canvas.visual_state.default");

    ImGui::SetNextItemWidth(150.0f);
    if (ImGui::BeginCombo("##visual_state", current_label.c_str())) {
        for (const auto& option : kVisualStateOptions) {
            const bool is_selected = option.first == cache_entry->visual_state;
            if (ImGui::Selectable(util::Tr(option.second).c_str(), is_selected)) {
                cache_entry->visual_state = option.first;
            }
            if (is_selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", util::Tr("canvas.visual_state.limitation").c_str());
    }
}

float DrawDevicePresetBar(int tab_id, DesignerVmCacheEntry* cache_entry, bool* out_view_reset_requested) {
    if (tab_id < 0 || cache_entry == nullptr) return 0.0f;

    const std::vector<designer::DeviceProfile>& profiles = designer::DeviceProfiles();
    const int count = static_cast<int>(profiles.size());
    if (cache_entry->device_preset_index < 0 || cache_entry->device_preset_index >= count) {
        cache_entry->device_preset_index = 0;
    }

    ImGui::SetNextItemWidth(220.0f);
    if (ImGui::BeginCombo("##device_preset", profiles[cache_entry->device_preset_index].name.c_str())) {
        for (int i = 0; i < count; ++i) {
            const bool is_selected = (i == cache_entry->device_preset_index);
            if (ImGui::Selectable(profiles[i].name.c_str(), is_selected)) {
                cache_entry->device_preset_index = i;
            }
            if (is_selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Button(util::Tr("canvas.reset_view").c_str())) {
        cache_entry->viewport.Reset();
        if (out_view_reset_requested) *out_view_reset_requested = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("-##canvas_zoom_out")) {
        designer::ZoomTool::ZoomOut(&cache_entry->viewport, designer::LayoutPoint{0.0, 0.0});
    }
    ImGui::SameLine();
    ImGui::Text("%d%%", static_cast<int>(std::lround(cache_entry->viewport.Zoom() * 100.0)));
    ImGui::SameLine();
    if (ImGui::Button("+##canvas_zoom_in")) {
        designer::ZoomTool::ZoomIn(&cache_entry->viewport, designer::LayoutPoint{0.0, 0.0});
    }
    ImGui::SameLine();
    if (ImGui::Button(util::Tr("canvas.zoom_fit").c_str())) {
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const designer::LayoutSize screen_size{static_cast<double>(std::max(avail.x, 1.0f)),
                                                static_cast<double>(std::max(avail.y, 1.0f))};
        designer::LayoutSize content_size = screen_size;
        if (cache_entry->device_preset_index > 0) {
            content_size = profiles[cache_entry->device_preset_index].size;
        }
        designer::ZoomTool::FitToScreen(&cache_entry->viewport, content_size, screen_size);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", util::Tr("canvas.zoom_render_limitation").c_str());
    }
    ImGui::SameLine();
    const bool is_design_mode = cache_entry->canvas_mode == designer::CanvasMode::Design;
    if (is_design_mode) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    if (ImGui::Button(util::Tr("canvas.mode_design").c_str())) {
        cache_entry->canvas_mode = designer::CanvasMode::Design;
    }
    if (is_design_mode) ImGui::PopStyleColor();
    ImGui::SameLine();
    if (!is_design_mode) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    if (ImGui::Button(util::Tr("canvas.mode_preview").c_str())) {
        cache_entry->canvas_mode = designer::CanvasMode::Preview;
    }
    if (!is_design_mode) ImGui::PopStyleColor();
    ImGui::SameLine();
    DrawVisualStateCombo(cache_entry);
    return ImGui::GetFrameHeightWithSpacing();
}

std::unordered_map<int, DesignerVmCacheEntry> g_designer_vm_cache;

std::string g_uncached_last_logged_live_render_error;
std::string g_uncached_last_logged_missing_rects;

struct CanvasDeleteRequest {
    bool open = false;
    int tab_id = -1;
    std::string node_id;
};
CanvasDeleteRequest g_canvas_delete_request;

struct CanvasExtractRequest {
    bool open = false;
    int tab_id = -1;
    std::string node_id;
};
CanvasExtractRequest g_canvas_extract_request;

designer::ResizeTool g_resize_tool;
std::vector<designer::AlignmentGuide> g_active_guides;
constexpr double kSnapThreshold = 4.0;

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

constexpr float kMinResizeDimension = 12.0f;

struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

ImVec2 ToImVec2(const Rect& r, ImVec2 origin) {
    return ImVec2(origin.x + r.x, origin.y + r.y);
}

constexpr float kNodeMargin = 3.0f;
constexpr float kNodeMarginPerDepth = 1.5f;
constexpr float kNodeMarginMax = 12.0f;
constexpr float kHeaderHeight = 20.0f;
constexpr float kChipHeight = 15.0f;
constexpr float kRealContainerPadSide = kNodeMarginMax + 4.0f;
constexpr float kRealContainerPadTop = kChipHeight + kRealContainerPadSide;
constexpr float kSelectionPad = 4.0f;
constexpr float kSelectionPadCompact = 2.0f;
constexpr float kCompactNodeHeightThreshold = 24.0f;

constexpr float kSelectionBorderThickness = 1.5f;
constexpr float kSelectionCornerRadius = 2.5f;
const ImU32 kSelectionBorderColor = palette::U32FromHex(palette::kPrimary, 0.85f);
const ImU32 kHoverBorderColor = palette::U32FromHex(palette::kPrimary, 0.5f);
constexpr float kCanvasTopReserve = kRealContainerPadTop;
constexpr float kPageOuterPad = 24.0f;
const ImU32 kCanvasBackdropColor = IM_COL32(0xDA, 0xDA, 0xDE, 0xFF);
const ImU32 kPageBorderColor = IM_COL32(0xC2, 0xC2, 0xC8, 0xFF);
const ImU32 kPageShadowColor = IM_COL32(0x00, 0x00, 0x00, 0x28);

struct SelectionBox {
    ImVec2 p0;
    ImVec2 p1;
    bool compact = false;
};

SelectionBox ComputeSelectionBox(ImVec2 base_p0, ImVec2 base_p1, bool has_own_padding = false) {
    SelectionBox box;
    box.compact = !has_own_padding && (base_p1.y - base_p0.y) < kCompactNodeHeightThreshold;
    const float pad = has_own_padding ? 0.0f : (box.compact ? kSelectionPadCompact : kSelectionPad);
    box.p0 = ImVec2(base_p0.x - pad, base_p0.y - pad);
    box.p1 = ImVec2(base_p1.x + pad, base_p1.y + pad);
    return box;
}

void DrawSelectionRing(ImDrawList* draw_list, ImVec2 p0, ImVec2 p1, ImU32 color) {
    draw_list->AddRect(p0, p1, color, kSelectionCornerRadius, 0, kSelectionBorderThickness);
}

void DrawInsetBand(ImDrawList* draw_list, ImVec2 origin, const designer::LayoutRect& outer,
                    const designer::LayoutRect& inner, ImU32 fill, ImU32 border) {
    const ImVec2 outer_p0(origin.x + static_cast<float>(outer.x), origin.y + static_cast<float>(outer.y));
    const ImVec2 outer_p1(outer_p0.x + static_cast<float>(outer.width), outer_p0.y + static_cast<float>(outer.height));
    const ImVec2 raw_inner_p0(origin.x + static_cast<float>(inner.x), origin.y + static_cast<float>(inner.y));
    const ImVec2 raw_inner_p1(raw_inner_p0.x + static_cast<float>(inner.width),
                               raw_inner_p0.y + static_cast<float>(inner.height));
    const ImVec2 inner_p0(std::clamp(raw_inner_p0.x, outer_p0.x, outer_p1.x),
                           std::clamp(raw_inner_p0.y, outer_p0.y, outer_p1.y));
    const ImVec2 inner_p1(std::clamp(raw_inner_p1.x, outer_p0.x, outer_p1.x),
                           std::clamp(raw_inner_p1.y, outer_p0.y, outer_p1.y));

    // Fill only the ring between outer and inner (top/bottom/left/right bands), so the real
    // content inside `inner` stays untouched instead of being washed out by a solid overlay.
    if (inner_p0.y > outer_p0.y) {
        draw_list->AddRectFilled(outer_p0, ImVec2(outer_p1.x, inner_p0.y), fill);
    }
    if (inner_p1.y < outer_p1.y) {
        draw_list->AddRectFilled(ImVec2(outer_p0.x, inner_p1.y), outer_p1, fill);
    }
    if (inner_p0.x > outer_p0.x) {
        draw_list->AddRectFilled(ImVec2(outer_p0.x, inner_p0.y), ImVec2(inner_p0.x, inner_p1.y), fill);
    }
    if (inner_p1.x < outer_p1.x) {
        draw_list->AddRectFilled(ImVec2(inner_p1.x, inner_p0.y), ImVec2(outer_p1.x, inner_p1.y), fill);
    }
    draw_list->AddRect(outer_p0, outer_p1, border, 0.0f, 0, 1.0f);
}

void DrawDashedRect(ImDrawList* draw_list, ImVec2 p0, ImVec2 p1, ImU32 color) {
    constexpr float kDash = 6.0f;
    constexpr float kGap = 4.0f;
    const auto dashed_line = [&](ImVec2 a, ImVec2 b) {
        const ImVec2 delta(b.x - a.x, b.y - a.y);
        const float length = std::sqrt(delta.x * delta.x + delta.y * delta.y);
        if (length <= 0.0f) return;
        const ImVec2 step(delta.x / length, delta.y / length);
        float travelled = 0.0f;
        while (travelled < length) {
            const float segment = std::min(kDash, length - travelled);
            const ImVec2 seg_p0(a.x + step.x * travelled, a.y + step.y * travelled);
            const ImVec2 seg_p1(a.x + step.x * (travelled + segment), a.y + step.y * (travelled + segment));
            draw_list->AddLine(seg_p0, seg_p1, color, 1.5f);
            travelled += kDash + kGap;
        }
    };
    dashed_line(p0, ImVec2(p1.x, p0.y));
    dashed_line(ImVec2(p1.x, p0.y), p1);
    dashed_line(p1, ImVec2(p0.x, p1.y));
    dashed_line(ImVec2(p0.x, p1.y), p0);
}

void SelectNode(designer::SelectionManager* selection, const std::string& node_id) {
    designer::SelectTool::Select(selection, node_id);
}

void ClearSelectedNode(designer::SelectionManager* selection) {
    designer::SelectTool::Clear(selection);
}

bool IsNodeSelected(const designer::SelectionManager* selection, const std::string& node_id) {
    return selection && selection->IsSelected(node_id);
}

std::string CurrentSelectedNodeId(const designer::SelectionManager* selection) {
    return selection ? selection->Primary() : std::string();
}

bool StyleOverrideHasField(const avalang::ui::theme::ControlStyleOverride& style, const std::string& name) {
    if (name == "backgroundColor") return style.backgroundColor.has_value();
    if (name == "textColor") return style.textColor.has_value();
    if (name == "borderColor") return style.borderColor.has_value();
    if (name == "fontName") return style.fontName.has_value();
    if (name == "fontSize") return style.fontSize.has_value();
    if (name == "borderWidth") return style.borderWidth.has_value();
    if (name == "borderRadius") return style.borderRadius.has_value();
    if (name == "padding") return style.padding.has_value();
    if (name == "margin") return style.margin.has_value();
    if (name == "spacing") return style.spacing.has_value();
    return false;
}

bool IsPureLayoutTypeName(const std::string& type_lower) {
    return type_lower == "row" || type_lower == "column" || type_lower == "stack" ||
           type_lower == "hstack" || type_lower == "vstack" || type_lower == "flex";
}

designer::PropertySource ResolvePropertySource(const design::DesignDocument& doc, avalang::ui::IComponent* node,
                                                const std::string& property_name, bool node_has_property,
                                                const std::string& project_root) {
    if (!node_has_property) {
        return designer::PropertySource::Default;
    }
    if (design::IsPropertyAuthored(doc, node->NodeId(), property_name)) {
        return designer::PropertySource::Local;
    }

    const std::string type_lower = LowerAscii(node->TypeName());
    const avalang::ui::theme::ProjectStyleSheet sheet = avalang::ui::theme::LoadProjectStyleOverrides(project_root);
    if (sheet.HasAnyStyles()) {
        const avalang::ui::theme::ControlStyleOverride resolved =
            sheet.Resolve(type_lower, IsPureLayoutTypeName(type_lower));
        if (StyleOverrideHasField(resolved, property_name)) {
            return designer::PropertySource::Style;
        }
    }
    return designer::PropertySource::Theme;
}

std::vector<designer::PropertyGridSection> BuildDesignerPropertyGrid(const design::DesignDocument& doc,
                                                                      avalang::ui::IComponent* node,
                                                                      const std::string& project_root) {
    std::vector<designer::PropertyGridSection> sections = designer::BuildPropertyGrid(node);

    for (designer::PropertyGridSection& section : sections) {
        if (section.category == designer::PropertyCategory::Identity) continue;
        for (designer::PropertyGridRow& row : section.rows) {
            const bool node_has_property = node->HasProperty(row.metadata.name);
            row.source = ResolvePropertySource(doc, node, row.metadata.name, node_has_property, project_root);
        }
    }

    std::unordered_set<std::string> known_names{"id"};
    for (const designer::PropertyGridSection& section : sections) {
        for (const designer::PropertyGridRow& row : section.rows) {
            known_names.insert(row.metadata.name);
        }
    }

    designer::PropertyGridSection advanced{designer::PropertyCategory::Advanced, {}};
    for (const auto& name : node->PropertyNames()) {
        if (known_names.count(name)) continue;
        const auto* value = node->GetProperty(name);
        if (!value) continue;

        designer::PropertyMetadata metadata;
        metadata.name = name;
        metadata.type = designer::PropertyTypeName(value->Type());
        metadata.category = designer::PropertyCategory::Advanced;
        metadata.designerEditor = avalang::ui::IsEventPropertyName(name) ? designer::PropertyEditorKind::Event
                                                                          : designer::PropertyEditorKind::String;

        designer::PropertyGridRow row;
        row.metadata = metadata;
        row.value = designer::FormatPropertyValue(*value);
        row.source = ResolvePropertySource(doc, node, name, true, project_root);
        advanced.rows.push_back(row);
    }
    if (!advanced.rows.empty()) {
        sections.push_back(std::move(advanced));
    }

    return sections;
}

PropertiesState ToPropertiesState(avalang::ui::IComponent* node, bool editable, int tab_id) {
    PropertiesState state;
    state.selected_component_type = LowerAscii(node->TypeName());
    state.selected_component_id = GetNodeIdProp(node);
    CollectPropertyRows(node, &state.properties, &state.events);
    state.editable = editable;
    state.source_tab_id = tab_id;
    state.selected_node_id = node->NodeId();
    return state;
}

PropertiesState ToPropertiesState(avalang::ui::IComponent* node, bool editable, int tab_id,
                                   const design::DesignDocument& doc, const std::string& project_root) {
    PropertiesState state = ToPropertiesState(node, editable, tab_id);
    if (editable) {
        state.grid = BuildDesignerPropertyGrid(doc, node, project_root);
    }
    return state;
}

std::string FindPropValue(const std::vector<PropertyRow>& props, const std::string& key,
                           const std::string& fallback) {
    for (const PropertyRow& p : props) {
        if (p.key == key) return p.value;
    }
    return fallback;
}

float FindPropValueF(const std::vector<PropertyRow>& props, const std::string& key,
                      float fallback) {
    const std::string raw = FindPropValue(props, key, std::string());
    if (raw.empty()) return fallback;
    char* end = nullptr;
    const float parsed = std::strtof(raw.c_str(), &end);
    return (end != raw.c_str()) ? parsed : fallback;
}

design::DropZone ComputeDropZone(float mouse_y, ImVec2 p0, ImVec2 p1, bool is_container) {
    const designer::LayoutRect rect{p0.x, p0.y, p1.x - p0.x, std::max(p1.y - p0.y, 1.0f)};
    const designer::LayoutPoint point{p0.x, mouse_y};
    return designer::ComputeDropZone(rect, point, is_container);
}

void DrawDropIndicator(ImVec2 p0, ImVec2 p1, design::DropZone zone, bool is_container) {
    const designer::LayoutRect target{p0.x, p0.y, p1.x - p0.x, p1.y - p0.y};
    const designer::DropIndicator indicator = designer::ComputeDropIndicator(target, zone, is_container);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const ImU32 highlight = palette::U32FromHex(palette::kPrimary);
    const ImVec2 ip0(static_cast<float>(indicator.rect.x), static_cast<float>(indicator.rect.y));
    const ImVec2 ip1(ip0.x + static_cast<float>(indicator.rect.width), ip0.y + static_cast<float>(indicator.rect.height));
    if (indicator.isLine) {
        const float mid_y = ip0.y + static_cast<float>(indicator.rect.height) * 0.5f;
        draw_list->AddLine(ImVec2(ip0.x, mid_y), ImVec2(ip1.x, mid_y), highlight, 3.0f);
    } else {
        draw_list->AddRect(ip0, ip1, highlight, 2.0f, 0, 3.0f);
    }
}

void HandleDropTarget(avalang::ui::IComponent* node, design::DesignDocument& doc,
                       designer::CommandManager* command_manager, designer::SelectionManager* selection,
                       bool is_container, ImVec2 p0, ImVec2 p1) {
    if (ImGui::BeginDragDropTarget()) {
        if (ImGui::AcceptDragDropPayload(kNodeMoveDragDropId, ImGuiDragDropFlags_AcceptPeekOnly)) {
            const design::DropZone zone = ComputeDropZone(ImGui::GetMousePos().y, p0, p1, is_container);
            DrawDropIndicator(p0, p1, zone, is_container);
        }

        if (is_container) {
            if (ImGui::AcceptDragDropPayload(kToolboxDragDropId, ImGuiDragDropFlags_AcceptPeekOnly)) {
                DrawDropIndicator(p0, p1, design::DropZone::kInto, true);
            }
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kToolboxDragDropId)) {
                const std::string dropped_type(static_cast<const char*>(payload->Data));
                if (avalang::ui::IComponent* real = design::FindNodeById(doc.Root(), node->NodeId())) {
                    designer::InsertTool::InsertInto(command_manager, doc, selection, real->NodeId(), dropped_type);
                }
            }
        } else {
            if (ImGui::AcceptDragDropPayload(kToolboxDragDropId, ImGuiDragDropFlags_AcceptPeekOnly)) {
                const design::DropZone zone = ComputeDropZone(ImGui::GetMousePos().y, p0, p1, is_container);
                DrawDropIndicator(p0, p1, zone, false);
            }
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kToolboxDragDropId)) {
                const std::string dropped_type(static_cast<const char*>(payload->Data));
                avalang::ui::IComponent* target = design::FindNodeById(doc.Root(), node->NodeId());
                avalang::ui::IComponent* parent = target ? design::FindParentOf(doc.Root(), target) : nullptr;
                if (parent) {
                    const design::DropZone zone = ComputeDropZone(ImGui::GetMousePos().y, p0, p1, is_container);
                    designer::InsertTool::InsertRelative(command_manager, doc, selection, parent->NodeId(),
                                                          node->NodeId(), zone, dropped_type);
                }
            }
        }

        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kNodeMoveDragDropId)) {
            const std::string moved_id(static_cast<const char*>(payload->Data));
            const design::DropZone zone = ComputeDropZone(ImGui::GetMousePos().y, p0, p1, is_container);
            designer::MoveTool::Execute(command_manager, doc, selection, moved_id, node->NodeId(), zone);
        }

        ImGui::EndDragDropTarget();
    }
}

#define AVA_FASE10_PASO_B_MODE 2

int PushClassicFrameStyle() {
    ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0xE0, 0xE0, 0xE0, 0xFF));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(0xE8, 0xE8, 0xE8, 0xFF));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(0xD0, 0xD0, 0xD0, 0xFF));
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0xE0, 0xE0, 0xE0, 0xFF));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(0xE8, 0xE8, 0xE8, 0xFF));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(0xD0, 0xD0, 0xD0, 0xFF));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0x80, 0x80, 0x80, 0xFF));
    ImGui::PushStyleColor(ImGuiCol_CheckMark, IM_COL32(0x00, 0x00, 0x00, 0xFF));
    return 8;
}

void PushClassicTextStyle() {
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0x00, 0x00, 0x00, 0xFF));
}

std::string ResolveImageSrcPath(const std::string& src, const std::string& project_root) {
    if (src.empty()) return {};
    const std::filesystem::path p(src);
    if (p.is_absolute() || project_root.empty()) return src;
    return (std::filesystem::path(project_root) / p).string();
}

struct ImagePreviewEntry {
    unsigned int texture_id = 0;
    int width = 0;
    int height = 0;
};

std::unordered_map<std::string, ImagePreviewEntry> g_image_preview_cache;
std::unordered_set<std::string> g_image_preview_failed;

const ImagePreviewEntry* GetOrLoadImagePreview(const std::string& resolved_path) {
    if (resolved_path.empty()) return nullptr;
    if (g_image_preview_failed.count(resolved_path) != 0) return nullptr;

    const auto cached = g_image_preview_cache.find(resolved_path);
    if (cached != g_image_preview_cache.end()) return &cached->second;

    int width = 0, height = 0, channels = 0;
    unsigned char* pixels = stbi_load(resolved_path.c_str(), &width, &height, &channels, 4 );
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

std::optional<ImU32> TryHexToImU32(const std::optional<std::string>& hex) {
    if (!hex) return std::nullopt;
    float rgba[4];
    if (!designer::TryParseColorValue(*hex, rgba)) return std::nullopt;
    return ImGui::ColorConvertFloat4ToU32(ImVec4(rgba[0], rgba[1], rgba[2], rgba[3]));
}

avalang::ui::theme::ControlStyleOverride ResolveVisualStateOverride(
    const avalang::ui::theme::ProjectStyleSheet* project_styles, const std::string& type_lower,
    designer::DesignerVisualState visual_state) {
    if (project_styles == nullptr || visual_state == designer::DesignerVisualState::Default) {
        return {};
    }
    return designer::ResolveVisualState(*project_styles, type_lower, visual_state);
}

int PushButtonVisualStateOverride(const avalang::ui::theme::ControlStyleOverride& resolved) {
    int pushed = 0;
    if (const std::optional<ImU32> background = TryHexToImU32(resolved.backgroundColor)) {
        ImGui::PushStyleColor(ImGuiCol_Button, *background);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, *background);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, *background);
        pushed += 3;
    }
    if (const std::optional<ImU32> text = TryHexToImU32(resolved.textColor)) {
        ImGui::PushStyleColor(ImGuiCol_Text, *text);
        pushed += 1;
    }
    if (const std::optional<ImU32> border = TryHexToImU32(resolved.borderColor)) {
        ImGui::PushStyleColor(ImGuiCol_Border, *border);
        pushed += 1;
    }
    return pushed;
}

bool DrawRealWidget(avalang::ui::IComponent* node, const std::string& evaluated_display, ImVec2 p0, ImVec2 p1,
                     const std::string& project_root,
                     const avalang::ui::theme::ProjectStyleSheet* project_styles,
                     designer::DesignerVisualState visual_state) {
    std::vector<PropertyRow> properties;
    CollectPropertyRows(node, &properties, nullptr);
    const std::string type = LowerAscii(node->TypeName());

    const ImVec2 size(std::max(p1.x - p0.x, 1.0f), std::max(p1.y - p0.y, 1.0f));
    ImGui::SetCursorScreenPos(p0);
    ImGui::PushItemWidth(size.x);

    const bool node_enabled = FindPropValue(properties, "disabled", "false") == "false";
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, node_enabled ? 1.0f : 0.60f);

#if AVA_FASE10_PASO_B_MODE == 0
    ImGui::BeginDisabled(true);
    ImGui::PushStyleVar(ImGuiStyleVar_DisabledAlpha, 1.0f);
#elif AVA_FASE10_PASO_B_MODE == 1
    if (!node_enabled) ImGui::BeginDisabled(true);
#elif AVA_FASE10_PASO_B_MODE == 2
    ImGui::SetNextItemAllowOverlap();
    if (!node_enabled) ImGui::BeginDisabled(true);
#else
#error "AVA_FASE10_PASO_B_MODE debe ser 0, 1 o 2"
#endif

    bool handled = true;
    if (type == "button") {
        const int frame_colors = PushClassicFrameStyle();
        PushClassicTextStyle();
        const avalang::ui::theme::ControlStyleOverride state_override =
            ResolveVisualStateOverride(project_styles, type, visual_state);
        const int state_colors = PushButtonVisualStateOverride(state_override);
        const float border_radius = FindPropValueF(properties, "borderRadius", 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, border_radius);
        ImGui::Button(evaluated_display.c_str(), size);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(frame_colors + 1 + state_colors);
    } else if (type == "textbox") {
        const int frame_colors = PushClassicFrameStyle();
        PushClassicTextStyle();
        std::string buf = evaluated_display;
        buf.resize(std::max<size_t>(buf.size() + 1, 256), '\0');
        ImGui::InputText("##textbox_preview", buf.data(), buf.size(), ImGuiInputTextFlags_ReadOnly);
        ImGui::PopStyleColor(frame_colors + 1);
    } else if (type == "checkbox") {
        const int frame_colors = PushClassicFrameStyle();
        bool checked = FindPropValue(properties, design::GetCheckedPropertyKey(type), "false") == "true";
        ImGui::Checkbox(evaluated_display.c_str(), &checked);
        ImGui::PopStyleColor(frame_colors);
    } else if (type == "radiobutton") {
        const int frame_colors = PushClassicFrameStyle();
        const bool checked = FindPropValue(properties, design::GetCheckedPropertyKey(type), "false") == "true";
        ImGui::RadioButton(evaluated_display.c_str(), checked);
        ImGui::PopStyleColor(frame_colors);
    } else if (type == "text") {
        ImGui::GetWindowDrawList()->AddRectFilled(p0, p1, IM_COL32(0xE0, 0xE0, 0xE0, 0xFF));
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0x00, 0x00, 0x00, 0xFF));
        ImGui::TextUnformatted(evaluated_display.c_str());
        ImGui::PopStyleColor();
    } else if (type == "link") {
        const avalang::ui::theme::ControlStyleOverride state_override =
            ResolveVisualStateOverride(project_styles, type, visual_state);
        const ImU32 link_color =
            TryHexToImU32(state_override.textColor).value_or(palette::U32FromHex(palette::kPrimary));
        ImGui::PushStyleColor(ImGuiCol_Text, link_color);
        ImGui::TextUnformatted(evaluated_display.c_str());
        ImGui::PopStyleColor();
        const ImVec2 text_size = ImGui::CalcTextSize(evaluated_display.c_str());
        ImGui::GetWindowDrawList()->AddLine(ImVec2(p0.x, p0.y + text_size.y),
                                             ImVec2(p0.x + text_size.x, p0.y + text_size.y),
                                             link_color, 1.0f);
    } else if (type == "divider") {
        ImGui::GetWindowDrawList()->AddLine(ImVec2(p0.x, p0.y + size.y * 0.5f), ImVec2(p1.x, p0.y + size.y * 0.5f),
                                             palette::U32FromHex(palette::kBorder), 1.0f);
    } else if (type == "spacer") {

    } else if (type == "image") {
        const std::string src = FindPropValue(properties, "src", "");
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
    ImGui::PopStyleVar(2);
    ImGui::EndDisabled();
#else
    ImGui::PopStyleVar(1);
    if (!node_enabled) ImGui::EndDisabled();
#endif
    ImGui::PopItemWidth();
    return handled;
}

bool IsDialogNode(avalang::ui::IComponent* node) { return LowerAscii(node->TypeName()) == "dialog"; }

bool IsNodeDrawnInCanvas(avalang::ui::IComponent* root, const std::string& node_id) {
    if (root == nullptr) return false;
    if (root->NodeId() == node_id) return true;
    for (avalang::ui::IComponent* child : root->Children()) {
        if (IsDialogNode(child)) continue;
        if (IsNodeDrawnInCanvas(child, node_id)) return true;
    }
    return false;
}

std::string PickSurfaceNode(design::DesignDocument& doc, const designer::LayoutCore& layout, ImVec2 origin) {
    if (!ImGui::IsWindowHovered()) return std::string();
    if (ImGui::GetDragDropPayload() != nullptr) return std::string();

    const ImVec2 mouse = ImGui::GetMousePos();
    const designer::LayoutPoint canvas_point{static_cast<double>(mouse.x - origin.x),
                                              static_cast<double>(mouse.y - origin.y)};
    const designer::NodeId picked = designer::HitTest(doc.tree.get(), layout, canvas_point);
    if (picked.empty()) return std::string();
    if (!IsNodeDrawnInCanvas(doc.Root(), picked)) return std::string();
    return picked;
}

void DrawNode(avalang::ui::IComponent* node, ImVec2 origin,
              design::DesignDocument& doc,
              std::optional<PropertiesState>& out_selected, int tab_id,
              designer::CommandManager* command_manager,
              designer::SelectionManager* selection,
              std::string* out_generated_handler, AvaVM* state_vm,
              std::unordered_map<std::string, std::string>* eval_cache,
              const std::string& project_root, bool live_render_painted,
              const avalang::ui::theme::ProjectStyleSheet* project_styles,
              designer::DesignerVisualState visual_state,
              const std::unordered_map<std::string, avalang::ui::LayoutRect>* uid_to_rect = nullptr,
              std::vector<std::string>* out_missing_rect_uids = nullptr,
              const std::string* surface_hovered_node_id = nullptr,
              std::string* out_imgui_hovered_node_id = nullptr,
              std::string* out_click_reroute = nullptr,
              std::unordered_map<std::string, NodeDecoration>* out_decorations = nullptr,
              bool design_mode = true,
              float extra_offset_y = 0.0f, int depth = 0,
              ImVec2 canvas_min = ImVec2(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()),
              ImVec2 canvas_max = ImVec2(std::numeric_limits<float>::max(), std::numeric_limits<float>::max())) {
    const std::string type_name = LowerAscii(node->TypeName());
    const design::ComponentTypeInfo* info = design::FindComponentType(type_name);
    const bool is_container = info != nullptr && info->is_container;

    Rect r{};
    bool have_rect = false;
    if (uid_to_rect != nullptr) {
        const auto lr_it = uid_to_rect->find(node->NodeId());
        if (lr_it != uid_to_rect->end()) {
            r = Rect{static_cast<float>(lr_it->second.x), static_cast<float>(lr_it->second.y),
                     static_cast<float>(lr_it->second.width), static_cast<float>(lr_it->second.height)};
            have_rect = true;
        }
    }
    if (!have_rect) {
        if (out_missing_rect_uids != nullptr) out_missing_rect_uids->push_back(node->NodeId());
        return;
    }
    if (g_resize_tool.IsDragging(node->NodeId())) {
        if (g_resize_tool.ResizesX()) r.w = g_resize_tool.PreviewWidth();
        if (g_resize_tool.ResizesY()) r.h = g_resize_tool.PreviewHeight();
    }

    const ImVec2 base_p0 = ToImVec2(r, origin);
    const ImVec2 raw_p0 = ImVec2(base_p0.x, base_p0.y + extra_offset_y);
    const ImVec2 raw_p1 = ImVec2(raw_p0.x + r.w, raw_p0.y + r.h);
    const float margin = std::min(kNodeMargin + static_cast<float>(depth) * kNodeMarginPerDepth, kNodeMarginMax);
    const ImVec2 p0(raw_p0.x + margin, raw_p0.y + margin);
    const ImVec2 p1(std::max(p0.x, raw_p1.x - margin), std::max(p0.y, raw_p1.y - margin));

    const bool pad_chrome = is_container && depth > 0;
    const ImVec2 raw_chrome_p0 =
        pad_chrome ? ImVec2(p0.x - kRealContainerPadSide, p0.y - kRealContainerPadTop) : p0;
    const ImVec2 raw_chrome_p1 =
        pad_chrome ? ImVec2(p1.x + kRealContainerPadSide, p1.y + kRealContainerPadSide) : p1;
    const ImVec2 chrome_p0(std::clamp(raw_chrome_p0.x, canvas_min.x, canvas_max.x),
                            std::clamp(raw_chrome_p0.y, canvas_min.y, canvas_max.y));
    const ImVec2 chrome_p1(std::clamp(raw_chrome_p1.x, canvas_min.x, canvas_max.x),
                            std::clamp(raw_chrome_p1.y, canvas_min.y, canvas_max.y));

    const bool selected = design_mode && IsNodeSelected(selection, node->NodeId());
    const bool skip_leaf_wireframe = live_render_painted && !is_container;
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    constexpr bool synthetic = false;
    ImGui::PushID(node->NodeId().c_str());

    const ImVec2 base_sel_p0 = is_container ? chrome_p0 : (skip_leaf_wireframe ? raw_p0 : p0);
    const ImVec2 base_sel_p1 = is_container ? chrome_p1 : (skip_leaf_wireframe ? raw_p1 : p1);
    const SelectionBox sel_box = ComputeSelectionBox(base_sel_p0, base_sel_p1, is_container);
    const ImVec2 sel_p0 = sel_box.p0;
    const ImVec2 sel_p1 = sel_box.p1;
    const bool compact = sel_box.compact;

    const bool overlay_driven = out_decorations != nullptr;
    if (overlay_driven) {
        NodeDecoration decoration;
        decoration.rect = designer::LayoutRect{static_cast<double>(sel_p0.x - origin.x),
                                                static_cast<double>(sel_p0.y - origin.y),
                                                static_cast<double>(sel_p1.x - sel_p0.x),
                                                static_cast<double>(sel_p1.y - sel_p0.y)};
        decoration.compact = compact;
        (*out_decorations)[node->NodeId()] = decoration;
    }

    const ImU32 fill = is_container ? IM_COL32(0xF2, 0xF2, 0xF4, 0xF0)
                                     : IM_COL32(0xE9, 0xE9, 0xEC, 0xF5);
    const ImU32 border = selected ? kSelectionBorderColor
                                   : (is_container ? IM_COL32(0xC9, 0xC9, 0xCE, 0x73)
                                                    : IM_COL32(0xC9, 0xC9, 0xCE, 0xFF));

    const bool skip_body_fill = live_render_painted && is_container;
    if (!skip_leaf_wireframe) {
        if (!skip_body_fill) {
            draw_list->AddRectFilled(chrome_p0, chrome_p1, fill, 2.0f);
        }
        if (!selected) {
            draw_list->AddRect(chrome_p0, chrome_p1, border, 2.0f, 0, 1.0f);
        }
    }

    if (selected) {
        if (!overlay_driven) {
            DrawSelectionRing(draw_list, sel_p0, sel_p1, kSelectionBorderColor);
        }

        const bool resizable = designer::ResizeTool::CanResize(node, doc.Root());
        if (!compact && resizable) {
            constexpr float kHandle = 6.0f;
            constexpr float kHandleHalf = kHandle * 0.5f;
            const float mid_x = (sel_p0.x + sel_p1.x) * 0.5f;
            const float mid_y = (sel_p0.y + sel_p1.y) * 0.5f;
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
            const ImU32 handle_fill = IM_COL32(0xFF, 0xFF, 0xFF, 0xFF);
            const ImU32 handle_border = palette::U32FromHex(palette::kPrimary);
            for (const ImVec2& c : handles) {
                const ImVec2 hp0(c.x - kHandleHalf, c.y - kHandleHalf);
                const ImVec2 hp1(c.x + kHandleHalf, c.y + kHandleHalf);
                draw_list->AddRectFilled(hp0, hp1, handle_fill);
                draw_list->AddRect(hp0, hp1, handle_border, 0.0f, 0, 1.0f);
            }

            if (!synthetic) {
                const auto ResizeHandle = [&](const char* str_id, ImVec2 center, ImGuiMouseCursor cursor, bool adjust_x,
                                              bool adjust_y) {
                    constexpr float kHitHalf = kHandleHalf + 3.0f;
                    ImGui::SetCursorScreenPos(ImVec2(center.x - kHitHalf, center.y - kHitHalf));
                    ImGui::InvisibleButton(str_id, ImVec2(kHitHalf * 2.0f, kHitHalf * 2.0f));
                    if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
                        ImGui::SetMouseCursor(cursor);
                    }
                    if (ImGui::IsItemActivated()) {
                        float start_w = base_sel_p1.x - base_sel_p0.x;
                        float start_h = base_sel_p1.y - base_sel_p0.y;
                        std::vector<PropertyRow> size_props;
                        CollectPropertyRows(node, &size_props, nullptr);
                        TryGetNumericProperty(size_props, "width", &start_w);
                        TryGetNumericProperty(size_props, "height", &start_h);
                        g_resize_tool.Begin(node->NodeId(), adjust_x, adjust_y, start_w, start_h);
                        g_active_guides.clear();
                    }
                    if (ImGui::IsItemActive() && g_resize_tool.IsDragging(node->NodeId())) {
                        const ImVec2 total_delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0f);
                        g_resize_tool.UpdateDelta(total_delta.x, total_delta.y, kMinResizeDimension);

                        g_active_guides.clear();
                        if (uid_to_rect != nullptr) {
                            if (avalang::ui::IComponent* parent = design::FindParentOf(doc.Root(), node)) {
                                std::vector<std::pair<designer::NodeId, designer::LayoutRect>> sibling_rects;
                                for (avalang::ui::IComponent* sibling : parent->Children()) {
                                    if (sibling == node) continue;
                                    const auto it = uid_to_rect->find(sibling->NodeId());
                                    if (it == uid_to_rect->end()) continue;
                                    sibling_rects.emplace_back(
                                        sibling->NodeId(),
                                        designer::LayoutRect{it->second.x, it->second.y, it->second.width,
                                                              it->second.height});
                                }
                                const designer::LayoutRect moving{r.x, r.y, g_resize_tool.PreviewWidth(),
                                                                   g_resize_tool.PreviewHeight()};
                                const designer::SnapResult snap =
                                    designer::ComputeSnap(moving, sibling_rects, kSnapThreshold);
                                g_resize_tool.ApplySnap(snap.dx, snap.dy);
                                g_active_guides = snap.guides;
                            }
                        }
                    }
                    if (ImGui::IsItemDeactivated() && g_resize_tool.IsDragging(node->NodeId())) {
                        if (g_resize_tool.Commit(command_manager, doc, selection)) {
                            doc.dirty = true;
                        }
                        g_active_guides.clear();
                    }
                };
                ResizeHandle("##resize_se", sel_p1, ImGuiMouseCursor_ResizeNWSE, true, true);
                ResizeHandle("##resize_e", ImVec2(sel_p1.x, mid_y), ImGuiMouseCursor_ResizeEW, true, false);
                ResizeHandle("##resize_s", ImVec2(mid_x, sel_p1.y), ImGuiMouseCursor_ResizeNS, false, true);
            }
        }
    }

    const bool header_reserves_space = is_container && !live_render_painted;
    const float header_bottom = header_reserves_space ? std::min(p1.y, p0.y + kHeaderHeight) : p0.y;
    if (is_container) {
        if (header_reserves_space) {
            draw_list->AddRectFilled(p0, ImVec2(p1.x, header_bottom), palette::U32FromHex(palette::kBorder, 0.55f),
                                      2.0f, ImDrawFlags_RoundCornersTop);
            draw_list->AddLine(ImVec2(p0.x, header_bottom), ImVec2(p1.x, header_bottom),
                                palette::U32FromHex(palette::kBorder), 1.0f);
        } else if (design_mode) {
            const bool container_hovered = surface_hovered_node_id != nullptr
                                                ? (*surface_hovered_node_id == node->NodeId())
                                                : ImGui::IsMouseHoveringRect(p0, p1);
            if (container_hovered) {
                ImGui::SetTooltip("%s", type_name.c_str());
            }
        }
    } else if (design_mode) {
        const bool leaf_hovered = surface_hovered_node_id != nullptr
                                       ? (*surface_hovered_node_id == node->NodeId())
                                       : ImGui::IsMouseHoveringRect(p0, p1);
        if (leaf_hovered) {
            ImGui::SetTooltip("%s", type_name.c_str());
        }
    }

    const std::vector<avalang::ui::IComponent*> node_children = node->Children();

    if (design_mode && is_container && !synthetic && node_children.empty() && header_bottom < p1.y - 4.0f) {
        const ImVec2 body_center((p0.x + p1.x) * 0.5f, (header_bottom + p1.y) * 0.5f);
        const ImU32 hint_color = palette::U32FromHex(palette::kTextDisabled);
        const float half = 6.0f;
        if (p1.y - header_bottom > half * 4.0f) {
            draw_list->AddLine(ImVec2(body_center.x - half, body_center.y - 12.0f),
                                ImVec2(body_center.x + half, body_center.y - 12.0f), hint_color, 1.5f);
            draw_list->AddLine(ImVec2(body_center.x, body_center.y - 12.0f - half),
                                ImVec2(body_center.x, body_center.y - 12.0f + half), hint_color, 1.5f);
            const std::string hint_text = util::Tr("canvas.drop_hint");
            const ImVec2 text_size = ImGui::CalcTextSize(hint_text.c_str());
            draw_list->AddText(ImVec2(body_center.x - text_size.x * 0.5f, body_center.y), hint_color,
                                hint_text.c_str());
        }
    }

    const std::string node_id_prop = GetNodeIdProp(node);
    std::string label = type_name;
    if (!node_id_prop.empty()) label += " (" + node_id_prop + ")";
    if (synthetic) label += " [import]";

    std::vector<PropertyRow> properties;
    std::vector<PropertyRow> events;
    CollectPropertyRows(node, &properties, &events);

    const std::string display_key = design::GetDisplayPropertyKey(type_name);
    std::string evaluated_display;
    if (!display_key.empty()) {
        for (const PropertyRow& prop : properties) {
            if (prop.key == display_key) {
                if (eval_cache != nullptr) {
                    const std::string cache_key = node->NodeId() + '\x1f' + prop.value;
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

    bool widget_drawn = live_render_painted && !is_container;
    if (!widget_drawn && !is_container) {
        const ImVec2 widget_p0(p0.x + 2.0f, p0.y + 2.0f);
        const ImVec2 widget_p1(std::max(widget_p0.x, p1.x - 2.0f), std::max(widget_p0.y, p1.y - 2.0f));
        widget_drawn = DrawRealWidget(node, evaluated_display, widget_p0, widget_p1, project_root, project_styles,
                                       visual_state);
    }
    if (!widget_drawn && !is_container) {
        draw_list->AddText(ImVec2(p0.x + 4.0f, p0.y + 4.0f), palette::U32FromHex(palette::kTextPrimary),
                            label.c_str());
        if (!evaluated_display.empty()) {
            draw_list->AddText(ImVec2(p0.x + 4.0f, p0.y + 20.0f),
                                palette::U32FromHex(palette::kTextSecondary), evaluated_display.c_str());
        }
    }

    const ImVec2 hit_p1 = !is_container ? p1 : (header_reserves_space ? ImVec2(p1.x, header_bottom) : p1);

    ImGui::SetCursorScreenPos(sel_p0);
    ImGui::SetNextItemAllowOverlap();
    ImGui::InvisibleButton("##node_hit_area",
                            ImVec2(std::max(sel_p1.x - sel_p0.x, 1.0f), std::max(sel_p1.y - sel_p0.y, 1.0f)));
    const bool imgui_hovered = ImGui::IsItemHovered();
    const bool item_clicked = ImGui::IsItemClicked();

    if (design_mode) {
        if (imgui_hovered && out_imgui_hovered_node_id != nullptr) {
            *out_imgui_hovered_node_id = node->NodeId();
        }
        const bool node_hovered = surface_hovered_node_id != nullptr
                                      ? (*surface_hovered_node_id == node->NodeId())
                                      : imgui_hovered;
        if (!synthetic && node_hovered) {

            ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
            if (!selected && !overlay_driven) {
                DrawSelectionRing(draw_list, sel_p0, sel_p1, kHoverBorderColor);
            }
        }
        if (item_clicked) {
            avalang::ui::IComponent* clicked = node;
            if (surface_hovered_node_id != nullptr && !surface_hovered_node_id->empty() &&
                *surface_hovered_node_id != node->NodeId()) {
                if (avalang::ui::IComponent* picked = design::FindNodeById(doc.Root(), *surface_hovered_node_id)) {
                    clicked = picked;
                    if (out_click_reroute != nullptr) {
                        *out_click_reroute = node->NodeId() + " -> " + picked->NodeId();
                    }
                }
            }

            SelectNode(selection, clicked->NodeId());
            out_selected = ToPropertiesState(clicked, !synthetic, tab_id, doc, project_root);

            if (!synthetic && ImGui::GetIO().KeyCtrl) {
                InvokeNodeClickHandler(state_vm, clicked, node, events, eval_cache);
            }
        }

        if (!synthetic && type_name == "button" && node_hovered &&
            ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            const std::string handler = design::EnsureClickHandler(doc, node->NodeId());
            if (!handler.empty() && out_generated_handler) {
                *out_generated_handler = handler;
            }
        }

        const bool movable = !synthetic && designer::MoveTool::CanMove(node, doc.Root());
        if (movable && ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload(kNodeMoveDragDropId, node->NodeId().c_str(), node->NodeId().size() + 1);
            ImGui::TextUnformatted(label.c_str());
            ImGui::EndDragDropSource();
        }

        if (movable && ImGui::BeginPopupContextItem("##node_context_menu")) {
            if (ImGui::MenuItem(util::Tr("canvas.extract_component").c_str())) {
                g_canvas_extract_request = {true, tab_id, node->NodeId()};
            }
            if (ImGui::MenuItem(util::Tr("explorer.delete").c_str())) {
                g_canvas_delete_request = {true, tab_id, node->NodeId()};
            }
            ImGui::EndPopup();
        }

        if (!synthetic) {
            HandleDropTarget(node, doc, command_manager, selection, is_container, p0, hit_p1);
        }
    } else if (!synthetic && item_clicked) {
        InvokeNodeClickHandler(state_vm, node, node, events, eval_cache);
    }

    if (design_mode) {
        if (is_container && !synthetic && header_reserves_space) {
            const SelectionBox body_sel_box = ComputeSelectionBox(ImVec2(p0.x, header_bottom), p1);
            ImGui::SetCursorScreenPos(ImVec2(p0.x, header_bottom));
            ImGui::SetNextItemAllowOverlap();
            ImGui::InvisibleButton("##node_body_drop_area",
                                    ImVec2(std::max(p1.x - p0.x, 1.0f), std::max(p1.y - header_bottom, 1.0f)));
            if (ImGui::IsItemHovered()) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
                if (ImGui::GetDragDropPayload() != nullptr) {
                    draw_list->AddRectFilled(ImVec2(p0.x, header_bottom), p1,
                                              palette::U32FromHex(palette::kPrimary, 0.06f));
                }

                if (!selected) {
                    DrawSelectionRing(draw_list, body_sel_box.p0, body_sel_box.p1, kHoverBorderColor);
                }
            }
            if (ImGui::IsItemClicked()) {
                SelectNode(selection, node->NodeId());
                out_selected = ToPropertiesState(node, true, tab_id, doc, project_root);
            }
            HandleDropTarget(node, doc, command_manager, selection, is_container, ImVec2(p0.x, header_bottom), p1);
        }
    }

    ImGui::PopID();

    const float child_offset_y = extra_offset_y + (header_reserves_space ? kHeaderHeight : 0.0f);
    for (avalang::ui::IComponent* child : node_children) {
        if (IsDialogNode(child)) continue;
        DrawNode(child, origin, doc, out_selected, tab_id, command_manager, selection, out_generated_handler,
                 state_vm, eval_cache, project_root, live_render_painted, project_styles, visual_state, uid_to_rect,
                 out_missing_rect_uids, surface_hovered_node_id, out_imgui_hovered_node_id, out_click_reroute,
                 out_decorations, design_mode, child_offset_y, depth + 1, canvas_min, canvas_max);
    }
}

struct BreadcrumbSegment {
    std::string label;
    std::string node_id;
};

bool CollectBreadcrumbPath(avalang::ui::IComponent* node, const std::string& target_id,
                            std::vector<BreadcrumbSegment>& out) {
    std::string label = LowerAscii(node->TypeName());
    const std::string node_id_prop = GetNodeIdProp(node);
    if (!node_id_prop.empty()) label += " (" + node_id_prop + ")";
    out.push_back({label, node->NodeId()});
    if (node->NodeId() == target_id) return true;
    for (avalang::ui::IComponent* child : node->Children()) {
        if (CollectBreadcrumbPath(child, target_id, out)) return true;
    }
    out.pop_back();
    return false;
}

float DrawBreadcrumbBar(avalang::ui::IComponent* root_to_draw, design::DesignDocument& doc, int tab_id,
                         designer::SelectionManager* selection, std::optional<PropertiesState>& out_selected,
                         const std::string& project_root) {
    const std::string selected_node_id = CurrentSelectedNodeId(selection);
    if (selected_node_id.empty()) return 0.0f;
    std::vector<BreadcrumbSegment> path;
    if (!CollectBreadcrumbPath(root_to_draw, selected_node_id, path)) return 0.0f;

    for (size_t i = 0; i < path.size(); ++i) {
        if (i > 0) {
            ImGui::SameLine(0.0f, 4.0f);
            ImGui::TextDisabled(">");
            ImGui::SameLine(0.0f, 4.0f);
        }
        ImGui::PushID(static_cast<int>(i));
        const bool is_last = (i + 1 == path.size());
        ImGui::BeginDisabled(is_last);
        if (ImGui::SmallButton(path[i].label.c_str())) {
            SelectNode(selection, path[i].node_id);
            if (avalang::ui::IComponent* real = design::FindNodeById(doc.Root(), path[i].node_id)) {
                out_selected = ToPropertiesState(real, true, tab_id, doc, project_root);
            }
        }
        ImGui::EndDisabled();
        ImGui::PopID();
    }
    return ImGui::GetFrameHeightWithSpacing();
}

struct DialogTrayEntry {
    std::string label;
    std::string node_id;
};

void CollectDialogNodes(avalang::ui::IComponent* node, std::vector<DialogTrayEntry>& out) {
    for (avalang::ui::IComponent* child : node->Children()) {
        if (IsDialogNode(child)) {
            const std::string child_type = LowerAscii(child->TypeName());
            const std::string child_id_prop = GetNodeIdProp(child);
            std::string label = child_id_prop.empty() ? child_type : (child_type + " (" + child_id_prop + ")");
            out.push_back({label, child->NodeId()});
        }
        CollectDialogNodes(child, out);
    }
}

float DrawDialogTray(avalang::ui::IComponent* root_to_draw, design::DesignDocument& doc, int tab_id,
                      designer::SelectionManager* selection, std::optional<PropertiesState>& out_selected,
                      const std::string& project_root) {
    std::vector<DialogTrayEntry> dialogs;
    CollectDialogNodes(root_to_draw, dialogs);
    if (dialogs.empty()) return 0.0f;

    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%s", util::Tr("canvas.dialogs_label").c_str());
    for (size_t i = 0; i < dialogs.size(); ++i) {
        ImGui::SameLine();
        ImGui::PushID(static_cast<int>(i));
        const bool is_selected = IsNodeSelected(selection, dialogs[i].node_id);
        if (is_selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        }
        if (ImGui::SmallButton(dialogs[i].label.c_str())) {
            SelectNode(selection, dialogs[i].node_id);
            if (avalang::ui::IComponent* real = design::FindNodeById(doc.Root(), dialogs[i].node_id)) {
                out_selected = ToPropertiesState(real, true, tab_id, doc, project_root);
            }
        }
        if (is_selected) {
            ImGui::PopStyleColor();
        }
        ImGui::PopID();
    }
    return ImGui::GetFrameHeightWithSpacing();
}

void DrawCanvasDeleteConfirmPopup(design::DesignDocument& doc, int tab_id,
                                   designer::CommandManager* command_manager,
                                   designer::SelectionManager* selection,
                                   std::optional<PropertiesState>& out_selected) {
    if (g_canvas_delete_request.tab_id != tab_id) return;

    const std::string delete_title = util::Tr("canvas.delete_title") + "##CanvasDeleteConfirm";
    if (g_canvas_delete_request.open) {
        ImGui::OpenPopup(delete_title.c_str());
        g_canvas_delete_request.open = false;
    }
    ImGui::SetNextWindowSize(ImVec2(360.0f, 0.0f));
    if (ImGui::BeginPopupModal(delete_title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        avalang::ui::IComponent* node = design::FindNodeById(doc.Root(), g_canvas_delete_request.node_id);
        std::string label = "this node";
        if (node) {
            label = LowerAscii(node->TypeName());
            const std::string node_id_prop = GetNodeIdProp(node);
            if (!node_id_prop.empty()) label += " (" + node_id_prop + ")";
        }
        ImGui::TextWrapped("%s", TrFormat("canvas.delete_confirm", label).c_str());
        ImGui::TextDisabled("%s", util::Tr("explorer.delete_undone").c_str());
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 8.0f));

        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float button_w = (ImGui::GetContentRegionAvail().x - spacing) / 2.0f;

        ImGui::PushStyleColor(ImGuiCol_Button, palette::FromHex(palette::kError, 0.75f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, palette::FromHex(palette::kError, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, palette::FromHex(0xc93b3b));
        if (ImGui::Button(util::Tr("explorer.delete").c_str(), ImVec2(button_w, 0.0f))) {
            if (designer::ExecuteRemoveComponent(command_manager, doc, selection, g_canvas_delete_request.node_id)) {
                if (out_selected && (out_selected->selected_node_id == g_canvas_delete_request.node_id ||
                                      CurrentSelectedNodeId(selection).empty())) {
                    out_selected.reset();
                }
            }
            g_canvas_delete_request = {};
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(3);
        ImGui::SameLine(0.0f, spacing);

        if (ImGui::Button(util::Tr("common.cancel").c_str(), ImVec2(button_w, 0.0f))) {
            g_canvas_delete_request = {};
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void HandleCanvasExtractRequest(design::DesignDocument& doc, int tab_id,
                                designer::CommandManager* command_manager,
                                designer::SelectionManager* selection,
                                std::optional<PropertiesState>& out_selected,
                                const std::string& project_root, LogBridge* log_bridge) {
    if (g_canvas_extract_request.tab_id != tab_id || !g_canvas_extract_request.open) {
        return;
    }
    g_canvas_extract_request.open = false;
    const std::string node_id = g_canvas_extract_request.node_id;
    g_canvas_extract_request = {};

    std::string error;
    const std::string created =
        designer::ExecuteExtractComponent(command_manager, doc, selection, node_id, project_root, &error);
    if (!created.empty()) {
        SelectNode(selection, created);
        if (out_selected) {
            out_selected.reset();
        }
        return;
    }
    if (!error.empty() && log_bridge != nullptr) {
        log_bridge->Log("[designer_canvas] extract failed: " + error);
    }
}

}

std::optional<PropertiesState> DrawDesignerCanvas(design::DesignDocument& doc, ImVec2 size,
                                                   const std::string& project_root, int tab_id,
                                                   std::string* out_generated_handler,
                                                   LogBridge* log_bridge) {
    std::optional<PropertiesState> selected;
    if (out_generated_handler) out_generated_handler->clear();
    if (!doc.tree || !doc.Root()) return selected;

    AvaVM* state_vm = nullptr;
    std::unordered_map<std::string, std::string>* eval_cache = nullptr;
    avalang::ui::IComponent* root_to_draw = doc.Root();

    DesignerVmCacheEntry* cache_entry_for_live_render = nullptr;
    studio::design::LiveRenderResult local_live_render;
    std::unique_ptr<avalang::ui::ImGuiRenderer> local_imgui_renderer;
    avalang::ui::theme::ProjectStyleSheet local_project_styles;
    bool tree_state_rebuilt_this_frame = false;

    if (tab_id >= 0) {
        DesignerVmCacheEntry& entry = g_designer_vm_cache[tab_id];
        const bool needs_rebuild = entry.vm == nullptr || entry.last_dirty != doc.dirty ||
                                    entry.last_revision != doc.revision ||
                                    entry.cached_project_root != project_root;
        tree_state_rebuilt_this_frame = needs_rebuild;
        if (needs_rebuild) {
            if (entry.vm) {
                design::ReleasePreviewInstance(entry.vm);
                ava_vm_destroy(entry.vm);
            }
            entry.vm = design::BuildStateVM(doc);
            design::BindCodeBehind(entry.vm, doc);
            entry.last_dirty = doc.dirty;
            entry.last_revision = doc.revision;
            entry.eval_cache.clear();
            entry.cached_project_root = project_root;
            entry.project_styles = avalang::ui::theme::LoadProjectStyleOverrides(project_root);
        }
        state_vm = entry.vm;
        eval_cache = &entry.eval_cache;
        cache_entry_for_live_render = &entry;
    } else {
        state_vm = design::BuildStateVM(doc);
        design::BindCodeBehind(state_vm, doc);
        local_project_styles = avalang::ui::theme::LoadProjectStyleOverrides(project_root);
    }

    designer::CommandManager* command_manager =
        cache_entry_for_live_render != nullptr ? &cache_entry_for_live_render->command_manager : nullptr;
    designer::SelectionManager* selection =
        cache_entry_for_live_render != nullptr ? &cache_entry_for_live_render->selection_manager : nullptr;
    const avalang::ui::theme::ProjectStyleSheet* project_styles =
        cache_entry_for_live_render != nullptr ? &cache_entry_for_live_render->project_styles : &local_project_styles;
    const designer::DesignerVisualState visual_state = cache_entry_for_live_render != nullptr
                                                             ? cache_entry_for_live_render->visual_state
                                                             : designer::DesignerVisualState::Default;

    const bool is_design_mode =
        cache_entry_for_live_render == nullptr || cache_entry_for_live_render->canvas_mode == designer::CanvasMode::Design;

    bool view_reset_requested = false;
    const float device_bar_height = DrawDevicePresetBar(tab_id, cache_entry_for_live_render, &view_reset_requested);
    const float breadcrumb_height =
        is_design_mode ? DrawBreadcrumbBar(root_to_draw, doc, tab_id, selection, selected, project_root) : 0.0f;
    const float dialog_tray_height =
        is_design_mode ? DrawDialogTray(root_to_draw, doc, tab_id, selection, selected, project_root) : 0.0f;
    ImVec2 canvas_size = size;
    const float reserved_height = device_bar_height + breadcrumb_height + dialog_tray_height;
    if (reserved_height > 0.0f) {
        canvas_size.y = std::max(size.y - reserved_height, 1.0f);
    }
    if (cache_entry_for_live_render != nullptr && cache_entry_for_live_render->device_preset_index > 0) {
        const designer::DeviceProfile& preset =
            designer::DeviceProfiles()[cache_entry_for_live_render->device_preset_index];
        canvas_size.x = static_cast<float>(preset.size.width);
        canvas_size.y = static_cast<float>(preset.size.height);

        const float avail_width = ImGui::GetContentRegionAvail().x;
        if (avail_width > canvas_size.x) {
            const float indent = (avail_width - canvas_size.x) * 0.5f;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);
        }
    }

    ImU32 page_bg = IM_COL32(0xFF, 0xFF, 0xFF, 0xFF);
    if (const auto* bg_prop = root_to_draw != nullptr ? root_to_draw->GetProperty("backgroundColor") : nullptr) {
        if (bg_prop->Type() == avalang::ui::PropertyType::String) {
            if (const std::optional<ImU32> parsed =
                    TryHexToImU32(std::optional<std::string>(bg_prop->AsString()))) {
                page_bg = *parsed;
            }
        }
    }
    ImGui::PushStyleColor(ImGuiCol_ChildBg, kCanvasBackdropColor);
    ImGui::PushStyleColor(ImGuiCol_Border, kPageBorderColor);
    ImGui::BeginChild("##DesignerCanvas", canvas_size, true, ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::PopStyleColor(2);

    if (view_reset_requested) {
        ImGui::SetScrollX(0.0f);
        ImGui::SetScrollY(0.0f);
    }
    if (cache_entry_for_live_render != nullptr) {
        cache_entry_for_live_render->viewport.SetPan(-static_cast<double>(ImGui::GetScrollX()),
                                                       -static_cast<double>(ImGui::GetScrollY()));
    }

    if (ImGui::IsWindowHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f)) {
        const ImVec2 delta = ImGui::GetIO().MouseDelta;
        if (cache_entry_for_live_render != nullptr) {
            designer::PanTool::Pan(&cache_entry_for_live_render->viewport, -static_cast<double>(delta.x),
                                    -static_cast<double>(delta.y));
        }
        ImGui::SetScrollX(ImGui::GetScrollX() - delta.x);
        ImGui::SetScrollY(ImGui::GetScrollY() - delta.y);
    }

    if (ImGui::IsWindowHovered() && ImGui::GetIO().KeyCtrl && cache_entry_for_live_render != nullptr) {
        const float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            const double factor = wheel > 0.0f ? 1.1 : 1.0 / 1.1;
            const ImVec2 mouse = ImGui::GetMousePos();
            designer::ZoomTool::ZoomBy(&cache_entry_for_live_render->viewport, factor,
                                        designer::LayoutPoint{static_cast<double>(mouse.x),
                                                               static_cast<double>(mouse.y)});
        }
    }

    const std::string active_selected_node_id = CurrentSelectedNodeId(selection);
    if (is_design_mode && ImGui::IsWindowFocused() && !active_selected_node_id.empty() &&
        active_selected_node_id != doc.Root()->NodeId() && ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
        g_canvas_delete_request = {true, tab_id, active_selected_node_id};
    }

    if (command_manager != nullptr && ImGui::IsWindowFocused()) {
        ShortcutRegistry& shortcuts = ShortcutRegistry::Instance();
        if (shortcuts.Pressed(ShortcutId::Undo, true)) {
            command_manager->Undo();
        } else if (shortcuts.Pressed(ShortcutId::Redo, true)) {
            command_manager->Redo();
        }
    }

    const ImVec2 avail_before_reserve = ImGui::GetContentRegionAvail();
    ImGui::Dummy(ImVec2(std::max(avail_before_reserve.x, 1.0f), kCanvasTopReserve));

    const bool device_preset_active =
        cache_entry_for_live_render != nullptr && cache_entry_for_live_render->device_preset_index > 0;
    const float page_pad = device_preset_active ? 0.0f : kPageOuterPad;

    const ImVec2 raw_origin = ImGui::GetCursorScreenPos();
    const ImVec2 raw_avail = ImGui::GetContentRegionAvail();
    const ImVec2 origin(raw_origin.x + page_pad, raw_origin.y + page_pad);
    const Rect canvas_rect{0.0f, 0.0f, std::max(raw_avail.x - 2.0f * page_pad, 1.0f),
                            std::max(raw_avail.y - 2.0f * page_pad, 1.0f)};

    {
        ImDrawList* page_draw_list = ImGui::GetWindowDrawList();
        const ImVec2 page_p0 = origin;
        const ImVec2 page_p1(origin.x + canvas_rect.w, origin.y + canvas_rect.h);
        page_draw_list->AddRectFilled(ImVec2(page_p0.x + 5.0f, page_p0.y + 6.0f),
                                       ImVec2(page_p1.x + 5.0f, page_p1.y + 6.0f), kPageShadowColor, 3.0f);
        page_draw_list->AddRectFilled(page_p0, page_p1, page_bg, 2.0f);
        page_draw_list->AddRect(page_p0, page_p1, kPageBorderColor, 2.0f, 0, 1.0f);
    }

    studio::design::LiveRenderResult* live_render = nullptr;
    avalang::ui::ImGuiRenderer* live_render_renderer = nullptr;
    {
        const int vw = static_cast<int>(canvas_rect.w);
        const int vh = static_cast<int>(canvas_rect.h);

        // The live render paints text from the render tree, which used to fall
        // back to the raw property source (`"status: " + count`) because it had
        // no evaluator. Give it the same evaluator the property rows use, backed
        // by the design-time state VM. Results are memoized in eval_cache (keys
        // prefixed with \x1d so they cannot collide with the per-node keys used
        // by DrawNode); the cache is cleared whenever a handler changes state.
        const studio::design::TextEvaluator live_eval = [state_vm, eval_cache](const std::string& raw) {
            if (state_vm == nullptr) return raw;
            if (eval_cache == nullptr) return design::EvalPropertyExpr(state_vm, raw);
            const std::string key = std::string(1, '\x1d') + raw;
            const auto hit = eval_cache->find(key);
            if (hit != eval_cache->end()) return hit->second;
            std::string value = design::EvalPropertyExpr(state_vm, raw);
            (*eval_cache)[key] = value;
            return value;
        };

        if (cache_entry_for_live_render != nullptr) {
            DesignerVmCacheEntry& entry = *cache_entry_for_live_render;
            const bool live_render_stale = !entry.live_render.ok || entry.live_render_w != vw ||
                                            entry.live_render_h != vh || tree_state_rebuilt_this_frame ||
                                            entry.live_render_mode != entry.canvas_mode ||
                                            entry.live_render_dirty;
            if (live_render_stale) {
                avalang::ui::ComponentTree* tree_for_live_render = doc.tree.get();
                if (entry.canvas_mode == designer::CanvasMode::Preview) {
                    entry.preview_resolved_tree = studio::design::ResolvePreviewTree(doc, project_root);
                    if (entry.preview_resolved_tree) {
                        tree_for_live_render = entry.preview_resolved_tree.get();
                    }
                } else {
                    entry.preview_resolved_tree.reset();
                }
                entry.live_render = studio::design::BuildLiveRender(tree_for_live_render, vw, vh,
                                                                       doc.extends, project_root, live_eval);
                entry.live_render_dirty = false;
                entry.live_render_w = vw;
                entry.live_render_h = vh;
                entry.live_render_mode = entry.canvas_mode;
                entry.imgui_renderer = std::make_unique<avalang::ui::ImGuiRenderer>(vw, vh);
                entry.animation_controller = entry.live_render.ok
                    ? avalang::ui::animation::AnimationController::Create(entry.live_render.sceneGraph.get())
                    : nullptr;
            }
            live_render = &entry.live_render;
            live_render_renderer = entry.imgui_renderer.get();
        } else {
            local_live_render = studio::design::BuildLiveRender(doc.tree.get(), vw, vh,
                                                                    doc.extends, project_root, live_eval);
            local_imgui_renderer = std::make_unique<avalang::ui::ImGuiRenderer>(vw, vh);
            live_render = &local_live_render;
            live_render_renderer = local_imgui_renderer.get();
        }
    }

    std::string* last_logged_live_render_error = cache_entry_for_live_render != nullptr
                                                      ? &cache_entry_for_live_render->last_logged_live_render_error
                                                      : &g_uncached_last_logged_live_render_error;
    if (!live_render->ok) {
        if (log_bridge != nullptr && *last_logged_live_render_error != live_render->error) {
            log_bridge->Log("[designer_canvas] BuildLiveRender fallo: " + live_render->error);
        }
        *last_logged_live_render_error = live_render->error;
    } else {
        last_logged_live_render_error->clear();
    }

    if (!is_design_mode && cache_entry_for_live_render != nullptr && cache_entry_for_live_render->animation_controller) {
        cache_entry_for_live_render->animation_controller->Update(ImGui::GetIO().DeltaTime);
    }

    bool live_render_painted = false;
    if (live_render->ok && live_render->sceneGraph && live_render_renderer) {
        live_render_renderer->SetTarget(ImGui::GetWindowDrawList(), origin);
        avalang::ui::RenderCommandSink sink;

        // Preview mode only: feed real mouse state into the walker so
        // Button/CheckBox/RadioButton/ComboBox get the same built-in
        // hover/pressed color feedback the native (Gdi) host gets from
        // SceneCommandWalker -- this call previously always passed
        // interactive=nullptr, so isHovered()/isPressed() were always
        // false here and controls never looked pressed, regardless of
        // ApplyBuiltinStateFeedback existing at all. Design mode keeps
        // nullptr: that surface uses its own InvisibleButton-based hit
        // testing for selection and shouldn't also tint the control.
        avalang::ui::InteractiveState interactive;
        const bool use_interactive = !is_design_mode;
        if (use_interactive) {
            const ImVec2 mouse = ImGui::GetMousePos();
            const bool canvas_hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
            if (canvas_hovered) {
                interactive.pointerX = static_cast<int>(mouse.x - origin.x);
                interactive.pointerY = static_cast<int>(mouse.y - origin.y);
            } else {
                // Keep the pointer off-canvas so no control's rect can
                // match while the mouse is elsewhere in the app.
                interactive.pointerX = -1000000;
                interactive.pointerY = -1000000;
            }
            interactive.pointerDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
        }

        avalang::ui::SceneCommandWalker::Walk(*live_render->sceneGraph, sink, *live_render_renderer,
                                               use_interactive ? &interactive : nullptr);
        live_render_painted = true;
    }

    if (is_design_mode && cache_entry_for_live_render != nullptr &&
        cache_entry_for_live_render->device_preset_index > 0) {
        const designer::DeviceProfile& preset =
            designer::DeviceProfiles()[cache_entry_for_live_render->device_preset_index];
        const bool has_safe_area = preset.safeArea.top != 0.0f || preset.safeArea.bottom != 0.0f ||
                                    preset.safeArea.left != 0.0f || preset.safeArea.right != 0.0f;
        if (has_safe_area) {
            const designer::LayoutRect device_rect{0.0, 0.0, preset.size.width, preset.size.height};
            const designer::LayoutRect safe_rect = designer::ApplySafeArea(device_rect, preset.safeArea);
            const ImVec2 safe_p0(origin.x + static_cast<float>(safe_rect.x), origin.y + static_cast<float>(safe_rect.y));
            const ImVec2 safe_p1(safe_p0.x + static_cast<float>(safe_rect.width),
                                  safe_p0.y + static_cast<float>(safe_rect.height));
            DrawDashedRect(ImGui::GetWindowDrawList(), safe_p0, safe_p1,
                           palette::U32FromHex(palette::kTextDisabled));
        }
    }

    if (!live_render->ok) {
        ImGui::TextColored(palette::FromHex(palette::kError), "%s",
                            TrFormat("canvas.render_error", live_render->error).c_str());
        ImGui::TextDisabled("%s", util::Tr("canvas.render_error_hint").c_str());
        ImGui::Separator();
    }

    const std::unordered_map<std::string, avalang::ui::LayoutRect>* uid_to_rect =
        live_render_painted ? &live_render->nodeIdToRect : nullptr;
    if (cache_entry_for_live_render != nullptr) {
        if (uid_to_rect != nullptr) {
            cache_entry_for_live_render->surface_layout.AdoptRects(*uid_to_rect);
        } else {
            cache_entry_for_live_render->surface_layout.Clear();
        }
    }

    const bool surface_picking_active = is_design_mode && cache_entry_for_live_render != nullptr && live_render_painted;
    std::string surface_hovered_node_id;
    if (surface_picking_active) {
        surface_hovered_node_id =
            PickSurfaceNode(doc, cache_entry_for_live_render->surface_layout, origin);
        cache_entry_for_live_render->surface_hovered_node_id = surface_hovered_node_id;
    } else if (cache_entry_for_live_render != nullptr) {
        cache_entry_for_live_render->surface_hovered_node_id.clear();
    }

    if (selection) {
        if (surface_picking_active && !surface_hovered_node_id.empty()) {
            selection->SetHovered(surface_hovered_node_id);
        } else {
            selection->ClearHovered();
        }
    }

    const bool overlay_driven = is_design_mode && cache_entry_for_live_render != nullptr && selection != nullptr;
    if (cache_entry_for_live_render != nullptr) {
        cache_entry_for_live_render->decorations.clear();
    }

    std::vector<std::string> missing_rect_uids;
    std::string imgui_hovered_node_id;
    std::string click_reroute;
    const ImVec2 canvas_min = origin;
    const ImVec2 canvas_max(origin.x + canvas_rect.w, origin.y + canvas_rect.h);
    DrawNode(root_to_draw, origin, doc, selected, tab_id, command_manager, selection, out_generated_handler,
             state_vm, eval_cache, project_root, live_render_painted, project_styles, visual_state, uid_to_rect,
             &missing_rect_uids, surface_picking_active ? &surface_hovered_node_id : nullptr, &imgui_hovered_node_id,
             &click_reroute, overlay_driven ? &cache_entry_for_live_render->decorations : nullptr, is_design_mode,
             0.0f, 0, canvas_min, canvas_max);

    if (g_state_changed_by_handler) {
        g_state_changed_by_handler = false;
        if (cache_entry_for_live_render != nullptr) {
            cache_entry_for_live_render->live_render_dirty = true;
        }
    }

    if (cache_entry_for_live_render != nullptr && !click_reroute.empty() && log_bridge != nullptr &&
        cache_entry_for_live_render->last_logged_click_reroute != click_reroute) {
        log_bridge->Log("[designer_canvas] clic reasignado por designer::HitTest: " + click_reroute);
        cache_entry_for_live_render->last_logged_click_reroute = click_reroute;
    }

    if (cache_entry_for_live_render != nullptr) {
        if (selection != nullptr) {
            const std::unordered_map<std::string, NodeDecoration>& decorations =
                cache_entry_for_live_render->decorations;
            const designer::OverlayRectResolver resolver = [&decorations](const designer::NodeId& id,
                                                                            designer::OverlayItem& item) {
                const auto it = decorations.find(id);
                if (it == decorations.end()) return false;
                item.rect = it->second.rect;
                item.compact = it->second.compact;
                return true;
            };
            const std::vector<designer::OverlayItem> overlay_items =
                designer::BuildOverlay(*selection, cache_entry_for_live_render->surface_layout, resolver);

            if (overlay_driven) {
                ImDrawList* overlay_draw_list = ImGui::GetWindowDrawList();
                for (const designer::OverlayItem& item : overlay_items) {
                    const ImVec2 item_p0(origin.x + static_cast<float>(item.rect.x),
                                          origin.y + static_cast<float>(item.rect.y));
                    const ImVec2 item_p1(item_p0.x + static_cast<float>(item.rect.width),
                                          item_p0.y + static_cast<float>(item.rect.height));
                    const ImU32 color = (item.kind == designer::OverlayItemKind::Hover) ? kHoverBorderColor
                                                                                         : kSelectionBorderColor;
                    DrawSelectionRing(overlay_draw_list, item_p0, item_p1, color);
                }

                const designer::NodeId primary_id = selection->Primary();
                if (!primary_id.empty() && cache_entry_for_live_render->surface_layout.HasRect(primary_id)) {
                    if (avalang::ui::IComponent* primary_node = design::FindNodeById(doc.Root(), primary_id)) {
                        const designer::LayoutRect primary_rect =
                            cache_entry_for_live_render->surface_layout.RectOf(primary_id);
                        const designer::EdgeInsets padding =
                            avalang::ui::layout::ReadEdgeInsets(primary_node, "padding");
                        const designer::EdgeInsets margin =
                            avalang::ui::layout::ReadEdgeInsets(primary_node, "margin");
                        const designer::InsetOverlay insets =
                            designer::ComputeInsetOverlay(primary_rect, padding, margin);
                        if (insets.hasMargin) {
                            DrawInsetBand(overlay_draw_list, origin, insets.marginRect, primary_rect,
                                          palette::U32FromHex(palette::kWarning, 0.08f),
                                          palette::U32FromHex(palette::kWarning, 0.35f));
                        }
                        if (insets.hasPadding) {
                            DrawInsetBand(overlay_draw_list, origin, primary_rect, insets.paddingRect,
                                          palette::U32FromHex(palette::kSuccess, 0.08f),
                                          palette::U32FromHex(palette::kSuccess, 0.35f));
                        }
                    }
                }

                if (g_resize_tool.IsActive() && !g_active_guides.empty()) {
                    const ImU32 guide_color = palette::U32FromHex(palette::kInfo, 0.85f);
                    for (const designer::AlignmentGuide& guide : g_active_guides) {
                        if (guide.orientation == designer::GuideOrientation::Vertical) {
                            const ImVec2 gp0(origin.x + static_cast<float>(guide.position),
                                              origin.y + static_cast<float>(guide.spanStart));
                            const ImVec2 gp1(origin.x + static_cast<float>(guide.position),
                                              origin.y + static_cast<float>(guide.spanEnd));
                            overlay_draw_list->AddLine(gp0, gp1, guide_color, 1.0f);
                        } else {
                            const ImVec2 gp0(origin.x + static_cast<float>(guide.spanStart),
                                              origin.y + static_cast<float>(guide.position));
                            const ImVec2 gp1(origin.x + static_cast<float>(guide.spanEnd),
                                              origin.y + static_cast<float>(guide.position));
                            overlay_draw_list->AddLine(gp0, gp1, guide_color, 1.0f);
                        }
                    }
                }
            }
            const bool selection_without_overlay = !selection->IsEmpty() && overlay_items.empty();
            std::string* last_logged_overlay_gap = &cache_entry_for_live_render->last_logged_overlay_gap;
            if (selection_without_overlay) {
                const std::string message =
                    "designer::BuildOverlay no encontro rects para la seleccion actual "
                    "(surface_layout desalineado con uid_to_rect)";
                if (log_bridge != nullptr && *last_logged_overlay_gap != message) {
                    log_bridge->Log("[designer_canvas] " + message);
                }
                *last_logged_overlay_gap = message;
            } else {
                last_logged_overlay_gap->clear();
            }
        }
    }

    std::string* last_logged_missing_rects = cache_entry_for_live_render != nullptr
                                                  ? &cache_entry_for_live_render->last_logged_missing_rects
                                                  : &g_uncached_last_logged_missing_rects;
    if (!missing_rect_uids.empty()) {
        std::string joined;
        for (size_t i = 0; i < missing_rect_uids.size(); ++i) {
            if (i != 0) joined += ", ";
            joined += missing_rect_uids[i];
        }
        if (log_bridge != nullptr && *last_logged_missing_rects != joined) {
            log_bridge->Log("[designer_canvas] " + std::to_string(missing_rect_uids.size()) +
                             " nodo(s) sin entrada en uid_to_rect (no se dibujaron): " + joined);
        }
        *last_logged_missing_rects = joined;
        ImGui::TextColored(palette::FromHex(palette::kWarning), "%s",
                            TrFormat("canvas.missing_layout_nodes",
                                     std::to_string(missing_rect_uids.size()))
                                .c_str());
    } else {
        last_logged_missing_rects->clear();
    }

    const std::string current_selected_node_id = CurrentSelectedNodeId(selection);
    if (!current_selected_node_id.empty()) {
        if (avalang::ui::IComponent* found = design::FindNodeById(doc.Root(), current_selected_node_id)) {
            selected = ToPropertiesState(found, true, tab_id, doc, project_root);
        } else {
            ClearSelectedNode(selection);
            selected.reset();
        }
    } else {
        selected.reset();
    }

    ImGui::EndChild();

    DrawCanvasDeleteConfirmPopup(doc, tab_id, command_manager, selection, selected);

    HandleCanvasExtractRequest(doc, tab_id, command_manager, selection, selected, project_root, log_bridge);

    if (tab_id < 0 && state_vm) {
        design::ReleasePreviewInstance(state_vm);
        ava_vm_destroy(state_vm);
    }

    return selected;
}

void InvalidateDesignerVmCache(int tab_id) {
    auto it = g_designer_vm_cache.find(tab_id);
    if (it == g_designer_vm_cache.end()) return;

    DesignerVmCacheEntry& entry = it->second;
    if (entry.vm) {
        design::ReleasePreviewInstance(entry.vm);
        ava_vm_destroy(entry.vm);
        entry.vm = nullptr;
    }
    entry.last_dirty = false;
    entry.eval_cache.clear();
    entry.cached_project_root.clear();
    entry.live_render = studio::design::LiveRenderResult{};
    entry.imgui_renderer.reset();
    entry.live_render_w = -1;
    entry.live_render_h = -1;
    entry.last_logged_live_render_error.clear();
    entry.last_logged_missing_rects.clear();
    entry.last_logged_overlay_gap.clear();
    entry.last_logged_click_reroute.clear();
    entry.decorations.clear();
    entry.surface_hovered_node_id.clear();
    entry.surface_layout.Clear();
}

void ReleaseDesignerTabState(int tab_id) {
    auto it = g_designer_vm_cache.find(tab_id);
    if (it == g_designer_vm_cache.end()) return;
    if (it->second.vm) {
        design::ReleasePreviewInstance(it->second.vm);
        ava_vm_destroy(it->second.vm);
    }
    g_designer_vm_cache.erase(it);
}

void ClearDesignerCommandHistory(int tab_id) {
    auto it = g_designer_vm_cache.find(tab_id);
    if (it == g_designer_vm_cache.end()) return;
    it->second.command_manager.Clear();
}

void ClearDesignerSelection(int tab_id) {
    auto it = g_designer_vm_cache.find(tab_id);
    if (it == g_designer_vm_cache.end()) return;
    it->second.selection_manager.Clear();
    it->second.selection_manager.ClearHovered();
    it->second.selection_manager.ClearFocused();
}

AvaVM* GetDesignerStateVM(int tab_id) {
    auto it = g_designer_vm_cache.find(tab_id);
    if (it == g_designer_vm_cache.end()) return nullptr;
    return it->second.vm;
}

designer::CommandManager* GetDesignerCommandManager(int tab_id) {
    auto it = g_designer_vm_cache.find(tab_id);
    if (it == g_designer_vm_cache.end()) return nullptr;
    return &it->second.command_manager;
}

designer::SelectionManager* GetDesignerSelectionManager(int tab_id) {
    auto it = g_designer_vm_cache.find(tab_id);
    if (it == g_designer_vm_cache.end()) return nullptr;
    return &it->second.selection_manager;
}

designer::LayoutCore* GetDesignerSurfaceLayout(int tab_id) {
    auto it = g_designer_vm_cache.find(tab_id);
    if (it == g_designer_vm_cache.end()) return nullptr;
    return &it->second.surface_layout;
}

designer::DesignerViewport* GetDesignerViewport(int tab_id) {
    auto it = g_designer_vm_cache.find(tab_id);
    if (it == g_designer_vm_cache.end()) return nullptr;
    return &it->second.viewport;
}

std::string GetDesignerHoveredNode(int tab_id) {
    auto it = g_designer_vm_cache.find(tab_id);
    if (it == g_designer_vm_cache.end()) return std::string();
    return it->second.surface_hovered_node_id;
}

designer::CanvasMode GetDesignerCanvasMode(int tab_id) {
    auto it = g_designer_vm_cache.find(tab_id);
    if (it == g_designer_vm_cache.end()) return designer::CanvasMode::Design;
    return it->second.canvas_mode;
}

avalang::ui::animation::AnimationController* GetDesignerAnimationController(int tab_id) {
    auto it = g_designer_vm_cache.find(tab_id);
    if (it == g_designer_vm_cache.end()) return nullptr;
    if (it->second.canvas_mode != designer::CanvasMode::Preview) return nullptr;
    return it->second.animation_controller.get();
}

std::vector<designer::OverlayItem> GetDesignerOverlay(int tab_id) {
    auto it = g_designer_vm_cache.find(tab_id);
    if (it == g_designer_vm_cache.end()) return {};

    const std::unordered_map<std::string, NodeDecoration>& decorations = it->second.decorations;
    const designer::OverlayRectResolver resolver = [&decorations](const designer::NodeId& id,
                                                                    designer::OverlayItem& item) {
        const auto found = decorations.find(id);
        if (found == decorations.end()) return true;
        item.rect = found->second.rect;
        item.compact = found->second.compact;
        return true;
    };
    return designer::BuildOverlay(it->second.selection_manager, it->second.surface_layout, resolver);
}

}