#pragma once

#include "imgui.h"

namespace studio::util {

// Returns a button size wide enough to fit `label` without clipping.
// ImGui::Button(label, size) does NOT grow past an explicit size, so a
// fixed width tuned for the English string clips or overflows once the
// label is translated to a longer language (e.g. "File" -> "Archivo").
// This keeps every call site auto-sized to whatever text is currently
// active, while still respecting a minimum width for visual rhythm.
inline ImVec2 AutoButtonSize(const char* label, float min_width = 0.0f, float extra_padding = 16.0f) {
    const float text_w = ImGui::CalcTextSize(label).x;
    const float w = text_w + extra_padding + ImGui::GetStyle().FramePadding.x * 2.0f;
    return ImVec2(w > min_width ? w : min_width, ImGui::GetFrameHeight());
}

}  // namespace studio::util
