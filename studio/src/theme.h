#pragma once

// Applies a dark theme modeled on VSCode's "Dark+" palette (colors,
// rounding, spacing) so Ava Studio feels immediately familiar to
// anyone coming from VSCode. Call once after ImGui::CreateContext().
namespace studio {

void ApplyVSCodeDarkTheme();

} // namespace studio
