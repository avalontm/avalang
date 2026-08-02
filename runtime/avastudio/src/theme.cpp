#include "theme.h"

#include "imgui.h"

namespace studio {

void ApplyVSCodeDarkTheme() {
    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // "Ava amber" -- orange-dominant theme. Chrome (backgrounds, borders,
    // panels) uses a neutral, almost-pure-black palette in the style of
    // terminal-style IDEs like OpenCode -- cool near-black instead of the
    // previous warm brown -- so the orange accent (buttons, active tab,
    // syntax highlighting) is the only thing carrying color and really
    // pops against it.
    const ImVec4 bg_editor      = ImVec4(0.043f, 0.043f, 0.051f, 1.00f); // #0b0b0d
    const ImVec4 bg_sidebar     = ImVec4(0.063f, 0.063f, 0.075f, 1.00f); // #101013
    const ImVec4 bg_titlebar    = ImVec4(0.055f, 0.055f, 0.063f, 1.00f); // #0e0e10
    const ImVec4 bg_input       = ImVec4(0.110f, 0.110f, 0.125f, 1.00f); // #1c1c20
    const ImVec4 border         = ImVec4(0.161f, 0.161f, 0.180f, 1.00f); // #29292e
    const ImVec4 accent         = ImVec4(0.910f, 0.478f, 0.204f, 1.00f); // #e87a34
    const ImVec4 accent_hover   = ImVec4(1.000f, 0.596f, 0.290f, 1.00f); // #ff984a
    const ImVec4 accent_active  = ImVec4(0.761f, 0.376f, 0.129f, 1.00f); // #c26021
    const ImVec4 tab_active     = ImVec4(0.043f, 0.043f, 0.051f, 1.00f); // #0b0b0d (same as editor -- active tab blends into content)
    const ImVec4 tab_inactive   = ImVec4(0.063f, 0.063f, 0.075f, 1.00f); // #101013
    const ImVec4 text_normal    = ImVec4(0.902f, 0.902f, 0.910f, 1.00f); // #e6e6e8
    const ImVec4 text_disabled  = ImVec4(0.420f, 0.420f, 0.439f, 1.00f); // #6b6b70
    const ImVec4 scrollbar      = ImVec4(0.180f, 0.180f, 0.200f, 0.60f);

    colors[ImGuiCol_Text]                  = text_normal;
    colors[ImGuiCol_TextDisabled]          = text_disabled;
    colors[ImGuiCol_WindowBg]              = bg_editor;
    colors[ImGuiCol_ChildBg]               = bg_editor;
    colors[ImGuiCol_PopupBg]               = bg_sidebar;
    colors[ImGuiCol_Border]                = border;
    colors[ImGuiCol_BorderShadow]          = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_FrameBg]               = bg_input;
    colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.165f, 0.165f, 0.188f, 1.00f); // #2a2a30
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.204f, 0.204f, 0.235f, 1.00f); // #34343c
    colors[ImGuiCol_TitleBg]               = bg_titlebar;
    colors[ImGuiCol_TitleBgActive]         = bg_titlebar;
    colors[ImGuiCol_TitleBgCollapsed]      = bg_titlebar;
    colors[ImGuiCol_MenuBarBg]             = bg_titlebar;
    colors[ImGuiCol_ScrollbarBg]           = bg_editor;
    colors[ImGuiCol_ScrollbarGrab]         = scrollbar;
    colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.313f, 0.313f, 0.313f, 0.80f);
    colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.400f, 0.400f, 0.400f, 0.80f);
    colors[ImGuiCol_CheckMark]             = accent_hover;
    colors[ImGuiCol_SliderGrab]            = accent;
    colors[ImGuiCol_SliderGrabActive]      = accent_hover;
    colors[ImGuiCol_Button]                = bg_input;
    colors[ImGuiCol_ButtonHovered]         = accent_active;
    colors[ImGuiCol_ButtonActive]          = accent;
    colors[ImGuiCol_Header]                = ImVec4(0.102f, 0.102f, 0.122f, 1.00f); // #1a1a1f -- list-item hover, e.g. Explorer rows
    colors[ImGuiCol_HeaderHovered]         = ImVec4(0.090f, 0.090f, 0.106f, 1.00f); // #17171b
    colors[ImGuiCol_HeaderActive]          = accent_active;
    colors[ImGuiCol_Separator]             = border;
    colors[ImGuiCol_SeparatorHovered]      = accent_hover;
    colors[ImGuiCol_SeparatorActive]       = accent;
    colors[ImGuiCol_ResizeGrip]            = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_ResizeGripHovered]     = accent_active;
    colors[ImGuiCol_ResizeGripActive]      = accent;
    colors[ImGuiCol_Tab]                   = tab_inactive;
    colors[ImGuiCol_TabHovered]            = accent_active;
    colors[ImGuiCol_TabActive]             = tab_active;
    colors[ImGuiCol_TabUnfocused]          = tab_inactive;
    colors[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.165f, 0.165f, 0.165f, 1.00f);
    colors[ImGuiCol_DockingPreview]        = ImVec4(accent.x, accent.y, accent.z, 0.35f);
    colors[ImGuiCol_DockingEmptyBg]        = bg_sidebar;
    colors[ImGuiCol_PlotLines]             = accent_hover;
    colors[ImGuiCol_PlotHistogram]         = accent_hover;
    colors[ImGuiCol_TextSelectedBg]        = ImVec4(accent.x, accent.y, accent.z, 0.35f);
    colors[ImGuiCol_NavHighlight]          = accent_hover;

    // VSCode is a flat, mostly-square UI -- small rounding only, tight
    // borders instead of heavy padding.
    style.WindowRounding    = 4.0f;
    style.ChildRounding     = 0.0f;
    style.FrameRounding     = 2.0f;
    style.PopupRounding     = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding      = 2.0f;
    style.TabRounding       = 4.0f;

    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize  = 0.0f;
    style.PopupBorderSize  = 1.0f;

    style.WindowPadding     = ImVec2(10, 10);
    style.FramePadding      = ImVec2(6, 4);
    style.ItemSpacing       = ImVec2(6, 6);
    style.IndentSpacing     = 16.0f;
    style.ScrollbarSize     = 14.0f;
}

} // namespace studio
