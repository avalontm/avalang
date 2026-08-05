#include "panels/titlebar_panel.h"

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <string>
#include <string_view>

#include "imgui.h"

#include "branding/logo_texture.h"
#include "palette.h"
#include "panels/builtin_panels.h"
#include "panels/editor_panel.h"
#include "platform/win32_titlebar.h"
#include "plugins/plugin_host.h"
#include "util/settings.h"

namespace studio {

namespace {

constexpr unsigned int kTitleBarBg = 0x0E0E10; // matches theme.cpp's bg_titlebar
constexpr float kButtonWidth = 46.0f;

ScreenRect ItemScreenRect() {
    const ImVec2 p0 = ImGui::GetItemRectMin();
    const ImVec2 p1 = ImGui::GetItemRectMax();
    return ScreenRect{p0.x, p0.y, p1.x, p1.y};
}

// Flat window-control button (minimize/maximize/close): an invisible
// button (so we control the exact hover/hit rect ourselves) plus a
// small hand-drawn glyph, since relying on font glyph coverage for
// "-", a square, or "x" is fragile across fonts and platforms -- VSCode
// draws its own caption glyphs the same way.
template <typename DrawIcon>
bool CaptionButton(const char* id, float width, float height, ImU32 hover_bg, DrawIcon draw_icon) {
    ImGui::PushID(id);
    ImGui::InvisibleButton("##btn", ImVec2(width, height));
    const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    const bool hovered = ImGui::IsItemHovered();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p0 = ImGui::GetItemRectMin();
    const ImVec2 p1 = ImGui::GetItemRectMax();
    if (hovered) {
        dl->AddRectFilled(p0, p1, hover_bg);
    }
    draw_icon(dl, p0, p1);

    ImGui::PopID();
    return clicked;
}

void DrawMinimizeIcon(ImDrawList* dl, ImVec2 p0, ImVec2 p1) {
    const float cx = (p0.x + p1.x) * 0.5f;
    const float cy = (p0.y + p1.y) * 0.5f;
    const float half = 5.0f;
    const ImU32 col = palette::U32FromHex(palette::kTextSecondary);
    dl->AddLine(ImVec2(cx - half, cy), ImVec2(cx + half, cy), col, 1.2f);
}

void DrawMaximizeIcon(ImDrawList* dl, ImVec2 p0, ImVec2 p1) {
    const float cx = (p0.x + p1.x) * 0.5f;
    const float cy = (p0.y + p1.y) * 0.5f;
    const float half = 4.5f;
    const ImU32 col = palette::U32FromHex(palette::kTextSecondary);
    dl->AddRect(ImVec2(cx - half, cy - half), ImVec2(cx + half, cy + half), col, 0.0f, 0, 1.2f);
}

void DrawRestoreIcon(ImDrawList* dl, ImVec2 p0, ImVec2 p1) {
    const float cx = (p0.x + p1.x) * 0.5f;
    const float cy = (p0.y + p1.y) * 0.5f;
    const float half = 4.0f;
    const ImU32 col = palette::U32FromHex(palette::kTextSecondary);
    const ImU32 bg = palette::U32FromHex(kTitleBarBg);
    // Two overlapping squares -- the universal "restore window" glyph.
    dl->AddRect(ImVec2(cx - half + 2.0f, cy - half - 1.0f), ImVec2(cx + half + 2.0f, cy + half - 1.0f), col, 0.0f, 0, 1.1f);
    dl->AddRectFilled(ImVec2(cx - half - 2.0f, cy - half + 1.0f), ImVec2(cx + half - 2.0f, cy + half + 1.0f), bg);
    dl->AddRect(ImVec2(cx - half - 2.0f, cy - half + 1.0f), ImVec2(cx + half - 2.0f, cy + half + 1.0f), col, 0.0f, 0, 1.1f);
}

void DrawCloseIcon(ImDrawList* dl, ImVec2 p0, ImVec2 p1) {
    const float cx = (p0.x + p1.x) * 0.5f;
    const float cy = (p0.y + p1.y) * 0.5f;
    const float half = 4.5f;
    const ImU32 col = palette::U32FromHex(palette::kTextPrimary);
    dl->AddLine(ImVec2(cx - half, cy - half), ImVec2(cx + half, cy + half), col, 1.2f);
    dl->AddLine(ImVec2(cx - half, cy + half), ImVec2(cx + half, cy - half), col, 1.2f);
}

} // namespace

TitleBarResult DrawTitleBar(EditorState& editor_state, StudioSettings& settings, bool is_maximized,
                             float height, const std::vector<PluginInfo>& plugins,
                             const std::vector<RegisteredPanel>& panels, const std::vector<std::string>& closed_panels) {
    TitleBarResult result;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, height));
    ImGui::SetNextWindowViewport(viewport->ID);

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                    ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings |
                                    ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
                                    ImGuiWindowFlags_NoDocking;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, palette::FromHex(kTitleBarBg));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("##TitleBar", nullptr, flags);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();

    // --- Brand icon (VSCode-style: just the icon, no text label) ----------
    // Draws the real Ava Studio logo (avastudio.png, baked into the exe --
    // see src/branding/logo_texture.h) instead of a flat orange dot.
    // kIconSize is bigger than the old dot's 14px: a placeholder dot reads
    // fine at any size, but the logo has real detail that needs the extra
    // pixels to stay legible at title-bar scale. It's drawn 1:1 (the
    // logo's own square aspect ratio), not stretched to any other shape.
    constexpr float kIconLeftPad = 14.0f;
    constexpr float kIconSize = 20.0f;
    ImGui::SetCursorPos(ImVec2(kIconLeftPad, (height - kIconSize) * 0.5f));
    {
        const unsigned int logo_texture = branding::GetLogoTextureId();
        if (logo_texture != 0) {
            ImGui::Image(static_cast<ImTextureID>(logo_texture), ImVec2(kIconSize, kIconSize));
        } else {
            // Fallback in the unlikely event the embedded logo failed to
            // decode/upload -- keeps the titlebar from showing a hole.
            const ImVec2 icon_pos = ImGui::GetCursorScreenPos();
            const ImVec2 icon_center(icon_pos.x + kIconSize * 0.5f, icon_pos.y + kIconSize * 0.5f);
            ImGui::GetWindowDrawList()->AddCircleFilled(icon_center, kIconSize * 0.5f, palette::U32FromHex(palette::kPrimary));
        }
    }

    // --- File / Run menu ----------------------------------------------
    // Plain buttons that each open their own popup explicitly, instead of
    // ImGui::BeginMenu() -- BeginMenu() assumes it lives inside a real
    // ImGuiWindowFlags_MenuBar row, and a real menu bar would force its
    // own dedicated top strip, breaking this titlebar's single-row custom
    // layout (brand mark / centered file name / window buttons all share
    // this same row). Used loose like before, BeginMenu()'s "close on
    // click outside" logic doesn't reliably see clicks on this
    // NoBringToFrontOnFocus window, so it can get stuck open. Explicit
    // OpenPopup/BeginPopup doesn't have that problem.
    // Three equal-size pill buttons (File / Run / About). Always-visible
    // background at roughly the same visual weight in every state --
    // normal state used to be near-transparent while hover was fully
    // opaque, so whichever button had the mouse over it read as visibly
    // "bigger"/more prominent even though the pixel dimensions were
    // identical. Now normal/hover/active are all solid, just progressively
    // lighter, so the three buttons look like a consistent set at rest.
    const float kMenuBtnMinWidth = 44.0f;
    const ImVec2 menu_btn_size(kMenuBtnMinWidth, ImGui::GetFrameHeight());

    // Positioned explicitly from the icon's known geometry instead of
    // ImGui::SameLine() chained off the previous item -- SameLine() derives
    // its Y from whatever the last item's line height/baseline was, which
    // is why File used to end up a pixel or two off from Run/About whenever
    // the item before it (the old brand text) had different metrics. An
    // absolute SetCursorPos() can't inherit that kind of drift.
    ImGui::SetCursorPos(ImVec2(kIconLeftPad + kIconSize + 20.0f, (height - menu_btn_size.y) * 0.5f));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, palette::FromHex(palette::kSurface));
    ImGui::PushStyleColor(ImGuiCol_Button, palette::FromHex(palette::kCard));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, palette::FromHex(palette::kBorder));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, palette::FromHex(palette::kBorder));

    if (ImGui::Button("File", menu_btn_size)) {
        ImGui::OpenPopup("##FileMenu");
    }
    result.file_menu_rect = ItemScreenRect();
    // Anchor the dropdown directly under the button that opened it
    // (VSCode-style) instead of ImGui's default of "wherever the mouse
    // happened to click" -- otherwise a click near a button's edge can
    // make the popup appear noticeably off to the side.
    ImGui::SetNextWindowPos(ImVec2(result.file_menu_rect.min_x, result.file_menu_rect.max_y + 2.0f));
    // Constraint en vez de SetNextWindowSize: 210px es el ancho de
    // siempre, pero este popup es el único que tiene un BeginMenu()
    // adentro (Preferences) -- ImGui necesita poder ensanchar la
    // ventana si su propio cálculo de columnas (el que ubica la flecha
    // de submenu a la derecha, mismo mecanismo que alinea "Ctrl+N" etc.
    // en los demás items) pide más lugar. Un ancho fijo se lo impediría
    // y la flecha terminaría mal posicionada -- dejarlo crecer es lo
    // que hace que la flecha nativa de BeginMenu se vea bien sin tener
    // que dibujar nada a mano.
    ImGui::SetNextWindowSizeConstraints(ImVec2(210.0f, 0.0f), ImVec2(FLT_MAX, FLT_MAX));
    static bool open_plugins_modal = false;
    if (ImGui::BeginPopup("##FileMenu")) {
        if (ImGui::MenuItem("New File", "Ctrl+N")) {
            result.new_requested = true;
        }
        if (ImGui::MenuItem("Open...", "Ctrl+O")) {
            result.open_requested = true;
        }
        if (ImGui::MenuItem("Open Folder...")) {
            result.open_folder_requested = true;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Save", "Ctrl+S")) {
            editor_state.save_requested = true;
        }
        if (ImGui::MenuItem("Close Tab", "Ctrl+W")) {
            editor_state.close_tab_requested = true;
        }
        if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) {
            result.save_as_requested = true;
        }
        ImGui::Separator();
        // Only meaningful for a .avaui tab (see ToggleTabViewMode's own
        // no-op guard) -- shown unconditionally, same as Save/Close Tab
        // above staying enabled with no tab open, rather than
        // special-casing this one item to disable/hide.
        {
            const EditorTab* active = editor_state.Active();
            const bool showing_design = active && active->is_avaui && active->view_mode == TabViewMode::Design;
            const char* label = showing_design ? "Ver Código" : "Ver Diseño";
            if (ImGui::MenuItem(label, "F7")) {
                if (EditorTab* mutable_active = editor_state.Active()) {
                    ToggleTabViewMode(*mutable_active);
                }
            }
        }
        ImGui::Separator();
        if (ImGui::BeginMenu("Preferences")) {
            if (ImGui::MenuItem("Settings", "Ctrl+,")) {
                result.open_settings_requested = true;
            }
            if (ImGui::MenuItem("Extensions")) {
                open_plugins_modal = true;
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Exit", "Alt+F4")) {
            result.quit_requested = true;
        }
        ImGui::EndPopup();
    }

    // --- View menu -------------------------------------------------------
    // Same VSCode idea as the reference screenshot: one checkbox per
    // panel that can be shown/hidden, checked when it's currently
    // visible. Built-ins first (see panels/builtin_panels.h), then a
    // separator and one entry per plugin panel -- both sections just
    // read/write the same `closed_panels` list, so a single click
    // handler (result.panel_toggle_requested) covers either kind; see
    // its comment in the header for why.
    ImGui::SameLine(0.0f, 4.0f);
    if (ImGui::Button("View", menu_btn_size)) {
        ImGui::OpenPopup("##ViewMenu");
    }
    result.view_menu_rect = ItemScreenRect();
    ImGui::SetNextWindowPos(ImVec2(result.view_menu_rect.min_x, result.view_menu_rect.max_y + 2.0f));
    ImGui::SetNextWindowSize(ImVec2(200.0f, 0.0f));
    if (ImGui::BeginPopup("##ViewMenu")) {
        for (const std::string_view& name : kBuiltinPanelNames) {
            const std::string name_str(name);
            const bool visible =
                std::find(closed_panels.begin(), closed_panels.end(), name_str) == closed_panels.end();
            if (ImGui::MenuItem(name_str.c_str(), nullptr, visible)) {
                result.panel_toggle_requested = name_str;
            }
        }
        if (!panels.empty()) {
            ImGui::Separator();
            for (const RegisteredPanel& panel : panels) {
                const bool visible =
                    std::find(closed_panels.begin(), closed_panels.end(), panel.name) == closed_panels.end();
                if (ImGui::MenuItem(panel.name.c_str(), nullptr, visible)) {
                    result.panel_toggle_requested = panel.name;
                }
            }
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine(0.0f, 4.0f);
    if (ImGui::Button("Run", menu_btn_size)) {
        ImGui::OpenPopup("##RunMenu");
    }
    result.run_menu_rect = ItemScreenRect();
    ImGui::SetNextWindowPos(ImVec2(result.run_menu_rect.min_x, result.run_menu_rect.max_y + 2.0f));
    ImGui::SetNextWindowSize(ImVec2(160.0f, 0.0f));
    if (ImGui::BeginPopup("##RunMenu")) {
        if (ImGui::MenuItem("Run Script", "F5")) {
            editor_state.run_requested = true;
        }
        ImGui::EndPopup();
    }

    // About: its own top-level button next to File/Run (not buried
    // inside a dropdown) -- opens the modal directly on click, same size
    // as its siblings.
    ImGui::SameLine(0.0f, 4.0f);
    static bool open_about_modal = false;
    if (ImGui::Button("About", menu_btn_size)) {
        open_about_modal = true;
    }
    result.about_rect = ItemScreenRect();

    ImGui::PopStyleColor(4);

    // Checked after both BeginPopup/EndPopup calls above so it reflects
    // this frame's actual state (a popup that just got closed by
    // MenuItem()/Escape/etc. this same frame should not keep forcing the
    // wider hit region on the next frame).
    result.any_popup_open =
        ImGui::IsPopupOpen("##FileMenu") || ImGui::IsPopupOpen("##ViewMenu") || ImGui::IsPopupOpen("##RunMenu");

    if (open_about_modal) {
        ImGui::OpenPopup("About Ava Studio##AboutModal");
        open_about_modal = false;
    }
    // Re-centered every frame against the *current* viewport size, not just
    // when the popup opens -- using GetCenter() (as opposed to a
    // one-time/cached position) is what keeps it centered whether the
    // window is maximized, restored, or resized while the popup is open.
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(320.0f, 0.0f));
    if (ImGui::BeginPopupModal("About Ava Studio##AboutModal", nullptr,
                               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)) {
        const float window_w = ImGui::GetWindowWidth();

        // Every line in this modal is centered under the logo, VSCode/
        // JetBrains "About" style -- CalcTextSize() + SetCursorPosX() since
        // ImGui has no built-in centered-text widget. Window padding
        // cancels out of the math (see the Close button below, which relied
        // on the same fact before this redesign), so it's just
        // (window width - item width) / 2.
        auto CenteredText = [&](const ImVec4& color, const char* text) {
            const ImVec2 size = ImGui::CalcTextSize(text);
            ImGui::SetCursorPosX((window_w - size.x) * 0.5f);
            ImGui::TextColored(color, "%s", text);
        };

        ImGui::Dummy(ImVec2(0.0f, 4.0f));

        // Real Ava Studio logo (see src/branding/logo_texture.h) instead of
        // no brand mark at all -- an About dialog with no logo was the
        // actual gap here, not just a text-formatting issue.
        constexpr float kLogoSize = 56.0f;
        if (const unsigned int logo_texture = branding::GetLogoTextureId(); logo_texture != 0) {
            ImGui::SetCursorPosX((window_w - kLogoSize) * 0.5f);
            ImGui::Image(static_cast<ImTextureID>(logo_texture), ImVec2(kLogoSize, kLogoSize));
        }

        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        CenteredText(palette::FromHex(palette::kTextPrimary), "Ava Studio");
        CenteredText(palette::FromHex(palette::kTextMuted), "AvaLang editor & runtime");
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 6.0f));

        CenteredText(palette::FromHex(palette::kTextPrimary), "Version 0.1.0");
        CenteredText(palette::FromHex(palette::kTextMuted), "Built on the AvaLang core VM");
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 6.0f));

        CenteredText(palette::FromHex(palette::kTextMuted), "Created by AvalonTM");
        {
            // Drawn as plain colored text (not a Button) so it reads as a
            // hyperlink rather than a UI control, with a manual underline
            // and hand cursor on hover for the usual "this is clickable"
            // affordance, same idea as a browser's status-bar link hint.
            const char* kGithubUrl = "https://github.com/avalontm";
            const ImVec2 link_size = ImGui::CalcTextSize(kGithubUrl);
            ImGui::SetCursorPosX((window_w - link_size.x) * 0.5f);
            const ImVec2 link_pos = ImGui::GetCursorScreenPos();
            ImGui::TextColored(palette::FromHex(palette::kPrimary), "%s", kGithubUrl);
            const bool hovered = ImGui::IsItemHovered();
            if (hovered) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                ImGui::GetWindowDrawList()->AddLine(
                    ImVec2(link_pos.x, link_pos.y + link_size.y),
                    ImVec2(link_pos.x + link_size.x, link_pos.y + link_size.y),
                    palette::U32FromHex(palette::kPrimary));
            }
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                titlebar::OpenUrl(kGithubUrl);
            }
        }
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 8.0f));

        const float close_w = 90.0f;
        ImGui::SetCursorPosX((window_w - close_w) * 0.5f);
        if (ImGui::Button("Close", ImVec2(close_w, 0.0f))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::Dummy(ImVec2(0.0f, 2.0f));
        ImGui::EndPopup();
    }

    // --- Plugins modal -----------------------------------------------------
    // A real window, not a cramped dropdown -- "File > Plugins..." opens
    // this instead of a popup under a button, same family as the
    // Properties modal above. Lists every .dll/.so PluginHost found in
    // plugins/ (see the `plugins` param) with a checkbox each; toggling
    // one sets result.plugin_toggle_requested for main.cpp to act on
    // (flip StudioSettings::disabled_plugins, SaveSettings,
    // PluginHost::Reload -- takes effect immediately, no restart) before
    // the next PluginHost::ScanAvailable() snapshot comes back around.
    if (open_plugins_modal) {
        ImGui::OpenPopup("Plugins##PluginsModal");
        open_plugins_modal = false;
    }
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(480.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0f, 20.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 10.0f));
    if (ImGui::BeginPopupModal("Plugins##PluginsModal", nullptr,
                               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)) {
        ImGui::TextColored(palette::FromHex(palette::kTextMuted), "Installed plugins");
        ImGui::Dummy(ImVec2(0.0f, 2.0f));
        ImGui::TextWrapped(
            "Enable or disable a plugin found in the plugins/ folder. Changes apply "
            "immediately -- no need to restart Ava Studio.");
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 6.0f));

        if (plugins.empty()) {
            ImGui::TextDisabled("No plugins found in plugins/.");
        } else {
            // Scrollable list -- caps the modal's height instead of
            // growing unbounded as more plugins get dropped in.
            ImGui::BeginChild("##PluginsList", ImVec2(0.0f, 220.0f), true);
            for (const PluginInfo& plugin : plugins) {
                bool enabled = plugin.enabled;
                ImGui::PushID(plugin.file_name.c_str());
                if (ImGui::Checkbox(plugin.file_name.c_str(), &enabled)) {
                    // `enabled` already reflects the NEW state ImGui
                    // just applied to the widget -- main.cpp only needs
                    // to know *which* plugin changed, it re-derives
                    // on/off from settings.disabled_plugins itself.
                    result.plugin_toggle_requested = plugin.file_name;
                }
                if (plugin.enabled && !plugin.loaded) {
                    // Enabled but not currently loaded: either
                    // LoadAll() rejected it (bad ABI, init failed --
                    // see the Output panel's log) or a Reload() triggered
                    // this same frame hasn't run yet.
                    ImGui::SameLine();
                    ImGui::TextDisabled("(no cargado)");
                }

                // Fase 9: display name / version / author, one muted
                // line under the checkbox -- indented to visually
                // belong to that plugin, not the next one in the list.
                // Any piece the plugin didn't export is just left out
                // rather than shown as "unknown"/empty parens.
                std::string meta_line;
                if (!plugin.plugin_name.empty()) meta_line += plugin.plugin_name;
                if (!plugin.version.empty()) meta_line += (meta_line.empty() ? "v" : " v") + plugin.version;
                if (!plugin.author.empty()) meta_line += (meta_line.empty() ? "" : "  ·  ") + std::string("by ") + plugin.author;
                if (!meta_line.empty()) {
                    ImGui::Indent();
                    ImGui::TextColored(palette::FromHex(palette::kTextMuted), "%s", meta_line.c_str());
                    ImGui::Unindent();
                }

                ImGui::PopID();
            }
            ImGui::EndChild();
        }

        // Per-panel show/hide (previously a second list in this same
        // modal) now lives in the "View" menu instead -- see its
        // dropdown right next to File, which covers both built-in and
        // plugin panels in one place instead of splitting them across
        // two different menus.

        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        const float close_button_w = 90.0f;
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - close_button_w - ImGui::GetStyle().WindowPadding.x);
        if (ImGui::Button("Close", ImVec2(close_button_w, 0.0f))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::Dummy(ImVec2(0.0f, 2.0f));
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(2);

    // --- Centered file name, like VSCode's window title -------------------
    {
        const EditorTab* active = editor_state.Active();
        const char* label = (!active || active->file_path.empty()) ? "Ava Studio" : active->file_path.c_str();
        const ImVec2 text_size = ImGui::CalcTextSize(label);
        ImGui::SetCursorPos(ImVec2((viewport->WorkSize.x - text_size.x) * 0.5f, (height - text_size.y) * 0.5f));
        ImGui::TextColored(palette::FromHex(palette::kTextMuted), "%s", label);
    }

    // --- Window controls (minimize / maximize / close) ---------------------
    ImGui::SetCursorPos(ImVec2(viewport->WorkSize.x - kButtonWidth * 3.0f, 0.0f));

    const ImU32 neutral_hover = palette::U32FromHex(palette::kCard);
    const ImU32 close_hover = IM_COL32(0xE8, 0x11, 0x23, 0xFF);

    if (CaptionButton("min", kButtonWidth, height, neutral_hover, DrawMinimizeIcon)) {
        result.minimize_clicked = true;
    }
    result.minimize_rect = ItemScreenRect();

    ImGui::SameLine(0.0f, 0.0f);
    if (is_maximized) {
        if (CaptionButton("max", kButtonWidth, height, neutral_hover, DrawRestoreIcon)) {
            result.maximize_or_restore_clicked = true;
        }
    } else {
        if (CaptionButton("max", kButtonWidth, height, neutral_hover, DrawMaximizeIcon)) {
            result.maximize_or_restore_clicked = true;
        }
    }
    result.maximize_rect = ItemScreenRect();

    ImGui::SameLine(0.0f, 0.0f);
    if (CaptionButton("close", kButtonWidth, height, close_hover, DrawCloseIcon)) {
        result.close_clicked = true;
    }
    result.close_rect = ItemScreenRect();

    ImGui::End();
    return result;
}

} // namespace studio
