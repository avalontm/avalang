#include "plugin_ui_bridge.h"

#include "imgui.h"

#include <cstring>
#include <string>

namespace studio {
namespace plugins_ui {

namespace {

// Full definition lives only here -- plugin_api.h only forward-declares
// `AvaPanelContext` as an opaque type, so a plugin (which never
// includes this file) has no way to look inside it even though it's
// handed the pointer on every UI call.

// SameLine() (below) doesn't call ImGui::SameLine() immediately -- it
// just remembers that the *next* widget asked to share the previous
// line, so that widget's own call can decide whether there's actually
// room for it. A narrow docked panel (see the "Plugins" panels docked
// at AVA_DOCK_BOTTOM, which end up sharing width with Explorer/Editor/
// Output) is exactly the case where an input_text()+same_line()+
// button() row -- a completely reasonable thing for a plugin to write
// -- doesn't fit, and ImGui's own SameLine() has no built-in concept
// of "wrap to a new line instead"; it just lets the next item's box
// extend past the window's right edge, clipped by the parent
// scroll region with no way back to it short of a horizontal
// scrollbar (bad UX for reading text/using a form either way).
bool g_pending_same_line = false;

// Called from every widget function that could legitimately follow
// same_line() (button/input_text/input_text_multiline/combo).
// `needed_width`: how much horizontal room this widget wants on the
// current line to render fully -- pass 0 to always honor the same-line
// placement without a wrap check (used for widgets where "just shrink
// a bit" reads fine, unlike a button/label whose text would be cut).
void ConsumePendingSameLine(float needed_width) {
    if (!g_pending_same_line) return;
    g_pending_same_line = false;
    ImGui::SameLine();
    if (needed_width > 0.0f && ImGui::GetContentRegionAvail().x < needed_width) {
        // Not enough room left on this line -- drop to a new one
        // instead of drawing (and clipping) past the panel's edge.
        ImGui::NewLine();
    }
}

// Minimum width a text field/combo needs to still be usable once
// shrunk -- below this it's easier to read on its own line than
// squeezed next to whatever came before it. Buttons/labels use their
// exact rendered width instead (see ConsumePendingSameLine's callers
// below), since those can't shrink at all.
constexpr float kMinFlexibleWidgetWidth = 90.0f;

// Room reserved on the input's own line for the send/stop button that
// always follows input_text_multiline_submit() (see its header comment
// -- it exists specifically for that "type + Enviar" chat row). Sized
// to "Detener" (the longer of the two labels actually used) plus frame
// padding and the SameLine gap -- any more than that just leaves a gap
// between the input and the button instead of the button sitting right
// after it.
constexpr float kTrailingButtonReserve = 70.0f;

} // namespace

} // namespace plugins_ui
} // namespace studio

// AvaPanelContext's definition must be visible wherever plugin_api.h's
// forward declaration is used with a real member access -- since that
// forward declaration lives in the (extern "C") global namespace, the
// definition does too.
struct AvaPanelContext {
    const char* panel_name = "";
};

namespace studio {
namespace plugins_ui {

namespace {

extern "C" void Label(AvaPanelContext* /*ctx*/, const char* text) {
    ConsumePendingSameLine(0.0f); // no wrap check -- a label can't shrink, but this was always the caller's call
    ImGui::TextUnformatted(text ? text : "");
}

extern "C" void TextWrapped(AvaPanelContext* /*ctx*/, const char* text) {
    ConsumePendingSameLine(0.0f);
    ImGui::TextWrapped("%s", text ? text : "");
}

extern "C" bool Button(AvaPanelContext* /*ctx*/, const char* label) {
    const char* text = label ? label : "";
    // A button can't shrink -- it needs its full rendered width (label
    // text + the frame padding ImGui adds on both sides) or it wraps to
    // its own line instead of getting clipped mid-label (see the
    // "Saludar" -> "Saluc" clipping this fixes).
    const float needed = ImGui::CalcTextSize(text).x + ImGui::GetStyle().FramePadding.x * 2.0f;
    ConsumePendingSameLine(needed);
    return ImGui::Button(text);
}

extern "C" bool InputText(AvaPanelContext* /*ctx*/, const char* label, char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) return false;
    ConsumePendingSameLine(kMinFlexibleWidgetWidth);
    return ImGui::InputText(label ? label : "##input", buffer, buffer_size);
}

extern "C" bool InputTextMultiline(AvaPanelContext* /*ctx*/, const char* label, char* buffer, size_t buffer_size,
                                    float height) {
    if (!buffer || buffer_size == 0) return false;
    ConsumePendingSameLine(kMinFlexibleWidgetWidth);
    return ImGui::InputTextMultiline(label ? label : "##input_multiline", buffer, buffer_size,
                                      ImVec2(-1.0f, height));
}

// Dear ImGui has no built-in "Enter submits, Shift+Enter inserts a
// newline" mode for multi-line inputs -- only a Ctrl+Enter variant
// (ImGuiInputTextFlags_CtrlEnterForNewLine), which isn't the chat
// convention every user already knows from Slack/Discord/etc. This is
// the standard ImGui workaround: a CallbackCharFilter runs *before* a
// keystroke is applied to the buffer, so it can inspect the '\n' that
// Enter produces and either swallow it (plain Enter -- flag it as a
// submit instead of a newline) or let it through (Shift+Enter, held at
// the moment the callback fires -- same frame as the keypress).
struct SubmitFilterState {
    bool submit_requested = false;
};

int SubmitOnEnterCallback(ImGuiInputTextCallbackData* data) {
    auto* filter_state = static_cast<SubmitFilterState*>(data->UserData);
    if (data->EventFlag != ImGuiInputTextFlags_CallbackCharFilter) return 0;
    if (data->EventChar != '\n' && data->EventChar != '\r') return 0;
    if (ImGui::GetIO().KeyShift) return 0; // Shift+Enter: let the newline through as usual.
    filter_state->submit_requested = true;
    return 1; // Reject the character -- don't insert a newline for a plain Enter.
}

extern "C" bool InputTextMultilineSubmitHint(AvaPanelContext* /*ctx*/, const char* label, const char* hint,
                                              char* buffer, size_t buffer_size, float height, bool* out_submit) {
    if (!buffer || buffer_size == 0) return false;
    ConsumePendingSameLine(kMinFlexibleWidgetWidth);
    SubmitFilterState filter_state;
    // -1.0f (full available width) would leave nothing for the send/stop
    // button that's always drawn right after this via same_line() --
    // that button would then wrap to its own line below instead of
    // sitting beside the input like every other chat app. Reserve room
    // for it here instead, and only fall back to full width if the
    // panel is too narrow for that to make sense anyway.
    float avail = ImGui::GetContentRegionAvail().x;
    float width = (avail - kTrailingButtonReserve >= kMinFlexibleWidgetWidth)
                      ? (avail - kTrailingButtonReserve)
                      : -1.0f;
    const char* id = label ? label : "##input_multiline";
    ImVec2 box_pos = ImGui::GetCursorScreenPos();
    bool changed = ImGui::InputTextMultiline(id, buffer, buffer_size,
                                              ImVec2(width, height), ImGuiInputTextFlags_CallbackCharFilter,
                                              &SubmitOnEnterCallback, &filter_state);
    // Dear ImGui's hint overlay (InputTextWithHint) only exists for the
    // single-line widget -- there's no multiline equivalent to call, so
    // draw the greyed placeholder by hand on top of the box, in the
    // same spot InputTextWithHint puts it, only while empty and not
    // focused (so it never fights with the real caret/text).
    if (hint && hint[0] != '\0' && buffer[0] == '\0' && !ImGui::IsItemActive()) {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        const ImGuiStyle& style = ImGui::GetStyle();
        ImVec2 text_pos(box_pos.x + style.FramePadding.x, box_pos.y + style.FramePadding.y);
        draw_list->AddText(text_pos, ImGui::GetColorU32(ImGuiCol_TextDisabled), hint);
    }
    if (filter_state.submit_requested) {
        // The caller (SendMessage) clears `buffer` right after this
        // returns, but this widget is still the active one -- ImGui
        // won't pick up that external change until it reactivates.
        // SetKeyboardFocusHere(-1) re-targets the item just drawn
        // (this input) for activation next frame, which is what makes
        // it redraw from the now-empty buffer instead of showing its
        // own stale internal copy. Same pattern as terminal_panel.cpp's
        // console input.
        ImGui::SetKeyboardFocusHere(-1);
    }
    if (out_submit) *out_submit = filter_state.submit_requested;
    return changed;
}

extern "C" bool InputTextMultilineSubmit(AvaPanelContext* ctx, const char* label, char* buffer,
                                          size_t buffer_size, float height, bool* out_submit) {
    return InputTextMultilineSubmitHint(ctx, label, "", buffer, buffer_size, height, out_submit);
}

extern "C" bool ButtonDisabled(AvaPanelContext* /*ctx*/, const char* label, bool disabled) {
    const char* text = label ? label : "";
    const float needed = ImGui::CalcTextSize(text).x + ImGui::GetStyle().FramePadding.x * 2.0f;
    ConsumePendingSameLine(needed);
    if (disabled) ImGui::BeginDisabled();
    bool clicked = ImGui::Button(text);
    if (disabled) ImGui::EndDisabled();
    return clicked && !disabled;
}

extern "C" float TextLineHeight(AvaPanelContext* /*ctx*/) {
    return ImGui::GetTextLineHeightWithSpacing();
}

extern "C" void TextColored(AvaPanelContext* /*ctx*/, const char* text, float r, float g, float b, float a) {
    ConsumePendingSameLine(0.0f);
    ImGui::TextColored(ImVec4(r, g, b, a), "%s", text ? text : "");
}

// Manually wraps `text` to `wrap_width`, inserting '\n' at the same break
// points ImGui::TextWrapped() would pick (ImFont::CalcWordWrapPositionA is
// the same helper TextWrapped uses internally) -- unlike TextWrapped,
// InputTextMultiline has no wrap mode of its own: every line already in
// the buffer is treated as one visual line verbatim. Without this, a
// message with no embedded '\n' rendered as one single line that either
// scrolled sideways or, with NoHorizontalScroll set (see
// selectable_message below), just got clipped at the edge -- which is
// exactly the "no salto de línea, se sale del chat" bug this fixes.
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
            if (wrap_pos <= seg) wrap_pos = seg + 1; // always advances, even for one very long "word"
            result.append(seg, wrap_pos);
            seg = wrap_pos;
            // Skip the single space that caused the break, same as
            // TextWrapped does -- otherwise every wrapped line but the
            // first would start with a stray leading space.
            if (seg < line_end && *seg == ' ') ++seg;
            if (seg < line_end) result += '\n';
        }
        if (line_end < text_end) { result += '\n'; p = line_end + 1; }
        else p = line_end;
    }
    return result;
}

// Fase 10: see plugin_api.h's comment on selectable_message for the
// "why" -- this is what actually renders it. A read-only
// InputTextMultiline instead of TextWrapped is the standard Dear ImGui
// way to get real selection/copy out of a block of text; TextWrapped
// has no selection model at all.
extern "C" void SelectableMessage(AvaPanelContext* /*ctx*/, const char* id, const char* text,
                                   float text_r, float text_g, float text_b, float text_a,
                                   float bg_r, float bg_g, float bg_b, float bg_a) {
    ConsumePendingSameLine(0.0f);
    if (!text) text = "";
    if (!id || id[0] == '\0') id = "##msg";

    const ImGuiStyle& style = ImGui::GetStyle();

    float wrap_width = ImGui::GetContentRegionAvail().x - style.FramePadding.x * 2.0f;
    if (wrap_width < 40.0f) wrap_width = 40.0f;

    // Wrapped once here (not left to the widget, which can't do it --
    // see WordWrapText's comment) and reused for both what's actually
    // drawn and the height measurement below, so the box is always sized
    // to exactly the lines it displays, hard-wrapped included.
    std::string wrapped = WordWrapText(text, wrap_width);

    int line_count = 1;
    for (char c : wrapped) {
        if (c == '\n') ++line_count;
    }
    float height = line_count * ImGui::GetTextLineHeight() + style.FramePadding.y * 2.0f + 4.0f;

    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(bg_r, bg_g, bg_b, bg_a));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(text_r, text_g, text_b, text_a));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

    // ReadOnly: this is a copyable label, not an editable field -- the
    // goal is letting the user drag-select/Ctrl+C their own chat
    // history, not rewrite it. const_cast is safe here: ImGui never
    // writes through a ReadOnly buffer, it only needs a non-const
    // pointer to match the same InputText signature used by editable
    // callers.
    ImGui::InputTextMultiline(id, const_cast<char*>(wrapped.c_str()), wrapped.size() + 1, ImVec2(-1.0f, height),
                               ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_NoHorizontalScroll);

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
}

extern "C" bool Combo(AvaPanelContext* /*ctx*/, const char* label, int* current_index, const char* const* items,
                       int items_count) {
    if (!current_index || !items || items_count <= 0) return false;
    ConsumePendingSameLine(kMinFlexibleWidgetWidth);
    return ImGui::Combo(label ? label : "##combo", current_index, items, items_count);
}

extern "C" void Separator(AvaPanelContext* /*ctx*/) {
    ConsumePendingSameLine(0.0f);
    ImGui::Separator();
}

extern "C" void SameLine(AvaPanelContext* /*ctx*/) {
    // Deferred -- see g_pending_same_line's comment above. The actual
    // ImGui::SameLine() call happens inside whichever widget function
    // is called next, once that widget knows whether it actually has
    // room.
    g_pending_same_line = true;
}

extern "C" void Spacing(AvaPanelContext* /*ctx*/) {
    ConsumePendingSameLine(0.0f);
    ImGui::Spacing();
}

extern "C" bool BeginChild(AvaPanelContext* /*ctx*/, const char* id, float height) {
    ConsumePendingSameLine(0.0f);
    return ImGui::BeginChild(id ? id : "##child", ImVec2(0.0f, height), true);
}

extern "C" void EndChild(AvaPanelContext* /*ctx*/) {
    ImGui::EndChild();
}

extern "C" void ScrollToBottom(AvaPanelContext* /*ctx*/) {
    // Only meaningful while a scroll region is open (between
    // BeginChild/EndChild above) -- same requirement ImGui itself has
    // for GetScrollMaxY()/SetScrollHereY().
    if (ImGui::GetScrollMaxY() > 0.0f) {
        ImGui::SetScrollHereY(1.0f);
    }
}

} // namespace

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
    // Reused across calls rather than heap-allocated per panel: panels
    // are drawn synchronously, one at a time, on the main thread (see
    // main.cpp's loop over PluginHost::Panels()) -- each Begin/End
    // pair fully completes before the next panel's starts, so a single
    // reused instance is safe and avoids an allocation every frame per
    // panel. ImGui::PushID scopes every widget id inside this panel
    // (its "##input"/"##combo"/etc defaults above) so two different
    // plugins' panels never collide.
    static AvaPanelContext ctx;
    ctx.panel_name = panel_name ? panel_name : "";
    // Defensive reset: if a misbehaving plugin's draw() calls
    // same_line() as its very last UI call (nothing to actually place
    // next to), g_pending_same_line would otherwise leak into the next
    // plugin panel drawn this frame and wrongly glue its first widget
    // onto whatever line this panel last drew.
    g_pending_same_line = false;
    ImGui::PushID(ctx.panel_name);
    return &ctx;
}

void EndPanelContext(AvaPanelContext* /*ctx*/) {
    ImGui::PopID();
}

} // namespace plugins_ui
} // namespace studio
