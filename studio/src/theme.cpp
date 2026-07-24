#include "theme.h"

#include "imgui.h"

namespace studio {

void ApplyVSCodeDarkTheme() {
    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // VSCode "Dark+" reference colors.
    const ImVec4 bg_editor      = ImVec4(0.118f, 0.118f, 0.118f, 1.00f); // #1e1e1e
    const ImVec4 bg_sidebar     = ImVec4(0.145f, 0.145f, 0.149f, 1.00f); // #252526
    const ImVec4 bg_titlebar    = ImVec4(0.129f, 0.129f, 0.129f, 1.00f); // #212121
    const ImVec4 bg_input       = ImVec4(0.235f, 0.235f, 0.235f, 1.00f); // #3c3c3c
    const ImVec4 border         = ImVec4(0.078f, 0.078f, 0.078f, 1.00f); // #141414
    const ImVec4 accent         = ImVec4(0.000f, 0.478f, 0.800f, 1.00f); // #007acc
    const ImVec4 accent_hover   = ImVec4(0.110f, 0.588f, 0.910f, 1.00f); // #1c96e8
    const ImVec4 accent_active  = ImVec4(0.055f, 0.388f, 0.639f, 1.00f); // #0e639c
    const ImVec4 tab_active     = ImVec4(0.118f, 0.118f, 0.118f, 1.00f); // #1e1e1e (same as editor -- active tab blends into content)
    const ImVec4 tab_inactive   = ImVec4(0.145f, 0.145f, 0.149f, 1.00f); // #252526
    const ImVec4 text_normal    = ImVec4(0.827f, 0.827f, 0.827f, 1.00f); // #d4d4d4
    const ImVec4 text_disabled  = ImVec4(0.518f, 0.518f, 0.518f, 1.00f); // #848484
    const ImVec4 scrollbar      = ImVec4(0.235f, 0.235f, 0.235f, 0.60f);

    colors[ImGuiCol_Text]                  = text_normal;
    colors[ImGuiCol_TextDisabled]          = text_disabled;
    colors[ImGuiCol_WindowBg]              = bg_editor;
    colors[ImGuiCol_ChildBg]               = bg_editor;
    colors[ImGuiCol_PopupBg]               = bg_sidebar;
    colors[ImGuiCol_Border]                = border;
    colors[ImGuiCol_BorderShadow]          = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_FrameBg]               = bg_input;
    colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.278f, 0.278f, 0.278f, 1.00f);
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.318f, 0.318f, 0.318f, 1.00f);
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
    colors[ImGuiCol_Header]                = ImVec4(0.212f, 0.212f, 0.216f, 1.00f); // list-item hover, e.g. Explorer rows
    colors[ImGuiCol_HeaderHovered]         = ImVec4(0.180f, 0.180f, 0.180f, 1.00f);
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

    style.WindowPadding     = ImVec2(8, 8);
    style.FramePadding      = ImVec2(6, 4);
    style.ItemSpacing       = ImVec2(6, 6);
    style.IndentSpacing     = 16.0f;
    style.ScrollbarSize     = 14.0f;
}

} // namespace studio
