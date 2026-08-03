#pragma once

// Host-side implementation of AvaUiApi (see plugin_api.h) -- the only
// file in Ava Studio that both includes imgui.h AND plugin_api.h.
// PluginHost wires an AvaStudioHost's `ui` field to these functions'
// addresses (see plugin_host.cpp); main.cpp calls FillUiApi once at
// startup and BeginPanelContext/EndPanelContext around each registered
// panel's draw() call every frame.
//
// AvaPanelContext itself carries nothing plugins can act on beyond
// passing the pointer back into these calls -- host-side it's just the
// currently-drawing panel's name, used to PushID/PopID so two
// different plugins' panels can each use e.g. "##input" as a widget id
// without colliding.

#include "plugin_api.h"

namespace studio {
namespace plugins_ui {

// Fills every function pointer in `ui` with this file's
// implementations. Called once by PluginHost's constructor.
void FillUiApi(AvaUiApi& ui);

// main.cpp brackets a registered panel's draw() call with these two --
// see plugin_host.h's RegisteredPanel and the main.cpp loop that walks
// PluginHost::Panels(). Must be called between ImGui::Begin(name) and
// ImGui::End() for that same panel.
AvaPanelContext* BeginPanelContext(const char* panel_name);
void EndPanelContext(AvaPanelContext* ctx);

} // namespace plugins_ui
} // namespace studio
