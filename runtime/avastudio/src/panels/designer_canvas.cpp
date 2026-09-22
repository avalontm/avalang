#include "panels/designer_canvas.h"

#include <algorithm>
#include <cmath>
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
#include "design/injected_properties.h"
#include "design/live_render_bridge.h"
#include "design/state_eval.h"
#include "commands/RenderCommandSink.h"
#include "commands/SceneCommandWalker.h"
#include "components/IComponent.h"
#include "designer/command.h"
#include "designer/device_viewport.h"
#include "designer/document_commands.h"
#include "designer/drop_target.h"
#include "designer/guides.h"
#include "designer/hit_test.h"
#include "designer/layout_core.h"
#include "designer/drop_resolver.h"
#include "designer/node_frame.h"
#include "designer/node_kind.h"
#include "designer/overlay.h"
#include "designer/preview.h"
#include "designer/responsive.h"
#include "designer/responsive_inspector.h"
#include "designer/property_editor.h"
#include "designer/property_catalog.h"
#include "designer/property_grid.h"
#include "designer/selection_manager.h"
#include "designer/surface.h"
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
#include "util/i18n.h"

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

using designer::IsDialogNode;
using designer::LowerAscii;

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

bool g_state_changed_by_handler = false;

void InvokeNodeClickHandler(AvaVM* state_vm, avalang::ui::IComponent* node,
                             std::unordered_map<std::string, std::string>* eval_cache) {
    if (!state_vm || node == nullptr) return;

    std::vector<PropertyRow> events;
    CollectPropertyRows(node, nullptr, &events);
    for (const PropertyRow& ev : events) {
        if (ev.key != "click" || ev.value.empty()) continue;
        std::string handler_error;
        if (design::InvokeHandler(state_vm, ev.value, &handler_error)) {
            g_state_changed_by_handler = true;
            if (eval_cache != nullptr) eval_cache->clear();
        }
        break;
    }
}

struct DesignerVmCacheEntry {
    AvaVM* vm = nullptr;
    bool last_dirty = false;
    int last_revision = -1;
    std::unordered_map<std::string, std::string> eval_cache;
    std::string cached_project_root;

    studio::design::LiveRenderResult live_render;
    std::vector<studio::design::InjectedProperty> design_injected;
    std::unique_ptr<avalang::ui::ImGuiRenderer> imgui_renderer;
    std::unique_ptr<avalang::ui::animation::AnimationController> animation_controller;
    std::unique_ptr<avalang::ui::ComponentTree> preview_resolved_tree;
    int live_render_w = -1;
    int live_render_h = -1;
    designer::CanvasMode live_render_mode = designer::CanvasMode::Design;
    bool live_render_dirty = false;

    int device_preset_index = 0;
    designer::DeviceOrientation device_orientation = designer::DeviceOrientation::Default;
    bool respect_safe_area = true;
    int custom_width = 390;
    int custom_height = 844;

    std::string last_logged_live_render_error;
    std::string last_logged_missing_rects;
    std::string last_logged_overlay_gap;
    std::string pressed_node_id;
    std::string context_menu_node_id;
    std::string surface_hovered_node_id;
    designer::NodeFrameMap frames;

    designer::CommandManager command_manager;
    designer::DesignerSurface surface;
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

bool IsCustomDevice(const DesignerVmCacheEntry& entry) {
    return entry.device_preset_index >= static_cast<int>(designer::DeviceProfiles().size());
}

designer::DeviceProfile ActiveDeviceProfile(const DesignerVmCacheEntry& entry) {
    const std::vector<designer::DeviceProfile>& profiles = designer::DeviceProfiles();
    if (IsCustomDevice(entry)) {
        const designer::LayoutSize size = designer::ClampCustomSize(entry.custom_width, entry.custom_height);
        return designer::MakeCustomProfile("Custom", size.width, size.height);
    }
    const int index = std::clamp(entry.device_preset_index, 0, static_cast<int>(profiles.size()) - 1);
    return profiles[static_cast<size_t>(index)];
}

bool DrawCustomSizeInput(const char* id, int* value) {
    ImGui::SetNextItemWidth(70.0f);
    if (!ImGui::InputInt(id, value, 0, 0, ImGuiInputTextFlags_EnterReturnsTrue)) return false;
    *value = static_cast<int>(std::clamp(static_cast<double>(*value), designer::kMinCustomDimension,
                                          designer::kMaxCustomDimension));
    return true;
}

std::optional<designer::DeviceViewport> FixedDeviceViewport(const DesignerVmCacheEntry* entry) {
    if (entry == nullptr) return std::nullopt;
    const designer::DeviceViewport view = designer::ResolveDeviceViewport(
        ActiveDeviceProfile(*entry), entry->device_orientation, entry->respect_safe_area, designer::LayoutSize{});
    if (!view.fixed) return std::nullopt;
    return view;
}

void DrawDeviceRow(DesignerVmCacheEntry* cache_entry) {
    const std::vector<designer::DeviceProfile>& profiles = designer::DeviceProfiles();
    const int count = static_cast<int>(profiles.size());
    cache_entry->device_preset_index = std::clamp(cache_entry->device_preset_index, 0, count);

    const std::string custom_label = util::Tr("canvas.device_custom");
    const std::string current_label = IsCustomDevice(*cache_entry)
                                          ? custom_label
                                          : profiles[static_cast<size_t>(cache_entry->device_preset_index)].name;
    ImGui::SetNextItemWidth(220.0f);
    if (ImGui::BeginCombo("##device_preset", current_label.c_str())) {
        for (int i = 0; i <= count; ++i) {
            const bool is_selected = (i == cache_entry->device_preset_index);
            const std::string& label = i == count ? custom_label : profiles[static_cast<size_t>(i)].name;
            if (ImGui::Selectable(label.c_str(), is_selected) && !is_selected) {
                cache_entry->device_preset_index = i;
                cache_entry->device_orientation = designer::DeviceOrientation::Default;
            }
            if (is_selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    if (IsCustomDevice(*cache_entry)) {
        ImGui::SameLine();
        DrawCustomSizeInput("##custom_width", &cache_entry->custom_width);
        ImGui::SameLine();
        DrawCustomSizeInput("##custom_height", &cache_entry->custom_height);
    }

    if (designer::SupportsRotation(ActiveDeviceProfile(*cache_entry))) {
        ImGui::SameLine();
        if (ImGui::Button(util::Tr("canvas.rotate_device").c_str())) {
            cache_entry->device_orientation = cache_entry->device_orientation == designer::DeviceOrientation::Default
                                                   ? designer::DeviceOrientation::Rotated
                                                   : designer::DeviceOrientation::Default;
        }
    }

    const std::optional<designer::DeviceViewport> fixed = FixedDeviceViewport(cache_entry);
    if (fixed && designer::HasSafeArea(fixed->insets)) {
        ImGui::SameLine();
        ImGui::Checkbox(util::Tr("canvas.safe_area").c_str(), &cache_entry->respect_safe_area);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", util::Tr("canvas.safe_area_tooltip").c_str());
        }
    }

    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    if (fixed) {
        ImGui::TextDisabled("%d x %d", static_cast<int>(std::lround(fixed->contentRect.width)),
                             static_cast<int>(std::lround(fixed->contentRect.height)));
    } else if (cache_entry->live_render_w > 0) {
        ImGui::TextDisabled("%d x %d", cache_entry->live_render_w, cache_entry->live_render_h);
    }

    const studio::design::LiveRenderResult& live = cache_entry->live_render;
    if (live.ok && live.styles && live.styles->HasAnyResponsiveStyles()) {
        ImGui::SameLine();
        if (const std::optional<uint32_t> active =
                designer::HighestActiveBreakpoint(*live.styles, static_cast<double>(live.viewportWidth))) {
            ImGui::TextDisabled("%s", TrFormat("canvas.breakpoint_active", std::to_string(*active)).c_str());
        } else {
            ImGui::TextDisabled("%s", util::Tr("canvas.breakpoint_base").c_str());
        }
    }
}

void DrawViewToolsRow(DesignerVmCacheEntry* cache_entry, bool* out_view_reset_requested) {
    if (ImGui::Button(util::Tr("canvas.reset_view").c_str())) {
        cache_entry->surface.Viewport().Reset();
        if (out_view_reset_requested) *out_view_reset_requested = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("-##canvas_zoom_out")) {
        designer::ZoomTool::ZoomOut(&cache_entry->surface.Viewport(), designer::LayoutPoint{0.0, 0.0});
    }
    ImGui::SameLine();
    ImGui::Text("%d%%", static_cast<int>(std::lround(cache_entry->surface.Viewport().Zoom() * 100.0)));
    ImGui::SameLine();
    if (ImGui::Button("+##canvas_zoom_in")) {
        designer::ZoomTool::ZoomIn(&cache_entry->surface.Viewport(), designer::LayoutPoint{0.0, 0.0});
    }
    ImGui::SameLine();
    if (ImGui::Button(util::Tr("canvas.zoom_fit").c_str())) {
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const designer::LayoutSize screen_size{static_cast<double>(std::max(avail.x, 1.0f)),
                                                static_cast<double>(std::max(avail.y, 1.0f))};
        designer::LayoutSize content_size = screen_size;
        if (const std::optional<designer::DeviceViewport> fixed = FixedDeviceViewport(cache_entry)) {
            content_size = fixed->deviceSize;
        }
        designer::ZoomTool::FitToScreen(&cache_entry->surface.Viewport(), content_size, screen_size);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", util::Tr("canvas.zoom_render_limitation").c_str());
    }
    ImGui::SameLine();
    const bool is_design_mode = cache_entry->surface.Mode() == designer::CanvasMode::Design;
    if (is_design_mode) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    if (ImGui::Button(util::Tr("canvas.mode_design").c_str())) {
        cache_entry->surface.SetMode(designer::CanvasMode::Design);
    }
    if (is_design_mode) ImGui::PopStyleColor();
    ImGui::SameLine();
    if (!is_design_mode) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    if (ImGui::Button(util::Tr("canvas.mode_preview").c_str())) {
        cache_entry->surface.SetMode(designer::CanvasMode::Preview);
    }
    if (!is_design_mode) ImGui::PopStyleColor();
    ImGui::SameLine();
    DrawVisualStateCombo(cache_entry);
}

float DrawDevicePresetBar(int tab_id, DesignerVmCacheEntry* cache_entry, bool* out_view_reset_requested) {
    if (tab_id < 0 || cache_entry == nullptr) return 0.0f;

    const float start_y = ImGui::GetCursorPosY();
    DrawDeviceRow(cache_entry);
    DrawViewToolsRow(cache_entry, out_view_reset_requested);
    return ImGui::GetCursorPosY() - start_y;
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

ImVec2 FrameMin(const designer::LayoutRect& rect, ImVec2 origin) {
    return ImVec2(origin.x + static_cast<float>(rect.x), origin.y + static_cast<float>(rect.y));
}

ImVec2 FrameMax(const designer::LayoutRect& rect, ImVec2 origin) {
    return ImVec2(origin.x + static_cast<float>(rect.x + rect.width), origin.y + static_cast<float>(rect.y + rect.height));
}

constexpr float kHeaderHeight = 20.0f;

constexpr float kSelectionBorderThickness = 1.5f;
constexpr float kSelectionCornerRadius = 2.5f;
const ImU32 kSelectionBorderColor = palette::U32FromHex(palette::kPrimary, 0.85f);
const ImU32 kHoverBorderColor = palette::U32FromHex(palette::kPrimary, 0.5f);
constexpr float kCanvasTopReserve = static_cast<float>(designer::kContainerChromePadTop);
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
    const designer::LayoutRect base{base_p0.x, base_p0.y, base_p1.x - base_p0.x, base_p1.y - base_p0.y};
    const designer::SelectionBox box = designer::ComputeSelectionBox(base, has_own_padding);
    SelectionBox result;
    result.p0 = ImVec2(static_cast<float>(box.rect.x), static_cast<float>(box.rect.y));
    result.p1 = ImVec2(static_cast<float>(box.rect.x + box.rect.width), static_cast<float>(box.rect.y + box.rect.height));
    result.compact = box.compact;
    return result;
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

    for (const auto& name : node->PropertyNames()) {
        if (known_names.count(name)) continue;
        const auto* value = node->GetProperty(name);
        if (!value) continue;

        designer::PropertyGridRow row;
        if (const designer::PropertyMetadata* declared = designer::FindCatalogProperty(name)) {
            row.metadata = *declared;
        } else {
            row.metadata.name = name;
            row.metadata.type = designer::PropertyTypeName(value->Type());
            row.metadata.category = designer::PropertyCategory::Advanced;
            row.metadata.designerEditor = avalang::ui::IsEventPropertyName(name)
                                              ? designer::PropertyEditorKind::Event
                                              : designer::PropertyEditorKind::String;
        }
        row.value = designer::FormatPropertyValue(*value);
        row.source = ResolvePropertySource(doc, node, name, true, project_root);
        designer::InsertGridRow(sections, std::move(row));
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
        state.addable = designer::ListAddableProperties(node, state.grid);
    }
    return state;
}

std::optional<PropertiesState> ToMultiPropertiesState(design::DesignDocument& doc,
                                                        const std::vector<designer::NodeId>& ids, int tab_id,
                                                        const std::string& project_root) {
    std::vector<avalang::ui::IComponent*> nodes;
    nodes.reserve(ids.size());
    for (const designer::NodeId& id : ids) {
        if (avalang::ui::IComponent* node = design::FindNodeById(doc.Root(), id)) {
            nodes.push_back(node);
        }
    }
    if (nodes.size() < 2) {
        return std::nullopt;
    }

    const std::string common_type = LowerAscii(nodes.front()->TypeName());
    bool mixed_types = false;
    for (avalang::ui::IComponent* node : nodes) {
        if (LowerAscii(node->TypeName()) != common_type) {
            mixed_types = true;
            break;
        }
    }

    PropertiesState state =
        mixed_types ? PropertiesState{} : ToPropertiesState(nodes.front(), true, tab_id, doc, project_root);
    state.mixed_types = mixed_types;
    state.selected_component_type = common_type;
    state.editable = true;
    state.source_tab_id = tab_id;
    state.selected_node_id = nodes.front()->NodeId();
    state.selected_node_ids.clear();
    for (avalang::ui::IComponent* node : nodes) {
        state.selected_node_ids.push_back(node->NodeId());
    }
    return state;
}

std::optional<ImU32> TryHexToImU32(const std::optional<std::string>& hex) {
    if (!hex) return std::nullopt;
    float rgba[4];
    if (!designer::TryParseColorValue(*hex, rgba)) return std::nullopt;
    return ImGui::ColorConvertFloat4ToU32(ImVec4(rgba[0], rgba[1], rgba[2], rgba[3]));
}

bool IsNodeDrawnInCanvas(avalang::ui::IComponent* root, const std::string& node_id) {
    if (root == nullptr) return false;
    if (root->NodeId() == node_id) return true;
    for (avalang::ui::IComponent* child : root->Children()) {
        if (IsDialogNode(child)) continue;
        if (IsNodeDrawnInCanvas(child, node_id)) return true;
    }
    return false;
}

designer::LayoutPoint MouseCanvasPoint(ImVec2 origin) {
    const ImVec2 mouse = ImGui::GetMousePos();
    return designer::LayoutPoint{static_cast<double>(mouse.x - origin.x), static_cast<double>(mouse.y - origin.y)};
}

std::string PickDrawnNode(design::DesignDocument& doc, const designer::DesignerSurface& surface,
                           const designer::LayoutPoint& canvas_point) {
    const designer::NodeId picked = surface.PickCanvas(doc.tree.get(), canvas_point);
    if (picked.empty()) return std::string();
    if (!IsNodeDrawnInCanvas(doc.Root(), picked)) return std::string();
    return picked;
}

std::string PickSurfaceNode(design::DesignDocument& doc, const designer::DesignerSurface& surface, ImVec2 origin) {
    if (!ImGui::IsWindowHovered()) return std::string();
    if (ImGui::GetDragDropPayload() != nullptr) return std::string();
    return PickDrawnNode(doc, surface, MouseCanvasPoint(origin));
}

void DrawNode(avalang::ui::IComponent* node, ImVec2 origin,
              design::DesignDocument& doc,
              designer::CommandManager* command_manager,
              designer::SelectionManager* selection,
              bool live_render_painted,
              const std::unordered_map<std::string, avalang::ui::LayoutRect>* uid_to_rect = nullptr,
              std::vector<std::string>* out_missing_rect_uids = nullptr,
              bool overlay_driven = false, bool design_mode = true,
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

    designer::NodeFrameParams frame_params;
    frame_params.layout = designer::LayoutRect{r.x, r.y, r.w, r.h};
    frame_params.isContainer = is_container;
    frame_params.depth = depth;
    frame_params.offsetY = extra_offset_y;
    frame_params.skipLeafWireframe = live_render_painted && !is_container;
    frame_params.bounds =
        designer::LayoutRect{static_cast<double>(canvas_min.x) - origin.x, static_cast<double>(canvas_min.y) - origin.y,
                              static_cast<double>(canvas_max.x) - canvas_min.x,
                              static_cast<double>(canvas_max.y) - canvas_min.y};
    const designer::NodeFrame frame = designer::ComputeNodeFrame(frame_params);
    const ImVec2 raw_p0 = FrameMin(frame.raw, origin);
    const ImVec2 raw_p1 = FrameMax(frame.raw, origin);
    const ImVec2 p0 = FrameMin(frame.content, origin);
    const ImVec2 p1 = FrameMax(frame.content, origin);
    const ImVec2 chrome_p0 = FrameMin(frame.chrome, origin);
    const ImVec2 chrome_p1 = FrameMax(frame.chrome, origin);

    const bool selected = design_mode && IsNodeSelected(selection, node->NodeId());
    const bool skip_leaf_wireframe = frame_params.skipLeafWireframe;
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    constexpr bool synthetic = false;
    ImGui::PushID(node->NodeId().c_str());

    const ImVec2 base_sel_p0 = is_container ? chrome_p0 : (skip_leaf_wireframe ? raw_p0 : p0);
    const ImVec2 base_sel_p1 = is_container ? chrome_p1 : (skip_leaf_wireframe ? raw_p1 : p1);
    const ImVec2 sel_p0 = FrameMin(frame.selection, origin);
    const ImVec2 sel_p1 = FrameMax(frame.selection, origin);
    const bool compact = frame.compact;

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

    if (selected && !overlay_driven) {
        DrawSelectionRing(draw_list, sel_p0, sel_p1, kSelectionBorderColor);

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

    const bool header_reserves_space = is_container && !live_render_painted;
    const float header_bottom = header_reserves_space ? std::min(p1.y, p0.y + kHeaderHeight) : p0.y;
    const float child_offset_y = extra_offset_y + (header_reserves_space ? kHeaderHeight : 0.0f);
    if (is_container && header_reserves_space) {
        draw_list->AddRectFilled(p0, ImVec2(p1.x, header_bottom), palette::U32FromHex(palette::kBorder, 0.55f), 2.0f,
                                  ImDrawFlags_RoundCornersTop);
        draw_list->AddLine(ImVec2(p0.x, header_bottom), ImVec2(p1.x, header_bottom), palette::U32FromHex(palette::kBorder),
                            1.0f);
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

    ImGui::PopID();

    for (avalang::ui::IComponent* child : node_children) {
        if (IsDialogNode(child)) continue;
        DrawNode(child, origin, doc, command_manager, selection, live_render_painted, uid_to_rect,
                 out_missing_rect_uids, overlay_driven, design_mode, child_offset_y, depth + 1, canvas_min,
                 canvas_max);
    }
}

void DrawResizeHandlesOverlay(design::DesignDocument& doc, DesignerVmCacheEntry& entry, ImVec2 origin) {
    designer::SelectionManager& selection = entry.surface.Selection();
    designer::CommandManager* command_manager = &entry.command_manager;
    designer::LayoutCore& layout = entry.surface.Layout();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    for (const designer::NodeId& node_id : selection.Selected()) {
        avalang::ui::IComponent* node = design::FindNodeById(doc.Root(), node_id);
        if (node == nullptr || !designer::ResizeTool::CanResize(node, doc.Root())) continue;

        const auto frame_it = entry.frames.find(node_id);
        if (frame_it == entry.frames.end() || frame_it->second.compact) continue;
        const designer::NodeFrame& frame = frame_it->second;

        const ImVec2 sel_p0 = FrameMin(frame.selection, origin);
        const ImVec2 sel_p1 = FrameMax(frame.selection, origin);
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

        const designer::LayoutRect node_rect = layout.RectOf(node_id);
        const auto ResizeHandle = [&](const char* str_id, ImVec2 center, ImGuiMouseCursor cursor, bool adjust_x,
                                      bool adjust_y) {
            constexpr float kHitHalf = kHandleHalf + 3.0f;
            ImGui::SetCursorScreenPos(ImVec2(center.x - kHitHalf, center.y - kHitHalf));
            ImGui::InvisibleButton(str_id, ImVec2(kHitHalf * 2.0f, kHitHalf * 2.0f));
            if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
                ImGui::SetMouseCursor(cursor);
            }
            if (ImGui::IsItemActivated()) {
                float start_w = static_cast<float>(frame.base.width);
                float start_h = static_cast<float>(frame.base.height);
                std::vector<PropertyRow> size_props;
                CollectPropertyRows(node, &size_props, nullptr);
                TryGetNumericProperty(size_props, "width", &start_w);
                TryGetNumericProperty(size_props, "height", &start_h);
                g_resize_tool.Begin(node_id, adjust_x, adjust_y, start_w, start_h);
                g_active_guides.clear();
            }
            if (ImGui::IsItemActive() && g_resize_tool.IsDragging(node_id)) {
                const ImVec2 total_delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0f);
                g_resize_tool.UpdateDelta(total_delta.x, total_delta.y, kMinResizeDimension);

                g_active_guides.clear();
                if (avalang::ui::IComponent* parent = design::FindParentOf(doc.Root(), node)) {
                    std::vector<std::pair<designer::NodeId, designer::LayoutRect>> sibling_rects;
                    for (avalang::ui::IComponent* sibling : parent->Children()) {
                        if (sibling == node || !layout.HasRect(sibling->NodeId())) continue;
                        sibling_rects.emplace_back(sibling->NodeId(), layout.RectOf(sibling->NodeId()));
                    }
                    const designer::LayoutRect moving{node_rect.x, node_rect.y, g_resize_tool.PreviewWidth(),
                                                       g_resize_tool.PreviewHeight()};
                    const designer::SnapResult snap = designer::ComputeSnap(moving, sibling_rects, kSnapThreshold);
                    g_resize_tool.ApplySnap(snap.dx, snap.dy);
                    g_active_guides = snap.guides;
                }
            }
            if (ImGui::IsItemDeactivated() && g_resize_tool.IsDragging(node_id)) {
                if (g_resize_tool.Commit(command_manager, doc, &selection)) {
                    doc.dirty = true;
                }
                g_active_guides.clear();
            }
        };
        ImGui::PushID(node_id.c_str());
        ResizeHandle("##resize_se", sel_p1, ImGuiMouseCursor_ResizeNWSE, true, true);
        ResizeHandle("##resize_e", ImVec2(sel_p1.x, mid_y), ImGuiMouseCursor_ResizeEW, true, false);
        ResizeHandle("##resize_s", ImVec2(mid_x, sel_p1.y), ImGuiMouseCursor_ResizeNS, false, true);
        ImGui::PopID();
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

struct CanvasInput {
    design::DesignDocument* doc = nullptr;
    DesignerVmCacheEntry* entry = nullptr;
    AvaVM* state_vm = nullptr;
    std::unordered_map<std::string, std::string>* eval_cache = nullptr;
    std::string* out_generated_handler = nullptr;
    ImVec2 origin;
    ImVec2 size;
    bool design_mode = true;
    int tab_id = -1;
};

std::string NodeDragLabel(avalang::ui::IComponent* node) {
    std::string label = LowerAscii(node->TypeName());
    const std::string id = GetNodeIdProp(node);
    if (!id.empty()) label += " (" + id + ")";
    return label;
}

void DrawDropIndicator(const designer::DropIndicator& indicator, ImVec2 origin) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const ImU32 highlight = palette::U32FromHex(palette::kPrimary);
    const ImVec2 ip0 = FrameMin(indicator.rect, origin);
    const ImVec2 ip1 = FrameMax(indicator.rect, origin);
    if (indicator.isLine) {
        const float mid_y = (ip0.y + ip1.y) * 0.5f;
        draw_list->AddLine(ImVec2(ip0.x, mid_y), ImVec2(ip1.x, mid_y), highlight, 3.0f);
    } else {
        draw_list->AddRect(ip0, ip1, highlight, 2.0f, 0, 3.0f);
    }
}

void DrawInsertMarker(const designer::LayoutRect& marker, ImVec2 origin) {
    ImGui::GetWindowDrawList()->AddRectFilled(FrameMin(marker, origin), FrameMax(marker, origin),
                                               palette::U32FromHex(palette::kPrimary), 1.5f);
}

void ApplyToolboxDrop(const CanvasInput& input, const designer::DropResolution& drop, const std::string& type) {
    design::DesignDocument& doc = *input.doc;
    designer::CommandManager* commands = &input.entry->command_manager;
    designer::SelectionManager* selection = &input.entry->surface.Selection();
    if (drop.zone == design::DropZone::kInto && !drop.anchorId.empty()) {
        designer::InsertTool::InsertRelative(commands, doc, selection, drop.targetId, drop.anchorId,
                                              design::DropZone::kBefore, type);
    } else if (drop.zone == design::DropZone::kInto) {
        designer::InsertTool::InsertInto(commands, doc, selection, drop.targetId, type);
    } else if (avalang::ui::IComponent* target = design::FindNodeById(doc.Root(), drop.targetId)) {
        if (avalang::ui::IComponent* parent = design::FindParentOf(doc.Root(), target)) {
            designer::InsertTool::InsertRelative(commands, doc, selection, parent->NodeId(), drop.targetId, drop.zone,
                                                  type);
        }
    }
}

void ApplyMoveDrop(const CanvasInput& input, const designer::DropResolution& drop, const std::string& moved_id) {
    design::DesignDocument& doc = *input.doc;
    designer::CommandManager* commands = &input.entry->command_manager;
    designer::SelectionManager* selection = &input.entry->surface.Selection();
    if (drop.zone == design::DropZone::kInto && !drop.anchorId.empty()) {
        designer::MoveTool::Execute(commands, doc, selection, moved_id, drop.anchorId, design::DropZone::kBefore);
    } else {
        designer::MoveTool::Execute(commands, doc, selection, moved_id, drop.targetId, drop.zone);
    }
}

void HandleCanvasDrop(const CanvasInput& input) {
    if (!ImGui::BeginDragDropTarget()) return;

    design::DesignDocument& doc = *input.doc;
    DesignerVmCacheEntry& entry = *input.entry;
    constexpr ImGuiDragDropFlags kPeek = ImGuiDragDropFlags_AcceptPeekOnly;
    constexpr ImGuiDragDropFlags kDeliver = ImGuiDragDropFlags_AcceptNoDrawDefaultRect;

    const ImGuiPayload* moving = ImGui::AcceptDragDropPayload(kNodeMoveDragDropId, kPeek);
    const ImGuiPayload* inserting = ImGui::AcceptDragDropPayload(kToolboxDragDropId, kPeek);
    const std::string moved_id =
        moving != nullptr ? std::string(static_cast<const char*>(moving->Data)) : std::string();

    designer::DropResolution drop;
    bool resolved = false;
    if (moving != nullptr || inserting != nullptr) {
        const designer::LayoutPoint point = MouseCanvasPoint(input.origin);
        const designer::NodeId target = PickDrawnNode(doc, entry.surface, point);
        resolved = designer::ResolveDrop(doc.Root(), target, entry.surface.Layout(), entry.frames, point, moved_id,
                                          &drop);
    }

    if (resolved) {
        DrawDropIndicator(drop.indicator, input.origin);
        if (drop.hasMarker) DrawInsertMarker(drop.marker, input.origin);

        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kToolboxDragDropId, kDeliver)) {
            ApplyToolboxDrop(input, drop, std::string(static_cast<const char*>(payload->Data)));
        }
        if (ImGui::AcceptDragDropPayload(kNodeMoveDragDropId, kDeliver) != nullptr) {
            ApplyMoveDrop(input, drop, moved_id);
        }
    }

    ImGui::EndDragDropTarget();
}

void DrawCanvasContextMenu(const CanvasInput& input) {
    if (!ImGui::BeginPopup("##canvas_context_menu")) return;
    const std::string& node_id = input.entry->context_menu_node_id;
    if (ImGui::MenuItem(util::Tr("canvas.extract_component").c_str())) {
        g_canvas_extract_request = {true, input.tab_id, node_id};
    }
    if (ImGui::MenuItem(util::Tr("menu.edit.duplicate").c_str(), "Ctrl+D")) {
        designer::SelectionManager* selection = &input.entry->surface.Selection();
        const std::string created = designer::ExecuteDuplicateComponent(&input.entry->command_manager, *input.doc,
                                                                         selection, node_id);
        if (!created.empty()) {
            SelectNode(selection, created);
        }
    }
    if (ImGui::MenuItem(util::Tr("explorer.delete").c_str())) {
        g_canvas_delete_request = {true, input.tab_id, node_id};
    }
    ImGui::EndPopup();
}

void HandleCanvasInput(const CanvasInput& input) {
    design::DesignDocument& doc = *input.doc;
    DesignerVmCacheEntry& entry = *input.entry;
    designer::SelectionManager* selection = &entry.surface.Selection();
    const ImVec2 hit_size(std::max(input.size.x, 1.0f), std::max(input.size.y, 1.0f));

    ImGui::SetCursorScreenPos(input.origin);
    ImGui::InvisibleButton("##canvas_hit_area", hit_size);
    const bool hovered = ImGui::IsItemHovered();
    const bool activated = ImGui::IsItemActivated();
    const std::string pointed =
        hovered ? PickDrawnNode(doc, entry.surface, MouseCanvasPoint(input.origin)) : std::string();

    if (!input.design_mode) {
        if (activated && !pointed.empty()) {
            avalang::ui::IComponent* node = design::FindNodeById(doc.Root(), pointed);
            while (node != nullptr) {
                InvokeNodeClickHandler(input.state_vm, node, input.eval_cache);
                node = design::FindParentOf(doc.Root(), node);
            }
        }
        return;
    }

    if (activated) {
        entry.pressed_node_id = pointed;
        if (!pointed.empty()) {
            SelectNode(selection, pointed);
            if (ImGui::GetIO().KeyCtrl) {
                InvokeNodeClickHandler(input.state_vm, design::FindNodeById(doc.Root(), pointed), input.eval_cache);
            }
        }
    }

    if (!pointed.empty() && ImGui::GetDragDropPayload() == nullptr) {
        if (avalang::ui::IComponent* node = design::FindNodeById(doc.Root(), pointed)) {
            ImGui::SetTooltip("%s", LowerAscii(node->TypeName()).c_str());
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && LowerAscii(node->TypeName()) == "button") {
                const std::string handler = design::EnsureClickHandler(doc, pointed);
                if (!handler.empty() && input.out_generated_handler != nullptr) {
                    *input.out_generated_handler = handler;
                }
            }
        }
    }

    avalang::ui::IComponent* pressed =
        entry.pressed_node_id.empty() ? nullptr : design::FindNodeById(doc.Root(), entry.pressed_node_id);
    const bool movable = pressed != nullptr && designer::MoveTool::CanMove(pressed, doc.Root());
    if (movable && ImGui::BeginDragDropSource()) {
        const std::string& id = entry.pressed_node_id;
        ImGui::SetDragDropPayload(kNodeMoveDragDropId, id.c_str(), id.size() + 1);
        ImGui::TextUnformatted(NodeDragLabel(pressed).c_str());
        ImGui::EndDragDropSource();
    }

    if (hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Right) && !pointed.empty()) {
        if (avalang::ui::IComponent* node = design::FindNodeById(doc.Root(), pointed)) {
            if (designer::MoveTool::CanMove(node, doc.Root())) {
                entry.context_menu_node_id = pointed;
                ImGui::OpenPopup("##canvas_context_menu");
            }
        }
    }
    DrawCanvasContextMenu(input);

    ImGui::SetCursorScreenPos(input.origin);
    ImGui::Dummy(hit_size);
    HandleCanvasDrop(input);
}

void FillResponsiveState(PropertiesState& state, const studio::design::LiveRenderResult& live_render) {
    if (state.mixed_types || state.selected_component_type.empty()) return;
    if (!live_render.ok || !live_render.styles) return;
    if (!live_render.styles->HasAnyStyles() && !live_render.styles->HasAnyResponsiveStyles()) return;
    state.responsive_viewport_width = live_render.viewportWidth;
    state.responsive_rows = designer::ResolveResponsiveRows(*live_render.styles, state.selected_component_type,
                                                             static_cast<double>(live_render.viewportWidth));
}

std::vector<designer::OverlayItem> BuildEntryOverlay(const DesignerVmCacheEntry& entry, bool fall_back_to_layout) {
    const designer::NodeFrameMap& frames = entry.frames;
    const designer::OverlayRectResolver resolver = [&frames, fall_back_to_layout](const designer::NodeId& id,
                                                                                   designer::OverlayItem& item) {
        const auto found = frames.find(id);
        if (found == frames.end()) return fall_back_to_layout;
        item.rect = found->second.selection;
        item.compact = found->second.compact;
        return true;
    };
    return entry.surface.Overlay(resolver);
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
        }
        state_vm = entry.vm;
        eval_cache = &entry.eval_cache;
        cache_entry_for_live_render = &entry;
    } else {
        state_vm = design::BuildStateVM(doc);
        design::BindCodeBehind(state_vm, doc);
    }

    designer::CommandManager* command_manager =
        cache_entry_for_live_render != nullptr ? &cache_entry_for_live_render->command_manager : nullptr;
    designer::SelectionManager* selection =
        cache_entry_for_live_render != nullptr ? &cache_entry_for_live_render->surface.Selection() : nullptr;

    const bool is_design_mode =
        cache_entry_for_live_render == nullptr ||
        cache_entry_for_live_render->surface.Mode() == designer::CanvasMode::Design;

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
    const std::optional<designer::DeviceViewport> fixed_viewport = FixedDeviceViewport(cache_entry_for_live_render);
    if (fixed_viewport) {
        canvas_size.x = static_cast<float>(fixed_viewport->deviceSize.width);
        canvas_size.y = static_cast<float>(fixed_viewport->deviceSize.height) + kCanvasTopReserve;

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
    if (fixed_viewport) ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::BeginChild("##DesignerCanvas", canvas_size, true,
                      fixed_viewport ? (ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)
                                     : ImGuiWindowFlags_HorizontalScrollbar);
    if (fixed_viewport) ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);

    if (view_reset_requested) {
        ImGui::SetScrollX(0.0f);
        ImGui::SetScrollY(0.0f);
    }
    if (cache_entry_for_live_render != nullptr) {
        cache_entry_for_live_render->surface.Viewport().SetPan(-static_cast<double>(ImGui::GetScrollX()),
                                                       -static_cast<double>(ImGui::GetScrollY()));
    }

    if (ImGui::IsWindowHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f)) {
        const ImVec2 delta = ImGui::GetIO().MouseDelta;
        if (cache_entry_for_live_render != nullptr) {
            designer::PanTool::Pan(&cache_entry_for_live_render->surface.Viewport(), -static_cast<double>(delta.x),
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
            designer::ZoomTool::ZoomBy(&cache_entry_for_live_render->surface.Viewport(), factor,
                                        designer::LayoutPoint{static_cast<double>(mouse.x),
                                                               static_cast<double>(mouse.y)});
        }
    }

    const std::string active_selected_node_id = CurrentSelectedNodeId(selection);
    if (is_design_mode && ImGui::IsWindowFocused() && !active_selected_node_id.empty() &&
        active_selected_node_id != doc.Root()->NodeId() && ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
        g_canvas_delete_request = {true, tab_id, active_selected_node_id};
    }

    const ImVec2 avail_before_reserve = ImGui::GetContentRegionAvail();
    ImGui::Dummy(ImVec2(std::max(avail_before_reserve.x, 1.0f), kCanvasTopReserve));

    const float page_pad = fixed_viewport ? 0.0f : kPageOuterPad;

    const ImVec2 raw_origin = ImGui::GetCursorScreenPos();
    const ImVec2 raw_avail = ImGui::GetContentRegionAvail();
    ImVec2 origin(raw_origin.x + page_pad, raw_origin.y + page_pad);
    Rect canvas_rect{0.0f, 0.0f, std::max(raw_avail.x - 2.0f * page_pad, 1.0f),
                      std::max(raw_avail.y - 2.0f * page_pad, 1.0f)};
    ImVec2 page_size(canvas_rect.w, canvas_rect.h);
    if (fixed_viewport) {
        origin = ImVec2(raw_origin.x + static_cast<float>(fixed_viewport->contentRect.x),
                        raw_origin.y + static_cast<float>(fixed_viewport->contentRect.y));
        canvas_rect = Rect{0.0f, 0.0f, static_cast<float>(fixed_viewport->contentRect.width),
                            static_cast<float>(fixed_viewport->contentRect.height)};
        page_size = ImVec2(static_cast<float>(fixed_viewport->deviceSize.width),
                           static_cast<float>(fixed_viewport->deviceSize.height));
    }
    const ImVec2 page_origin = fixed_viewport ? raw_origin : origin;

    {
        ImDrawList* page_draw_list = ImGui::GetWindowDrawList();
        const ImVec2 page_p0 = page_origin;
        const ImVec2 page_p1(page_origin.x + page_size.x, page_origin.y + page_size.y);
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
                                            entry.live_render_mode != entry.surface.Mode() ||
                                            entry.live_render_dirty;
            if (live_render_stale) {
                const bool design_build = entry.surface.Mode() == designer::CanvasMode::Design;
                if (design_build) {
                    studio::design::RevertInjectedProperties(
                        doc.tree.get(), entry.design_injected,
                        [&doc](const std::string& node_id, const std::string& key) {
                            return design::IsPropertyAuthored(doc, node_id, key);
                        });
                    entry.design_injected.clear();
                }
                avalang::ui::ComponentTree* tree_for_live_render = doc.tree.get();
                if (entry.surface.Mode() == designer::CanvasMode::Preview) {
                    entry.preview_resolved_tree = studio::design::ResolvePreviewTree(doc, project_root);
                    if (entry.preview_resolved_tree) {
                        tree_for_live_render = entry.preview_resolved_tree.get();
                    }
                } else {
                    entry.preview_resolved_tree.reset();
                }
                entry.live_render = studio::design::BuildLiveRender(
                    tree_for_live_render, vw, vh, doc.extends, project_root, live_eval,
                    entry.surface.Mode() == designer::CanvasMode::Design);
                if (design_build) entry.design_injected = entry.live_render.injected;
                entry.live_render_dirty = false;
                entry.live_render_w = vw;
                entry.live_render_h = vh;
                entry.live_render_mode = entry.surface.Mode();
                entry.imgui_renderer = std::make_unique<avalang::ui::ImGuiRenderer>(vw, vh);
                entry.animation_controller = entry.live_render.ok
                    ? avalang::ui::animation::AnimationController::Create(entry.live_render.sceneGraph.get())
                    : nullptr;
            }
            live_render = &entry.live_render;
            live_render_renderer = entry.imgui_renderer.get();
        } else {
            local_live_render = studio::design::BuildLiveRender(doc.tree.get(), vw, vh, doc.extends, project_root,
                                                                 live_eval, is_design_mode);
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

        avalang::ui::InteractiveState interactive;
        const bool use_interactive = !is_design_mode;
        if (use_interactive) {
            const ImVec2 mouse = ImGui::GetMousePos();
            const bool canvas_hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
            if (canvas_hovered) {
                interactive.pointerX = static_cast<int>(mouse.x - origin.x);
                interactive.pointerY = static_cast<int>(mouse.y - origin.y);
            } else {
                interactive.pointerX = -1000000;
                interactive.pointerY = -1000000;
            }
            interactive.pointerDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
        }

        avalang::ui::SceneCommandWalker::Walk(*live_render->sceneGraph, sink, *live_render_renderer,
                                               use_interactive ? &interactive : nullptr);
        live_render_painted = true;
    }

    if (is_design_mode && fixed_viewport && designer::HasSafeArea(fixed_viewport->insets)) {
        ImDrawList* guide_draw_list = ImGui::GetWindowDrawList();
        const ImU32 band_color = palette::U32FromHex(palette::kTextDisabled, 0.14f);
        for (const designer::LayoutRect& band :
             designer::SafeAreaBands(fixed_viewport->deviceSize, fixed_viewport->insets)) {
            const ImVec2 band_p0(page_origin.x + static_cast<float>(band.x), page_origin.y + static_cast<float>(band.y));
            guide_draw_list->AddRectFilled(band_p0, ImVec2(band_p0.x + static_cast<float>(band.width),
                                                            band_p0.y + static_cast<float>(band.height)),
                                           band_color);
        }
        const designer::LayoutRect device_rect{0.0, 0.0, fixed_viewport->deviceSize.width,
                                                fixed_viewport->deviceSize.height};
        const designer::LayoutRect safe_rect = designer::ApplySafeArea(device_rect, fixed_viewport->insets);
        const ImVec2 safe_p0(page_origin.x + static_cast<float>(safe_rect.x),
                              page_origin.y + static_cast<float>(safe_rect.y));
        const ImVec2 safe_p1(safe_p0.x + static_cast<float>(safe_rect.width),
                              safe_p0.y + static_cast<float>(safe_rect.height));
        DrawDashedRect(guide_draw_list, safe_p0, safe_p1, palette::U32FromHex(palette::kTextDisabled));
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
            cache_entry_for_live_render->surface.Layout().AdoptRects(*uid_to_rect);
        } else {
            cache_entry_for_live_render->surface.Layout().Clear();
        }
    }

    const bool surface_picking_active = is_design_mode && cache_entry_for_live_render != nullptr && live_render_painted;
    std::string surface_hovered_node_id;
    if (surface_picking_active) {
        surface_hovered_node_id = PickSurfaceNode(doc, cache_entry_for_live_render->surface, origin);
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
        const designer::LayoutRect frame_bounds{0.0, 0.0, static_cast<double>(canvas_rect.w),
                                                 static_cast<double>(canvas_rect.h)};
        cache_entry_for_live_render->frames =
            overlay_driven ? designer::CollectNodeFrames(doc.Root(), cache_entry_for_live_render->surface.Layout(),
                                                          frame_bounds, &g_resize_tool)
                           : designer::NodeFrameMap();
    }

    std::vector<std::string> missing_rect_uids;
    const ImVec2 canvas_min = origin;
    const ImVec2 canvas_max(origin.x + canvas_rect.w, origin.y + canvas_rect.h);
    DrawNode(root_to_draw, origin, doc, command_manager, selection, live_render_painted, uid_to_rect,
             &missing_rect_uids, overlay_driven, is_design_mode, 0.0f, 0, canvas_min, canvas_max);

    if (overlay_driven) {
        DrawResizeHandlesOverlay(doc, *cache_entry_for_live_render, origin);
    }

    if (cache_entry_for_live_render != nullptr && live_render_painted) {
        CanvasInput input;
        input.doc = &doc;
        input.entry = cache_entry_for_live_render;
        input.state_vm = state_vm;
        input.eval_cache = eval_cache;
        input.out_generated_handler = out_generated_handler;
        input.origin = origin;
        input.size = ImVec2(canvas_rect.w, canvas_rect.h);
        input.design_mode = is_design_mode;
        input.tab_id = tab_id;
        HandleCanvasInput(input);
    }

    if (g_state_changed_by_handler) {
        g_state_changed_by_handler = false;
        if (cache_entry_for_live_render != nullptr) {
            cache_entry_for_live_render->live_render_dirty = true;
        }
    }

    if (cache_entry_for_live_render != nullptr) {
        if (selection != nullptr) {
            const std::vector<designer::OverlayItem> overlay_items =
                BuildEntryOverlay(*cache_entry_for_live_render, false);

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
                if (!primary_id.empty() && cache_entry_for_live_render->surface.Layout().HasRect(primary_id)) {
                    if (avalang::ui::IComponent* primary_node = design::FindNodeById(doc.Root(), primary_id)) {
                        const designer::LayoutRect primary_rect =
                            cache_entry_for_live_render->surface.Layout().RectOf(primary_id);
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
            const bool selection_without_overlay = overlay_driven && !selection->IsEmpty() && overlay_items.empty();
            std::string* last_logged_overlay_gap = &cache_entry_for_live_render->last_logged_overlay_gap;
            if (selection_without_overlay) {
                const std::string message =
                    "designer::BuildOverlay no encontro rects para la seleccion actual "
                    "(surface layout desalineado con uid_to_rect)";
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

    if (selection != nullptr && selection->Selected().size() > 1) {
        selected = ToMultiPropertiesState(doc, selection->Selected(), tab_id, project_root);
    } else {
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
    }

    if (selected && cache_entry_for_live_render != nullptr) {
        FillResponsiveState(*selected, *live_render);
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

void RevertDesignerInjectedProperties(int tab_id, design::DesignDocument& doc) {
    const auto it = g_designer_vm_cache.find(tab_id);
    if (it == g_designer_vm_cache.end()) return;

    DesignerVmCacheEntry& entry = it->second;
    if (entry.design_injected.empty()) return;
    studio::design::RevertInjectedProperties(doc.tree.get(), entry.design_injected,
                                             [&doc](const std::string& node_id, const std::string& key) {
                                                 return design::IsPropertyAuthored(doc, node_id, key);
                                             });
    entry.design_injected.clear();
    entry.live_render_dirty = true;
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
    entry.design_injected.clear();
    entry.imgui_renderer.reset();
    entry.live_render_w = -1;
    entry.live_render_h = -1;
    entry.last_logged_live_render_error.clear();
    entry.last_logged_missing_rects.clear();
    entry.last_logged_overlay_gap.clear();
    entry.pressed_node_id.clear();
    entry.context_menu_node_id.clear();
    entry.frames.clear();
    entry.surface_hovered_node_id.clear();
    entry.surface.Layout().Clear();
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
    designer::SelectionManager& selection = it->second.surface.Selection();
    selection.Clear();
    selection.ClearHovered();
    selection.ClearFocused();
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
    return &it->second.surface.Selection();
}

designer::LayoutCore* GetDesignerSurfaceLayout(int tab_id) {
    auto it = g_designer_vm_cache.find(tab_id);
    if (it == g_designer_vm_cache.end()) return nullptr;
    return &it->second.surface.Layout();
}

designer::DesignerViewport* GetDesignerViewport(int tab_id) {
    auto it = g_designer_vm_cache.find(tab_id);
    if (it == g_designer_vm_cache.end()) return nullptr;
    return &it->second.surface.Viewport();
}

std::string GetDesignerHoveredNode(int tab_id) {
    auto it = g_designer_vm_cache.find(tab_id);
    if (it == g_designer_vm_cache.end()) return std::string();
    return it->second.surface_hovered_node_id;
}

designer::CanvasMode GetDesignerCanvasMode(int tab_id) {
    auto it = g_designer_vm_cache.find(tab_id);
    if (it == g_designer_vm_cache.end()) return designer::CanvasMode::Design;
    return it->second.surface.Mode();
}

avalang::ui::animation::AnimationController* GetDesignerAnimationController(int tab_id) {
    auto it = g_designer_vm_cache.find(tab_id);
    if (it == g_designer_vm_cache.end()) return nullptr;
    if (it->second.surface.Mode() != designer::CanvasMode::Preview) return nullptr;
    return it->second.animation_controller.get();
}

std::vector<designer::OverlayItem> GetDesignerOverlay(int tab_id) {
    auto it = g_designer_vm_cache.find(tab_id);
    if (it == g_designer_vm_cache.end()) return {};
    return BuildEntryOverlay(it->second, true);
}

}