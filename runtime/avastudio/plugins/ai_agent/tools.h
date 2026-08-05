#pragma once

#include "openrouter_client.h"
#include "plugin_api.h"

#include <string>
#include <vector>

// Fase 4 -- read-only tool calling. Four tools, all resolved locally by
// the plugin: three against the same read-only host services (Fase 0)
// and filesystem walk (context_builder) that automatic context (Fase 3)
// already uses, and one -- avalang_syntax_lookup -- against
// avalang_reference.h/.cpp's CSV-backed tables instead (added
// afterwards: the model was guessing AvaLang syntax by analogy with
// Python/JS because nothing in its context ever said otherwise -- see
// avalang_reference.h's comment). Nothing new is added to the host for
// either.
//
// No writing: these tools can only look, never touch a file, run
// anything, or otherwise affect the project. That's the whole point of
// doing Fase 4 before Fase 5 (approval-gated writes).

// The fixed set of tools this plugin currently advertises to the model.
std::vector<OpenRouterToolDef> BuildReadOnlyToolDefs();

// Runs a tool the model asked for by name, with its raw JSON arguments
// string (as accumulated from the streaming response). Always returns a
// string (JSON on success, `{"error": "..."}` on failure) -- never
// throws -- so the caller can hand it straight back to the model as the
// content of a "tool" role message.
//
// avalang_syntax_lookup is the odd one out here: unlike the other three
// it never touches `host` at all (its data comes from
// avalang_reference.h's CSV tables, not from project files or the
// editor), but it's still read-only in every sense that matters, so it
// lives in this same read-only bucket rather than getting a category of
// its own.
std::string ExecuteReadOnlyTool(AvaStudioHost* host, const std::string& tool_name, const std::string& arguments_json);

// Fase 5 -- write tool calling. Two tools, both resolved against the
// write services the host added to AvaHostServices for this phase (see
// plugin_api.h's "Write services (Fase 5)" section):
//
//  - apply_edit: never writes anything itself. Just forwards to
//    AvaHostServices::apply_edit, which queues the proposal for the
//    person to review (PendingEditsPanel's Aplicar/Rechazar) and
//    returns immediately -- "el agente nunca escribe ni ejecuta nada
//    sin confirmacion explicita del usuario" (see the plan's principio
//    rector) holds here because the tool result the model gets back
//    only ever says the proposal was queued, never that it was applied.
//  - run_project: forwards to AvaHostServices::run_project, which
//    really does run the active tab (same pipeline as F5) and blocks
//    until it's done -- no approval gate, by the same reasoning as the
//    host service's own doc comment: running code the person can
//    already see and Run themselves isn't a new capability the way
//    writing a file is.
std::vector<OpenRouterToolDef> BuildWriteToolDefs();
std::string ExecuteWriteTool(AvaStudioHost* host, const std::string& tool_name, const std::string& arguments_json);

// Fase 6 -- Designer integration. Two tools, both resolved against
// AvaHostServices::design_add_component/design_edit_component (see
// plugin_api.h's "Design services (Fase 6)" section): agrega/edita un
// componente sobre el ComponentTree del tab .avaui activo. Same
// approval gate as apply_edit above -- neither of these two host
// services writes anything by itself; both compute the resulting
// AvaLang UI source and queue it as a normal Aplicar/Rechazar
// proposal, so the tool result the model gets back only ever says the
// change was proposed, never that it was applied (same reasoning as
// ToolApplyEdit's result message in tools.cpp).
std::vector<OpenRouterToolDef> BuildDesignToolDefs();
std::string ExecuteDesignTool(AvaStudioHost* host, const std::string& tool_name, const std::string& arguments_json);
