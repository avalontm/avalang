#pragma once

#include "plugins/plugin_host.h"

namespace studio {

// Fase 5 (see PLAN_agente_ia_openrouter.md): the approval gate for
// AvaHostServices::apply_edit. Every proposal a plugin has queued (see
// PluginHost::PendingEdits) shows up here as a collapsible diff against
// the file's current contents, with Aplicar/Rechazar buttons -- nothing
// a plugin proposes ever reaches disk without a click here.
//
// Draws nothing (not even an empty window) when there are no pending
// edits, so it doesn't take up dock space or attention the rest of the
// time. Call once per frame, anywhere after PluginHost's plugins have
// had a chance to run this frame's tool calls.
void DrawPendingEditsPanel(PluginHost& plugin_host);

} // namespace studio
