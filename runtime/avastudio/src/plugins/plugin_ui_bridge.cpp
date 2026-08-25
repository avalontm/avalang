#include "plugin_ui_bridge.h"

#include "imgui.h"

#include <cstring>
#include <string>

namespace studio {
namespace plugins_ui {

namespace {

bool g_pending_same_line = false;

void ConsumePendingSameLine(float needed_width) {
    if (!g_pending_same_line) return;
    g_pending_same_line = false;
    ImGui::SameLine();
    if (needed_width > 0.0f && ImGui::GetContentRegionAvail().x < needed_width) {

        ImGui::NewLine();
    }
}

constexpr float kMinFlexibleWidgetWidth = 90.0f;

constexpr float kTrailingButtonReserve = 70.0f;

}

}
}

struct AvaPanelContext {
    const char* panel_name = "";
};

namespace studio {
namespace plugins_ui {

namespace {

extern "C" void Label(AvaPanelContext* , const char* text) {
    ConsumePendingSameLine(0.0f);
    ImGui::TextUnformatted(text ? text : "");
}

extern "C" void TextWrapped(AvaPanelContext* , const char* text) {
    ConsumePendingSameLine(0.0f);
    ImGui::TextWrapped("%s", text ? text : "");
}

extern "C" bool Button(AvaPanelContext* , const char* label) {
    const char* text = label ? label : "";

    const float needed = ImGui::CalcTextSize(text).x + ImGui::GetStyle().FramePadding.x * 2.0f;
    ConsumePendingSameLine(needed);
    return ImGui::Button(text);
}

extern "C" bool InputText(AvaPanelContext* , const char* label, char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) return false;
    ConsumePendingSameLine(kMinFlexibleWidgetWidth);
    return ImGui::InputText(label ? label : "##input", buffer, buffer_size);
}

extern "C" bool InputTextMultiline(AvaPanelContext* , const char* label, char* buffer, size_t buffer_size,
                                    float height) {
    if (!buffer || buffer_size == 0) return false;
    ConsumePendingSameLine(kMinFlexibleWidgetWidth);
    return ImGui::InputTextMultiline(label ? label : "##input_multiline", buffer, buffer_size,
                                      ImVec2(-1.0f, height));
}

struct SubmitFilterState {
    bool submit_requested = false;
};

int SubmitOnEnterCallback(ImGuiInputTextCallbackData* data) {
    auto* filter_state = static_cast<SubmitFilterState*>(data->UserData);
    if (data->EventFlag != ImGuiInputTextFlags_CallbackCharFilter) return 0;
    if (data->EventChar != '\n' && data->EventChar != '\r') return 0;
    if (ImGui::GetIO().KeyShift) return 0;
    filter_state->submit_requested = true;
    return 1;
}

extern "C" bool InputTextMultilineSubmitHint(AvaPanelContext* , const char* label, const char* hint,
                                              char* buffer, size_t buffer_size, float height, bool* out_submit) {
    if (!buffer || buffer_size == 0) return false;
    ConsumePendingSameLine(kMinFlexibleWidgetWidth);
    SubmitFilterState filter_state;

    float avail = ImGui::GetContentRegionAvail().x;
    float width = (avail - kTrailingButtonReserve >= kMinFlexibleWidgetWidth)
                      ? (avail - kTrailingButtonReserve)
                      : -1.0f;
    const char* id = label ? label : "##input_multiline";
    ImVec2 box_pos = ImGui::GetCursorScreenPos();
    bool changed = ImGui::InputTextMultiline(id, buffer, buffer_size,
                                              ImVec2(width, height), ImGuiInputTextFlags_CallbackCharFilter,
                                              &SubmitOnEnterCallback, &filter_state);

    if (hint && hint[0] != '\0' && buffer[0] == '\0' && !ImGui::IsItemActive()) {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        const ImGuiStyle& style = ImGui::GetStyle();
        ImVec2 text_pos(box_pos.x + style.FramePadding.x, box_pos.y + style.FramePadding.y);
        draw_list->AddText(text_pos, ImGui::GetColorU32(ImGuiCol_TextDisabled), hint);
    }
    if (filter_state.submit_requested) {

        ImGui::SetKeyboardFocusHere(-1);
    }
    if (out_submit) *out_submit = filter_state.submit_requested;
    return changed;
}

extern "C" bool InputTextMultilineSubmit(AvaPanelContext* ctx, const char* label, char* buffer,
                                          size_t buffer_size, float height, bool* out_submit) {
    return InputTextMultilineSubmitHint(ctx, label, "", buffer, buffer_size, height, out_submit);
}

extern "C" bool ButtonDisabled(AvaPanelContext* , const char* label, bool disabled) {
    const char* text = label ? label : "";
    const float needed = ImGui::CalcTextSize(text).x + ImGui::GetStyle().FramePadding.x * 2.0f;
    ConsumePendingSameLine(needed);
    if (disabled) ImGui::BeginDisabled();
    bool clicked = ImGui::Button(text);
    if (disabled) ImGui::EndDisabled();
    return clicked && !disabled;
}

extern "C" float TextLineHeight(AvaPanelContext* ) {
    return ImGui::GetTextLineHeightWithSpacing();
}

extern "C" void TextColored(AvaPanelContext* , const char* text, float r, float g, float b, float a) {
    ConsumePendingSameLine(0.0f);
    ImGui::TextColored(ImVec4(r, g, b, a), "%s", text ? text : "");
}

std::string WordWrapText(const char* text, float wrap_width) {
    std::string result;
    if (!text || !*text) return result;
    result.reserve(std::strlen(text) + 16);

    ImFont* font = ImGui::GetFont();
    const char* text_end = text + std::strlen(text);
    const char* p = text;
    while (p < text_end) {
        const char* line_end = p;
        while (line_end < text_end && *line_end != '\n') ++line_end;

        const char* seg = p;
        while (seg < line_end) {
            const char* wrap_pos = font->CalcWordWrapPositionA(1.0f, seg, line_end, wrap_width);
            if (wrap_pos <= seg) wrap_pos = seg + 1;
            result.append(seg, wrap_pos);
            seg = wrap_pos;

            if (seg < line_end && *seg == ' ') ++seg;
            if (seg < line_end) result += '\n';
        }
        if (line_end < text_end) { result += '\n'; p = line_end + 1; }
        else p = line_end;
    }
    return result;
}

extern "C" void SelectableMessage(AvaPanelContext* , const char* id, const char* text,
                                   float text_r, float text_g, float text_b, float text_a,
                                   float bg_r, float bg_g, float bg_b, float bg_a) {
    ConsumePendingSameLine(0.0f);
    if (!text) text = "";
    if (!id || id[0] == '\0') id = "##msg";

    const ImGuiStyle& style = ImGui::GetStyle();

    float wrap_width = ImGui::GetContentRegionAvail().x - style.FramePadding.x * 2.0f;
    if (wrap_width < 40.0f) wrap_width = 40.0f;

    std::string wrapped = WordWrapText(text, wrap_width);

    int line_count = 1;
    for (char c : wrapped) {
        if (c == '\n') ++line_count;
    }
    float height = line_count * ImGui::GetTextLineHeight() + style.FramePadding.y * 2.0f + 4.0f;

    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(bg_r, bg_g, bg_b, bg_a));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(text_r, text_g, text_b, text_a));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

    ImGui::InputTextMultiline(id, const_cast<char*>(wrapped.c_str()), wrapped.size() + 1, ImVec2(-1.0f, height),
                               ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_NoHorizontalScroll);

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
}

extern "C" bool Combo(AvaPanelContext* , const char* label, int* current_index, const char* const* items,
                       int items_count) {
    if (!current_index || !items || items_count <= 0) return false;
    ConsumePendingSameLine(kMinFlexibleWidgetWidth);
    return ImGui::Combo(label ? label : "##combo", current_index, items, items_count);
}

extern "C" void Separator(AvaPanelContext* ) {
    ConsumePendingSameLine(0.0f);
    ImGui::Separator();
}

extern "C" void SameLine(AvaPanelContext* ) {

    g_pending_same_line = true;
}

extern "C" void Spacing(AvaPanelContext* ) {
    ConsumePendingSameLine(0.0f);
    ImGui::Spacing();
}

extern "C" bool BeginChild(AvaPanelContext* , const char* id, float height) {
    ConsumePendingSameLine(0.0f);
    return ImGui::BeginChild(id ? id : "##child", ImVec2(0.0f, height), true);
}

extern "C" void EndChild(AvaPanelContext* ) {
    ImGui::EndChild();
}

extern "C" void ScrollToBottom(AvaPanelContext* ) {

    if (ImGui::GetScrollMaxY() > 0.0f) {
        ImGui::SetScrollHereY(1.0f);
    }
}

}

void FillUiApi(AvaUiApi& ui) {
    ui.label = &Label;
    ui.text_wrapped = &TextWrapped;
    ui.button = &Button;
    ui.input_text = &InputText;
    ui.input_text_multiline = &InputTextMultiline;
    ui.combo = &Combo;
    ui.separator = &Separator;
    ui.same_line = &SameLine;
    ui.spacing = &Spacing;
    ui.begin_child = &BeginChild;
    ui.end_child = &EndChild;
    ui.scroll_to_bottom = &ScrollToBottom;
    ui.input_text_multiline_submit = &InputTextMultilineSubmit;
    ui.input_text_multiline_submit_hint = &InputTextMultilineSubmitHint;
    ui.button_disabled = &ButtonDisabled;
    ui.text_line_height = &TextLineHeight;
    ui.text_colored = &TextColored;
    ui.selectable_message = &SelectableMessage;
}

AvaPanelContext* BeginPanelContext(const char* panel_name) {

    static AvaPanelContext ctx;
    ctx.panel_name = panel_name ? panel_name : "";

    g_pending_same_line = false;
    ImGui::PushID(ctx.panel_name);
    return &ctx;
}

void EndPanelContext(AvaPanelContext* ) {
    ImGui::PopID();
}

}
}
