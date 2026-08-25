#pragma once

namespace studio {

struct ActivityBarResult {
    bool explorer_clicked = false;
    bool search_clicked = false;
    bool toolbox_clicked = false;
    bool extensions_clicked = false;
    bool settings_clicked = false;
};

// Fase 7: fixed vertical icon strip at the left edge of the workbench, same
// role as VS Code's Activity Bar. It switches/focuses the panels that share
// a dock slot as tabs today (Explorer+Toolbox on the left, Settings on the
// right alongside Properties) and gives Search (Find in Project) and
// Extensions a permanent, always-visible entry point -- replacing the
// per-panel MenuItem list that used to live in the View menu (see
// titlebar_panel.cpp's kBuiltinPanelNames loop, removed in this same phase)
// so that "toggle a panel" has one obvious home for the common panels
// instead of being spread across a text menu. Command Palette (Fase 3)
// still lists every panel, including the ones not represented here
// (Terminal/Logs/Problems/Build/plugin panels) -- this bar is deliberately
// not a second copy of that full list, only the handful of views a VS Code
// user would expect as permanent icons.
//
// Draws its own top-level ImGui window (NoDecoration, fixed pos/size, not
// part of the dockspace) -- same idiom DrawTitleBar already uses for its
// caption-button strip. `*_open` flags only drive the highlight/active bar
// on each icon; they don't gate whether the icon is clickable.
ActivityBarResult DrawActivityBar(float pos_x, float pos_y, float width, float height, bool explorer_open,
                                   bool search_open, bool toolbox_open, bool settings_open);

}
