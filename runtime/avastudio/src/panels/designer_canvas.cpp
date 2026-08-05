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
#include "design/live_render_bridge.h"
#include "design/state_eval.h"
#include "commands/RenderCommandSink.h"
#include "commands/SceneCommandWalker.h"
#include "components/IComponent.h"
#include "components/PropertyValue.h"
#include "events/AutoBind.h"
#include "layout/LayoutEngine.h"
#include "imgui.h"
#include "palette.h"
#include "panels/toolbox_panel.h"

#include "GLFW/glfw3.h"
#include "stb_image.h"

namespace studio {

namespace {

std::string LowerAscii(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

std::string PropertyValueToDisplayString(const avalang::ui::PropertyValue& v) {
    switch (v.Type()) {
        case avalang::ui::PropertyType::Bool:
            return v.AsBool() ? "true" : "false";
        case avalang::ui::PropertyType::Number: {
            double n = v.AsNumber();
            if (n == static_cast<long long>(n)) return std::to_string(static_cast<long long>(n));
            return std::to_string(n);
        }
        case avalang::ui::PropertyType::String:
            return v.AsString();
        default:
            return "";
    }
}

std::string GetNodeIdProp(avalang::ui::IComponent* node) {
    const auto* idProp = node->GetProperty("id");
    if (idProp && idProp->Type() == avalang::ui::PropertyType::String) return idProp->AsString();
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

struct DesignerVmCacheEntry {
    AvaVM* vm = nullptr;
    bool last_dirty = false;
    std::unordered_map<std::string, std::string> eval_cache;
    std::string cached_project_root;

    studio::design::LiveRenderResult live_render;
    std::unique_ptr<avalang::ui::ImGuiRenderer> imgui_renderer;
    int live_render_w = -1;
    int live_render_h = -1;

    std::string last_logged_live_render_error;
    std::string last_logged_missing_rects;
};

std::unordered_map<int, DesignerVmCacheEntry> g_designer_vm_cache;

std::string g_uncached_last_logged_live_render_error;
std::string g_uncached_last_logged_missing_rects;

struct CanvasDeleteRequest {
    bool open = false;
    int tab_id = -1;
    std::string node_id;
};
CanvasDeleteRequest g_canvas_delete_request;

struct ResizeDragState {
    bool active = false;
    std::string node_id;
    bool resize_x = false;
    bool resize_y = false;
    float start_width = 0.0f;
    float start_height = 0.0f;
};
ResizeDragState g_resize_drag;

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

void SetSizeProperty(avalang::ui::IComponent* node, const char* key, float value) {
    const std::string text = std::to_string(static_cast<int>(std::lround(value)));
    node->SetProperty(key, avalang::ui::PropertyValue(text));
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
// Single shared visual language for the selection ring, used identically for
// containers and leaf controls (previously containers drew a fainter,
// thinner, more-rounded ring than leaves -- see designer_canvas.cpp session
// notes, "selection consistency" pass).
constexpr float kSelectionBorderThickness = 2.0f;
constexpr float kSelectionCornerRadius = 2.5f;
const ImU32 kSelectionBorderColor = palette::U32FromHex(palette::kPrimary);
const ImU32 kHoverBorderColor = palette::U32FromHex(palette::kPrimary, 0.7f);
constexpr float kCanvasTopReserve = kRealContainerPadTop;

// Single source of truth for "what rectangle does the selection system use
// for this node/region". Every place that needs to know the selection box --
// the hit-test area for hover/click, the hover ring, the selected ring, the
// resize handles -- calls this once and uses the result, instead of each
// site re-deriving its own padding/threshold logic (which is exactly how the
// hover/selected/hit-test mismatches from before crept in: three separate
// call sites, three subtly different rectangles). A future change to the pad
// amount or the compact threshold only has to happen here.
struct SelectionBox {
    ImVec2 p0;
    ImVec2 p1;
    bool compact = false;
};

// `has_own_padding`: true for containers, whose base rect is already
// chrome_p0/chrome_p1 -- a decorative box padded outward from the real
// layout rect (kRealContainerPadSide/kRealContainerPadTop) so the type chip
// and border have room to draw. Adding the usual selection pad ON TOP of
// that stacked two paddings, so a selected/hovered container's ring grew
// well past its own chrome and could bleed into a tightly-packed sibling
// right below it. Containers get pad=0 here (their chrome already supplies
// the breathing room); only leaf controls, which have no chrome of their
// own, get the extra pad.
SelectionBox ComputeSelectionBox(ImVec2 base_p0, ImVec2 base_p1, bool has_own_padding = false) {
    SelectionBox box;
    box.compact = !has_own_padding && (base_p1.y - base_p0.y) < kCompactNodeHeightThreshold;
    const float pad = has_own_padding ? 0.0f : (box.compact ? kSelectionPadCompact : kSelectionPad);
    box.p0 = ImVec2(base_p0.x - pad, base_p0.y - pad);
    box.p1 = ImVec2(base_p1.x + pad, base_p1.y + pad);
    return box;
}

// Single draw call for the selection/hover ring -- color is the only thing
// that ever differs between "hovering" and "selected", so thickness/radius
// live here once instead of being repeated (and able to drift) at every
// AddRect call site.
void DrawSelectionRing(ImDrawList* draw_list, ImVec2 p0, ImVec2 p1, ImU32 color) {
    draw_list->AddRect(p0, p1, color, kSelectionCornerRadius, 0, kSelectionBorderThickness);
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

constexpr float kEdgeBandFrac = 0.25f;

design::DropZone ComputeDropZone(float mouse_y, ImVec2 p0, ImVec2 p1, bool is_container) {
    if (is_container) return design::DropZone::kInto;
    const float height = std::max(p1.y - p0.y, 1.0f);
    const float frac = (mouse_y - p0.y) / height;
    return frac <= 0.5f ? design::DropZone::kBefore : design::DropZone::kAfter;
}

void HandleDropTarget(avalang::ui::IComponent* node, design::DesignDocument& doc, bool is_container,
                       ImVec2 p0, ImVec2 p1) {
    if (ImGui::BeginDragDropTarget()) {
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
            if (ImGui::AcceptDragDropPayload(kToolboxDragDropId, ImGuiDragDropFlags_AcceptPeekOnly)) {
                ImGui::GetWindowDrawList()->AddRect(p0, p1, palette::U32FromHex(palette::kPrimary), 2.0f, 0, 3.0f);
            }
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kToolboxDragDropId)) {
                const std::string dropped_type(static_cast<const char*>(payload->Data));
                if (avalang::ui::IComponent* real = design::FindNodeById(doc.Root(), node->NodeId())) {
                    design::AddComponentNode(doc, real->NodeId(), dropped_type, "", {});
                }
            }
        } else {
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
                avalang::ui::IComponent* target = design::FindNodeById(doc.Root(), node->NodeId());
                avalang::ui::IComponent* parent = target ? design::FindParentOf(doc.Root(), target) : nullptr;
                if (parent) {
                    const design::DropZone zone = ComputeDropZone(ImGui::GetMousePos().y, p0, p1, is_container);
                    const std::string new_id = design::AddComponentNode(doc, parent->NodeId(), dropped_type, "", {});
                    if (!new_id.empty()) {
                        design::MoveNode(doc, new_id, node->NodeId(), zone);
                    }
                }
            }
        }

        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kNodeMoveDragDropId)) {
            const std::string moved_id(static_cast<const char*>(payload->Data));
            const design::DropZone zone = ComputeDropZone(ImGui::GetMousePos().y, p0, p1, is_container);
            design::MoveNode(doc, moved_id, node->NodeId(), zone);
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

bool DrawRealWidget(avalang::ui::IComponent* node, const std::string& evaluated_display, ImVec2 p0, ImVec2 p1,
                     const std::string& project_root) {
    std::vector<PropertyRow> properties;
    CollectPropertyRows(node, &properties, nullptr);
    const std::string type = LowerAscii(node->TypeName());

    const ImVec2 size(std::max(p1.x - p0.x, 1.0f), std::max(p1.y - p0.y, 1.0f));
    ImGui::SetCursorScreenPos(p0);
    ImGui::PushItemWidth(size.x);

    const bool node_enabled = FindPropValue(properties, "enabled", "true") == "true";
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
        const float border_radius = FindPropValueF(properties, "borderRadius", 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, border_radius);
        ImGui::Button(evaluated_display.c_str(), size);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(frame_colors + 1);
    } else if (type == "textbox") {
        const int frame_colors = PushClassicFrameStyle();
        PushClassicTextStyle();
        std::string buf = evaluated_display;
        buf.resize(std::max<size_t>(buf.size() + 1, 256), '\0');
        ImGui::InputText("##textbox_preview", buf.data(), buf.size(), ImGuiInputTextFlags_ReadOnly);
        ImGui::PopStyleColor(frame_colors + 1);
    } else if (type == "checkbox") {
        const int frame_colors = PushClassicFrameStyle();
        bool checked = FindPropValue(properties, "checked", "false") == "true";
        ImGui::Checkbox(evaluated_display.c_str(), &checked);
        ImGui::PopStyleColor(frame_colors);
    } else if (type == "radiobutton") {
        const int frame_colors = PushClassicFrameStyle();
        const bool checked = FindPropValue(properties, "checked", "false") == "true";
        ImGui::RadioButton(evaluated_display.c_str(), checked);
        ImGui::PopStyleColor(frame_colors);
    } else if (type == "text") {
        ImGui::GetWindowDrawList()->AddRectFilled(p0, p1, IM_COL32(0xE0, 0xE0, 0xE0, 0xFF));
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0x00, 0x00, 0x00, 0xFF));
        ImGui::TextUnformatted(evaluated_display.c_str());
        ImGui::PopStyleColor();
    } else if (type == "link") {
        ImGui::PushStyleColor(ImGuiCol_Text, palette::U32FromHex(palette::kPrimary));
        ImGui::TextUnformatted(evaluated_display.c_str());
        ImGui::PopStyleColor();
        const ImVec2 text_size = ImGui::CalcTextSize(evaluated_display.c_str());
        ImGui::GetWindowDrawList()->AddLine(ImVec2(p0.x, p0.y + text_size.y),
                                             ImVec2(p0.x + text_size.x, p0.y + text_size.y),
                                             palette::U32FromHex(palette::kPrimary), 1.0f);
    } else if (type == "divider") {
        ImGui::GetWindowDrawList()->AddLine(ImVec2(p0.x, p0.y + size.y * 0.5f), ImVec2(p1.x, p0.y + size.y * 0.5f),
                                             palette::U32FromHex(palette::kBorder), 1.0f);
    } else if (type == "spacer") {
        // intencionalmente vacio
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

void DrawNode(avalang::ui::IComponent* node, ImVec2 origin,
              design::DesignDocument& doc,
              std::optional<PropertiesState>& out_selected, int tab_id,
              std::string* out_generated_handler, AvaVM* state_vm,
              std::unordered_map<std::string, std::string>* eval_cache,
              const std::string& project_root, bool live_render_painted,
              const std::unordered_map<std::string, avalang::ui::LayoutRect>* uid_to_rect = nullptr,
              std::vector<std::string>* out_missing_rect_uids = nullptr,
              float extra_offset_y = 0.0f, int depth = 0) {
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

    const ImVec2 base_p0 = ToImVec2(r, origin);
    const ImVec2 raw_p0 = ImVec2(base_p0.x, base_p0.y + extra_offset_y);
    const ImVec2 raw_p1 = ImVec2(raw_p0.x + r.w, raw_p0.y + r.h);
    const float margin = std::min(kNodeMargin + static_cast<float>(depth) * kNodeMarginPerDepth, kNodeMarginMax);
    const ImVec2 p0(raw_p0.x + margin, raw_p0.y + margin);
    const ImVec2 p1(std::max(p0.x, raw_p1.x - margin), std::max(p0.y, raw_p1.y - margin));

    const bool pad_chrome = is_container;
    const ImVec2 chrome_p0 = pad_chrome ? ImVec2(p0.x - kRealContainerPadSide, p0.y - kRealContainerPadTop) : p0;
    const ImVec2 chrome_p1 = pad_chrome ? ImVec2(p1.x + kRealContainerPadSide, p1.y + kRealContainerPadSide) : p1;

    const bool selected = (node->NodeId() == doc.selected_node_id);
    const bool skip_leaf_wireframe = live_render_painted && !is_container;
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    constexpr bool synthetic = false;
    ImGui::PushID(node->NodeId().c_str());

    // Computed once and shared by the hover ring AND the selected ring below
    // (previously hover drew at the tight p0/hit_p1 bounds while the selected
    // ring drew at these padded sel_p0/sel_p1 bounds, and for containers the
    // selected ring additionally started from the bigger chrome_p0/chrome_p1
    // instead of p0/p1 -- so selecting a hovered node visibly "grew" the box
    // instead of just changing its color, which is what was reported).
    const ImVec2 base_sel_p0 = is_container ? chrome_p0 : (skip_leaf_wireframe ? raw_p0 : p0);
    const ImVec2 base_sel_p1 = is_container ? chrome_p1 : (skip_leaf_wireframe ? raw_p1 : p1);
    const SelectionBox sel_box = ComputeSelectionBox(base_sel_p0, base_sel_p1, /*has_own_padding=*/is_container);
    const ImVec2 sel_p0 = sel_box.p0;
    const ImVec2 sel_p1 = sel_box.p1;
    const bool compact = sel_box.compact;

    const ImU32 fill = is_container ? palette::U32FromHex(palette::kSurface, 0.6f)
                                     : palette::U32FromHex(palette::kCard, 0.9f);
    const ImU32 border = selected ? palette::U32FromHex(palette::kPrimary)
                                   : (is_container ? palette::U32FromHex(palette::kBorder, 0.45f)
                                                    : palette::U32FromHex(palette::kBorder));

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
        // Unified selection chrome (09_DESIGNER_CANVAS_UX_PLAN.md "selection
        // consistency" pass): containers and leaf controls now share the exact
        // same ring color/opacity/thickness/corner-radius and the same padding
        // rule, instead of containers getting a fainter, thinner, more-rounded
        // ring with no handles. The only thing that still varies by size is
        // whether resize handles fit -- not the ring itself.
        DrawSelectionRing(draw_list, sel_p0, sel_p1, kSelectionBorderColor);

        if (!compact) {
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
            const ImU32 handle_fill = palette::U32FromHex(palette::kBackground);
            const ImU32 handle_border = palette::U32FromHex(palette::kPrimary);
            for (const ImVec2& c : handles) {
                const ImVec2 hp0(c.x - kHandleHalf, c.y - kHandleHalf);
                const ImVec2 hp1(c.x + kHandleHalf, c.y + kHandleHalf);
                draw_list->AddRectFilled(hp0, hp1, handle_fill);
                draw_list->AddRect(hp0, hp1, handle_border, 0.0f, 0, 1.0f);
            }

            // Resize handles are now available for containers too, not just
            // leaf controls -- width/height are plain properties on any node
            // (see SetSizeProperty/TryGetNumericProperty above), so there was
            // no functional reason a Panel/Row/Column couldn't be resized the
            // same way a Button or Label already could.
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
                        g_resize_drag = ResizeDragState{true, node->NodeId(), adjust_x, adjust_y, start_w, start_h};
                    }
                    if (ImGui::IsItemActive() && g_resize_drag.active && g_resize_drag.node_id == node->NodeId()) {
                        const ImVec2 total_delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0f);
                        if (adjust_x) {
                            SetSizeProperty(node, "width",
                                            std::max(kMinResizeDimension, g_resize_drag.start_width + total_delta.x));
                        }
                        if (adjust_y) {
                            SetSizeProperty(node, "height",
                                            std::max(kMinResizeDimension, g_resize_drag.start_height + total_delta.y));
                        }
                        doc.dirty = true;
                    }
                    if (ImGui::IsItemDeactivated() && g_resize_drag.node_id == node->NodeId()) {
                        g_resize_drag.active = false;
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
        } else {
            const bool container_hovered = ImGui::IsMouseHoveringRect(p0, p1);
            if (container_hovered) {
                ImGui::SetTooltip("%s", type_name.c_str());
            }
        }
    }

    const std::vector<avalang::ui::IComponent*> node_children = node->Children();

    if (is_container && !synthetic && node_children.empty() && header_bottom < p1.y - 4.0f) {
        const ImVec2 body_center((p0.x + p1.x) * 0.5f, (header_bottom + p1.y) * 0.5f);
        const ImU32 hint_color = palette::U32FromHex(palette::kTextDisabled);
        const float half = 6.0f;
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
        widget_drawn = DrawRealWidget(node, evaluated_display, widget_p0, widget_p1, project_root);
    }
    if (!widget_drawn) {
        draw_list->AddText(ImVec2(p0.x + 4.0f, p0.y + 4.0f), palette::U32FromHex(palette::kTextPrimary),
                            label.c_str());
        if (!evaluated_display.empty()) {
            draw_list->AddText(ImVec2(p0.x + 4.0f, p0.y + 20.0f),
                                palette::U32FromHex(palette::kTextSecondary), evaluated_display.c_str());
        }
    }

    const ImVec2 hit_p1 = !is_container ? p1 : (header_reserves_space ? ImVec2(p1.x, header_bottom) : p1);
    // Hit-test area now matches sel_p0/sel_p1 -- the same padded rect the
    // hover/selected ring is drawn at -- instead of the tighter p0/hit_p1.
    // That was the last piece of the mismatch: the ring already lined up
    // between hover and selected, but the mouse only actually registered as
    // "over the node" inside the narrower p0/hit_p1 box, so there was a dead
    // strip (the padding) where you could see the ring's territory but
    // hovering there did nothing.
    ImGui::SetCursorScreenPos(sel_p0);
    ImGui::SetNextItemAllowOverlap();
    ImGui::InvisibleButton("##node_hit_area",
                            ImVec2(std::max(sel_p1.x - sel_p0.x, 1.0f), std::max(sel_p1.y - sel_p0.y, 1.0f)));
    const bool node_hovered = ImGui::IsItemHovered();
    if (!synthetic && node_hovered) {
        // Arrow, not Hand: this is a design-surface selection affordance, not
        // a hyperlink/button action -- Hand here was misleading (matches the
        // report that the cursor shouldn't change to a hand on hover).
        ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
        if (!selected) {
            // Same sel_p0/sel_p1 rect the selected ring below uses -- hover
            // and selected now trace the identical rectangle both for what's
            // drawn AND for what actually responds to the mouse.
            DrawSelectionRing(draw_list, sel_p0, sel_p1, kHoverBorderColor);
        }
    }
    if (ImGui::IsItemClicked()) {
        doc.selected_node_id = node->NodeId();
        out_selected = ToPropertiesState(node, /*editable=*/!synthetic, tab_id);

        const bool should_invoke_click = !synthetic && state_vm && ImGui::GetIO().KeyCtrl;
        if (should_invoke_click) {
            for (const PropertyRow& ev : events) {
                if (ev.key == "click" && !ev.value.empty()) {
                    std::string handler_error;
                    const bool ok = design::InvokeHandler(state_vm, ev.value, &handler_error);
                    if (ok && eval_cache != nullptr) {
                        eval_cache->clear();
                    }
                    (void)handler_error;
                    break;
                }
            }
        }
    }

    if (!synthetic && type_name == "button" && ImGui::IsItemHovered() &&
        ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        const std::string handler = design::EnsureClickHandler(doc, node->NodeId());
        if (!handler.empty() && out_generated_handler) {
            *out_generated_handler = handler;
        }
    }

    const bool movable = !synthetic && node != doc.Root();
    if (movable && ImGui::BeginDragDropSource()) {
        ImGui::SetDragDropPayload(kNodeMoveDragDropId, node->NodeId().c_str(), node->NodeId().size() + 1);
        ImGui::TextUnformatted(label.c_str());
        ImGui::EndDragDropSource();
    }

    if (movable && ImGui::BeginPopupContextItem("##node_context_menu")) {
        if (ImGui::MenuItem("Delete")) {
            g_canvas_delete_request = {true, tab_id, node->NodeId()};
        }
        ImGui::EndPopup();
    }

    if (!synthetic) {
        HandleDropTarget(node, doc, is_container, p0, hit_p1);
    }

    // Only reserve a separate body hit-area when a header strip actually
    // takes up its own space (the non-live-render placeholder mode). In the
    // normal live-render mode header_reserves_space is false and hit_p1
    // above already equals p1 -- so this used to fire anyway (guarded only by
    // "header_bottom < p1.y", which is true for any container with height)
    // and stamped a second, fully-overlapping InvisibleButton directly on top
    // of "##node_hit_area" for every container. Because it was added later
    // it silently won hover every time, which meant the container's
    // right-click delete menu and its drag-to-move source -- both bound to
    // "##node_hit_area" as "the last item" -- stopped firing, and clicks
    // landed on whichever of the two duplicate buttons ImGui happened to
    // resolve. That's the actual cause of containers selecting/behaving
    // inconsistently, not just a cursor style issue.
    if (is_container && !synthetic && header_reserves_space) {
        const SelectionBox body_sel_box = ComputeSelectionBox(ImVec2(p0.x, header_bottom), p1);
        ImGui::SetCursorScreenPos(ImVec2(p0.x, header_bottom));
        ImGui::SetNextItemAllowOverlap();
        ImGui::InvisibleButton("##node_body_drop_area",
                                ImVec2(std::max(p1.x - p0.x, 1.0f), std::max(p1.y - header_bottom, 1.0f)));
        if (ImGui::IsItemHovered()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
            if (ImGui::GetDragDropPayload() != nullptr) {
                draw_list->AddRectFilled(ImVec2(p0.x, header_bottom), p1, palette::U32FromHex(palette::kPrimary, 0.06f));
            }
            // Same helper/box the header/leaf hit-area uses -- any future
            // change to padding, color, or ring style applies here too
            // automatically instead of needing a matching edit.
            if (!selected) {
                DrawSelectionRing(draw_list, body_sel_box.p0, body_sel_box.p1, kHoverBorderColor);
            }
        }
        if (ImGui::IsItemClicked()) {
            doc.selected_node_id = node->NodeId();
            out_selected = ToPropertiesState(node, /*editable=*/true, tab_id);
        }
        HandleDropTarget(node, doc, is_container, ImVec2(p0.x, header_bottom), p1);
    }

    ImGui::PopID();

    const float child_offset_y = extra_offset_y + (header_reserves_space ? kHeaderHeight : 0.0f);
    for (avalang::ui::IComponent* child : node_children) {
        if (IsDialogNode(child)) continue;
        DrawNode(child, origin, doc, out_selected, tab_id, out_generated_handler, state_vm,
                 eval_cache, project_root, live_render_painted, uid_to_rect, out_missing_rect_uids,
                 child_offset_y, depth + 1);
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
                         std::optional<PropertiesState>& out_selected) {
    if (doc.selected_node_id.empty()) return 0.0f;
    std::vector<BreadcrumbSegment> path;
    if (!CollectBreadcrumbPath(root_to_draw, doc.selected_node_id, path)) return 0.0f;

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
            doc.selected_node_id = path[i].node_id;
            if (avalang::ui::IComponent* real = design::FindNodeById(doc.Root(), path[i].node_id)) {
                out_selected = ToPropertiesState(real, /*editable=*/true, tab_id);
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
                      std::optional<PropertiesState>& out_selected) {
    std::vector<DialogTrayEntry> dialogs;
    CollectDialogNodes(root_to_draw, dialogs);
    if (dialogs.empty()) return 0.0f;

    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("Dialogs (non-visual):");
    for (size_t i = 0; i < dialogs.size(); ++i) {
        ImGui::SameLine();
        ImGui::PushID(static_cast<int>(i));
        const bool is_selected = (doc.selected_node_id == dialogs[i].node_id);
        if (is_selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        }
        if (ImGui::SmallButton(dialogs[i].label.c_str())) {
            doc.selected_node_id = dialogs[i].node_id;
            if (avalang::ui::IComponent* real = design::FindNodeById(doc.Root(), dialogs[i].node_id)) {
                out_selected = ToPropertiesState(real, /*editable=*/true, tab_id);
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
                                   std::optional<PropertiesState>& out_selected) {
    if (g_canvas_delete_request.tab_id != tab_id) return;
    if (g_canvas_delete_request.open) {
        ImGui::OpenPopup("Delete node?");
        g_canvas_delete_request.open = false;
    }
    ImGui::SetNextWindowSize(ImVec2(360.0f, 0.0f));
    if (ImGui::BeginPopupModal("Delete node?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        avalang::ui::IComponent* node = design::FindNodeById(doc.Root(), g_canvas_delete_request.node_id);
        std::string label = "this node";
        if (node) {
            label = LowerAscii(node->TypeName());
            const std::string node_id_prop = GetNodeIdProp(node);
            if (!node_id_prop.empty()) label += " (" + node_id_prop + ")";
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
            if (design::RemoveNode(doc, g_canvas_delete_request.node_id)) {
                if (out_selected && (out_selected->selected_node_id == g_canvas_delete_request.node_id ||
                                      doc.selected_node_id.empty())) {
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
        const bool needs_rebuild =
            entry.vm == nullptr || entry.last_dirty != doc.dirty || entry.cached_project_root != project_root;
        tree_state_rebuilt_this_frame = needs_rebuild;
        if (needs_rebuild) {
            if (entry.vm) ava_vm_destroy(entry.vm);
            entry.vm = design::BuildStateVM(doc);
            design::BindCodeBehind(entry.vm, doc);
            entry.last_dirty = doc.dirty;
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

    const float breadcrumb_height = DrawBreadcrumbBar(root_to_draw, doc, tab_id, selected);
    const float dialog_tray_height = DrawDialogTray(root_to_draw, doc, tab_id, selected);
    ImVec2 canvas_size = size;
    const float reserved_height = breadcrumb_height + dialog_tray_height;
    if (reserved_height > 0.0f) {
        canvas_size.y = std::max(size.y - reserved_height, 1.0f);
    }

    ImGui::BeginChild("##DesignerCanvas", canvas_size, true, ImGuiWindowFlags_HorizontalScrollbar);

    if (ImGui::IsWindowFocused() && !doc.selected_node_id.empty() && doc.selected_node_id != doc.Root()->NodeId() &&
        ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
        g_canvas_delete_request = {true, tab_id, doc.selected_node_id};
    }

    const ImVec2 avail_before_reserve = ImGui::GetContentRegionAvail();
    ImGui::Dummy(ImVec2(std::max(avail_before_reserve.x, 1.0f), kCanvasTopReserve));

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const Rect canvas_rect{0.0f, 0.0f, std::max(avail.x, 1.0f), std::max(avail.y, 1.0f)};

    studio::design::LiveRenderResult* live_render = nullptr;
    avalang::ui::ImGuiRenderer* live_render_renderer = nullptr;
    {
        const int vw = static_cast<int>(canvas_rect.w);
        const int vh = static_cast<int>(canvas_rect.h);
        if (cache_entry_for_live_render != nullptr) {
            DesignerVmCacheEntry& entry = *cache_entry_for_live_render;
            const bool live_render_stale = !entry.live_render.ok || entry.live_render_w != vw ||
                                            entry.live_render_h != vh || tree_state_rebuilt_this_frame;
            if (live_render_stale) {
                entry.live_render = studio::design::BuildLiveRender(doc.tree.get(), vw, vh,
                                                                       doc.extends, project_root);
                entry.live_render_w = vw;
                entry.live_render_h = vh;
                entry.imgui_renderer = std::make_unique<avalang::ui::ImGuiRenderer>(vw, vh);
            }
            live_render = &entry.live_render;
            live_render_renderer = entry.imgui_renderer.get();
        } else {
            local_live_render = studio::design::BuildLiveRender(doc.tree.get(), vw, vh,
                                                                    doc.extends, project_root);
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

    bool live_render_painted = false;
    if (live_render->ok && live_render->sceneGraph && live_render_renderer) {
        live_render_renderer->SetTarget(ImGui::GetWindowDrawList(), origin);
        avalang::ui::RenderCommandSink sink;
        avalang::ui::SceneCommandWalker::Walk(*live_render->sceneGraph, sink, *live_render_renderer);
        live_render_painted = true;
    }

    if (!live_render->ok) {
        ImGui::TextColored(palette::FromHex(palette::kError), "Error de render: %s",
                            live_render->error.c_str());
        ImGui::TextDisabled("No se pudo reconstruir el layout real -- revisa el arbol/import.");
        ImGui::Separator();
    }

    const std::unordered_map<std::string, avalang::ui::LayoutRect>* uid_to_rect =
        live_render_painted ? &live_render->nodeIdToRect : nullptr;
    std::vector<std::string> missing_rect_uids;
    DrawNode(root_to_draw, origin, doc, selected, tab_id, out_generated_handler, state_vm,
             eval_cache, project_root, live_render_painted, uid_to_rect, &missing_rect_uids);

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
        ImGui::TextColored(palette::FromHex(palette::kWarning),
                            "%d nodo(s) sin layout -- no se dibujaron este frame.",
                            static_cast<int>(missing_rect_uids.size()));
    } else {
        last_logged_missing_rects->clear();
    }

    if (!doc.selected_node_id.empty()) {
        if (avalang::ui::IComponent* found = design::FindNodeById(doc.Root(), doc.selected_node_id)) {
            selected = ToPropertiesState(found, /*editable=*/true, tab_id);
        } else {
            doc.selected_node_id.clear();
            selected.reset();
        }
    } else {
        selected.reset();
    }

    ImGui::EndChild();

    DrawCanvasDeleteConfirmPopup(doc, tab_id, selected);

    if (tab_id < 0 && state_vm) ava_vm_destroy(state_vm);

    return selected;
}

void InvalidateDesignerVmCache(int tab_id) {
    auto it = g_designer_vm_cache.find(tab_id);
    if (it == g_designer_vm_cache.end()) return;
    if (it->second.vm) ava_vm_destroy(it->second.vm);
    g_designer_vm_cache.erase(it);
}

} // namespace studio
