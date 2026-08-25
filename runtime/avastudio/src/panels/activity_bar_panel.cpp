#include "panels/activity_bar_panel.h"

#include <cmath>

#include "imgui.h"

#include "palette.h"
#include "util/i18n.h"

namespace studio {

namespace {

constexpr unsigned int kBarBg = 0x0B0B0D;
constexpr float kActiveBarWidth = 3.0f;

// Same InvisibleButton + ImDrawList idiom titlebar_panel.cpp already uses
// for its caption buttons (CaptionButton) -- kept as a separate copy here
// rather than shared, same call titlebar_panel.cpp itself makes for its
// button helper (file-local, no new coupling between the two bars).
template <typename DrawIcon>
bool ActivityIconButton(const char* id, float width, float height, bool active, const std::string& tooltip,
                         DrawIcon draw_icon) {
    ImGui::PushID(id);
    ImGui::InvisibleButton("##btn", ImVec2(width, height));
    const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    const bool hovered = ImGui::IsItemHovered();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p0 = ImGui::GetItemRectMin();
    const ImVec2 p1 = ImGui::GetItemRectMax();

    if (hovered) {
        dl->AddRectFilled(p0, p1, palette::U32FromHex(palette::kCard));
    }
    if (active) {
        dl->AddRectFilled(p0, ImVec2(p0.x + kActiveBarWidth, p1.y), palette::U32FromHex(palette::kPrimary));
    }

    const ImU32 icon_color = palette::U32FromHex(active ? palette::kTextPrimary : palette::kTextSecondary);
    const ImVec2 center((p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f);
    draw_icon(dl, center, icon_color);

    if (hovered && !tooltip.empty()) {
        ImGui::SetTooltip("%s", tooltip.c_str());
    }

    ImGui::PopID();
    return clicked;
}

void DrawExplorerIcon(ImDrawList* dl, ImVec2 c, ImU32 col) {
    const float half_w = 7.0f;
    const float top = c.y - 6.0f;
    const float bottom = c.y + 6.0f;
    dl->AddRect(ImVec2(c.x - half_w, top + 2.0f), ImVec2(c.x + half_w, bottom), col, 1.5f, 0, 1.4f);
    dl->AddLine(ImVec2(c.x - half_w, top + 2.0f), ImVec2(c.x - half_w + 4.0f, top + 2.0f), col, 1.4f);
    dl->AddLine(ImVec2(c.x - half_w + 4.0f, top + 2.0f), ImVec2(c.x - half_w + 6.0f, top), col, 1.4f);
    dl->AddLine(ImVec2(c.x - half_w + 6.0f, top), ImVec2(c.x + half_w - 3.0f, top), col, 1.4f);
}

void DrawSearchIcon(ImDrawList* dl, ImVec2 c, ImU32 col) {
    const ImVec2 glass_center(c.x - 2.0f, c.y - 2.0f);
    dl->AddCircle(glass_center, 5.0f, col, 16, 1.4f);
    const ImVec2 handle_start(glass_center.x + 3.6f, glass_center.y + 3.6f);
    dl->AddLine(handle_start, ImVec2(c.x + 6.0f, c.y + 6.0f), col, 1.7f);
}

void DrawToolboxIcon(ImDrawList* dl, ImVec2 c, ImU32 col) {
    const float s = 4.0f;
    const float gap = 2.0f;
    const ImVec2 offsets[4] = {
        ImVec2(-s - gap * 0.5f, -s - gap * 0.5f),
        ImVec2(gap * 0.5f, -s - gap * 0.5f),
        ImVec2(-s - gap * 0.5f, gap * 0.5f),
        ImVec2(gap * 0.5f, gap * 0.5f),
    };
    for (const ImVec2& off : offsets) {
        dl->AddRectFilled(ImVec2(c.x + off.x, c.y + off.y), ImVec2(c.x + off.x + s, c.y + off.y + s), col, 1.0f);
    }
}

void DrawExtensionsIcon(ImDrawList* dl, ImVec2 c, ImU32 col) {
    const ImVec2 pts[4] = {
        ImVec2(c.x, c.y - 7.5f),
        ImVec2(c.x + 7.5f, c.y),
        ImVec2(c.x, c.y + 7.5f),
        ImVec2(c.x - 7.5f, c.y),
    };
    dl->AddPolyline(pts, 4, col, ImDrawFlags_Closed, 1.4f);
}

void DrawSettingsIcon(ImDrawList* dl, ImVec2 c, ImU32 col) {
    dl->AddCircle(c, 4.5f, col, 12, 1.4f);
    dl->AddCircle(c, 1.6f, col, 8, 1.2f);
    constexpr int kTeeth = 8;
    for (int i = 0; i < kTeeth; ++i) {
        const float angle = (static_cast<float>(i) / static_cast<float>(kTeeth)) * 2.0f * 3.14159265f;
        const float cos_a = std::cos(angle);
        const float sin_a = std::sin(angle);
        const ImVec2 p0(c.x + cos_a * 6.0f, c.y + sin_a * 6.0f);
        const ImVec2 p1(c.x + cos_a * 8.2f, c.y + sin_a * 8.2f);
        dl->AddLine(p0, p1, col, 1.3f);
    }
}

}

ActivityBarResult DrawActivityBar(float pos_x, float pos_y, float width, float height, bool explorer_open,
                                   bool search_open, bool toolbox_open, bool settings_open) {
    ActivityBarResult result;

    ImGui::SetNextWindowPos(ImVec2(pos_x, pos_y));
    ImGui::SetNextWindowSize(ImVec2(width, height));

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                    ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings |
                                    ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
                                    ImGuiWindowFlags_NoDocking;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, palette::FromHex(kBarBg));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 8.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("##ActivityBar", nullptr, flags);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();

    const float button_h = width;

    if (ActivityIconButton("activity_explorer", width, button_h, explorer_open, util::Tr("panel.explorer.title"),
                            DrawExplorerIcon)) {
        result.explorer_clicked = true;
    }
    if (ActivityIconButton("activity_search", width, button_h, search_open, util::Tr("panel.find_in_project.title"),
                            DrawSearchIcon)) {
        result.search_clicked = true;
    }
    if (ActivityIconButton("activity_toolbox", width, button_h, toolbox_open, util::Tr("panel.toolbox.title"),
                            DrawToolboxIcon)) {
        result.toolbox_clicked = true;
    }

    // Extensions/Settings pinned to the bottom of the strip, same "primary
    // views up top, secondary/global stuff at the bottom" placement VS
    // Code uses for its own Accounts/Settings icons.
    ImGui::SetCursorPosY(height - button_h * 2.0f - ImGui::GetStyle().WindowPadding.y);

    if (ActivityIconButton("activity_extensions", width, button_h, false, util::Tr("menu.file.extensions"),
                            DrawExtensionsIcon)) {
        result.extensions_clicked = true;
    }
    if (ActivityIconButton("activity_settings", width, button_h, settings_open, util::Tr("panel.settings.title"),
                            DrawSettingsIcon)) {
        result.settings_clicked = true;
    }

    ImGui::End();
    return result;
}

}
