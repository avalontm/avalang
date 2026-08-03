#include "plugin_api.h"
#include "openrouter_client.h"
#include "avalang_reference.h"
#include "config.h"
#include "context_builder.h"
#include "memory.h"
#include "tools.h"

#include <curl/curl.h>

#include <atomic>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

const char* kDefaultModel = "openrouter/auto";
const size_t kContextCharBudget = 12000;
// El agente estaba adivinando la sintaxis de AvaLang por analogia con
// Python/JS porque nada en su contexto le decia otra cosa (ver
// avalang_reference.h) -- este presupuesto es para el cheat sheet fijo
// de palabras clave + builtins que se manda como mensaje "system" en
// TODOS los turnos (a diferencia del arbol de archivos/archivo activo
// de BuildContextMessage, que solo se manda con auto_context prendido:
// esto no depende del proyecto abierto, es la gramatica del lenguaje).
const size_t kAvalangReferenceCharBudget = 6000;
// Fase 4 safety valve: a model that keeps calling tools forever (buggy
// loop, or just very thorough) still gets cut off instead of hammering
// OpenRouter indefinitely for one user message.
const int kMaxToolIterations = 8;

// What's shown in the chat panel. `kind` controls how DrawAgentPanel
// renders the line -- user/assistant turns look like before Fase 4;
// ToolNotice is the new one, a dim one-liner like
// "read_file(\"src/main.ava\") -> ok (1204 bytes)" so the person can see
// what the agent looked at without the raw tool JSON cluttering the chat.
enum class MessageKind { User, Assistant, ToolNotice };

struct ChatMessage {
    MessageKind kind;
    std::string content;
};

struct AgentState {
    std::mutex mutex;
    std::vector<ChatMessage> messages;
    char input_buffer[4096] = "";
    char api_key_buffer[256] = "";
    std::atomic<bool> streaming{false};
    std::atomic<bool> stop_requested{false};
    std::thread worker;

    // Bumped every time SendMessage clears input_buffer (Enter or
    // "Enviar", both go through SendMessage). Folded into the input
    // widget's id in DrawAgentPanel below so that clearing the buffer
    // always shows up on screen: a Dear ImGui InputText keeps its own
    // internal edit copy of the text while it's the active widget, and
    // only resyncs it from the external buffer when the widget is
    // (re)created under a new id -- an id that never changes can end up
    // still showing its last-typed text even after this struct's buffer
    // has been cleared out from under it, depending on exactly when/how
    // the send happened (Enter vs. clicking "Enviar" vs. focus state at
    // that moment). Changing the id on every send sidesteps that
    // entirely instead of depending on a specific reactivation trick.
    int input_reset_counter = 0;

    // Loading indicator: short human-readable string describing what the
    // worker thread is doing right now ("Pensando...", "Ejecutando
    // read_file..."), read every frame by DrawAgentPanel to render a
    // spinner so the panel never looks frozen while waiting on the
    // network or on a tool. Empty when nothing is in flight.
    std::mutex status_mutex;
    std::string status_text;
    std::string api_key;
    bool has_api_key = false;
    std::string current_model = kDefaultModel;
    bool auto_context = true;

    // Fase 7.1: memoria persistente entre sesiones, un archivo por
    // proyecto (ver memory.h/.cpp). `current_project_root` es lo último
    // que vio DrawAgentPanel -- cuando cambia (se abrió otro proyecto),
    // se recarga el historial correspondiente desde disco.
    std::string current_project_root;
    bool project_root_known = false;

    // Fase 7.2: costo acumulado del proyecto activo, en USD. Se mueve
    // junto con el resto de la memoria (mismo archivo, ver memory.h).
    double accumulated_cost_usd = 0.0;

    std::mutex models_mutex;
    std::vector<OpenRouterModel> models;
    std::vector<std::string> model_labels;
    int selected_model_index = -1;
    std::atomic<bool> loading_models{false};
    bool models_loaded = false;
    std::string models_error;
    std::thread models_worker;
};

AgentState g_state;
AvaStudioHost* g_host = nullptr;

std::string FormatModelLabel(const OpenRouterModel& model) {
    char buffer[320];
    std::snprintf(buffer, sizeof(buffer), "%s (%ldk ctx)", model.id.c_str(), model.context_length / 1000);
    return buffer;
}

// --- Loading indicators --------------------------------------------------
// The UI toolkit exposed to plugins (see plugin_api.h's AvaUiApi) has no
// built-in spinner/progress widget, and DrawAgentPanel is called once per
// frame regardless of whether anything changed, so a wall-clock-driven
// animation here is enough to look alive without any host support: pick
// the frame based on elapsed milliseconds, no timer/callback needed.

int64_t NowMillis() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// Small braille spinner, same idea as the ones CLIs use for "working..."
// -- 10 frames, ~80ms each, so it visibly moves every frame without being
// distracting.
const char* Spinner() {
    static const char* kFrames[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
    int idx = static_cast<int>((NowMillis() / 80) % 10);
    return kFrames[idx];
}

// "...", "..", ".", "" cycling every 400ms, appended after a status word
// like "Pensando" so it doesn't just sit there as static text.
std::string AnimatedDots() {
    int step = static_cast<int>((NowMillis() / 400) % 4);
    return std::string(step, '.');
}

// Blinking "▌" caret appended to the assistant bubble that's currently
// receiving streamed tokens, same idea as a text-cursor blink -- toggles
// every 500ms.
const char* TypingCursor() {
    return ((NowMillis() / 500) % 2 == 0) ? " ▌" : "  ";
}

void SetStatus(AgentState* state, std::string text) {
    std::lock_guard<std::mutex> lock(state->status_mutex);
    state->status_text = std::move(text);
}

// Composed line drawn under the chat history while streaming: spinner +
// current status word + animated dots, e.g. "⠹ Pensando..." or
// "⠼ Ejecutando read_file, list_files...". Returns empty when there's
// nothing to report (worker just hasn't set a status yet, or already
// cleared it).
std::string BuildStatusLine(AgentState* state) {
    std::string status;
    {
        std::lock_guard<std::mutex> lock(state->status_mutex);
        status = state->status_text;
    }
    if (status.empty()) return "";
    return std::string(Spinner()) + "  " + status + AnimatedDots();
}

void PersistConfig(AgentState* state) {
    AgentConfig config;
    config.api_key = state->api_key;
    config.last_model = state->current_model;
    SaveAgentConfig(config);
}

// Fase 7.1: reconstruye AgentMemory a partir de lo que hoy se muestra en
// el panel -- solo turnos User/Assistant, igual que SendMessage arma
// `history` para mandar a OpenRouter (ToolNotice no se persiste, se
// reconstruiría llamando a las tools de nuevo si hiciera falta).
void PersistMemory(AgentState* state) {
    if (!state->project_root_known) return;
    AgentMemory memory;
    memory.accumulated_cost_usd = state->accumulated_cost_usd;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        for (const auto& m : state->messages) {
            if (m.kind == MessageKind::User) memory.history.push_back({"user", m.content});
            else if (m.kind == MessageKind::Assistant) memory.history.push_back({"assistant", m.content});
        }
    }
    SaveAgentMemory(state->current_project_root, memory);
}

// Llamado cada frame desde DrawAgentPanel: si el proyecto activo cambió
// desde el último frame, descarta el chat en pantalla y carga el
// historial guardado para el proyecto nuevo (vacío la primera vez que
// se abre un proyecto).
void SyncProjectMemory(AgentState* state) {
    if (!g_host) return;
    const char* root = g_host->services.get_project_root(g_host);
    std::string root_str = root ? root : "";
    if (state->project_root_known && root_str == state->current_project_root) return;

    state->current_project_root = root_str;
    state->project_root_known = true;

    AgentMemory memory = LoadAgentMemory(root_str);
    state->accumulated_cost_usd = memory.accumulated_cost_usd;
    std::lock_guard<std::mutex> lock(state->mutex);
    state->messages.clear();
    for (const auto& entry : memory.history) {
        if (entry.role == "user") state->messages.push_back({MessageKind::User, entry.content});
        else if (entry.role == "assistant") state->messages.push_back({MessageKind::Assistant, entry.content});
    }
}

void FetchModels(AgentState* state) {
    if (state->loading_models.load()) return;
    state->loading_models.store(true);
    if (state->models_worker.joinable()) state->models_worker.join();

    state->models_worker = std::thread([state]() {
        OpenRouterClient client(state->api_key);
        client.ListModels([state](bool success, std::vector<OpenRouterModel> models, const std::string& error) {
            std::lock_guard<std::mutex> lock(state->models_mutex);
            if (success) {
                state->models = std::move(models);
                state->model_labels.clear();
                state->selected_model_index = -1;
                for (size_t i = 0; i < state->models.size(); ++i) {
                    state->model_labels.push_back(FormatModelLabel(state->models[i]));
                    if (state->models[i].id == state->current_model) {
                        state->selected_model_index = static_cast<int>(i);
                    }
                }
                state->models_loaded = true;
                state->models_error.clear();
            } else {
                state->models_error = error;
            }
            state->loading_models.store(false);
        });
    });
}

// Fase 5: the model now sees both the Fase 4 read-only tools and the
// Fase 5 write tools in the same `tools` list (see SendMessage below),
// but they're still implemented by two separate functions in tools.cpp
// (ExecuteReadOnlyTool/ExecuteWriteTool) -- read-only vs write is
// exactly the boundary that matters for auditing what this plugin can
// touch, so tools.cpp keeps that split rather than merging into one
// function. This is just the one place that has to know both exist and
// route a call by name to whichever one defines it.
// Fase 6: the two design tools (design_add_component/
// design_edit_component) count as "write" tools for this dispatch --
// they only ever queue a proposal (same approval gate as apply_edit,
// see plugin_api.h's "Design services (Fase 6)" section), never write
// anything by themselves, but a plugin thread routing them straight to
// ExecuteReadOnlyTool would be wrong regardless -- IsWriteToolName is
// about "does tools.cpp define this in the write half or the
// read-only half", not about whether it's approval-gated.
bool IsWriteToolName(const std::string& name) {
    return name == "apply_edit" || name == "run_project" || name == "design_add_component" ||
           name == "design_edit_component";
}

std::string ExecuteAnyTool(AvaStudioHost* host, const std::string& name, const std::string& arguments_json) {
    if (name == "design_add_component" || name == "design_edit_component") {
        return ExecuteDesignTool(host, name, arguments_json);
    }
    if (IsWriteToolName(name)) return ExecuteWriteTool(host, name, arguments_json);
    return ExecuteReadOnlyTool(host, name, arguments_json);
}

// One line summarizing a tool call + its result for the ToolNotice
// display message -- deliberately short (not the full JSON result,
// which is what actually goes to the model) so the chat stays readable
// even if the agent reads several files in a row.
std::string SummarizeToolCall(const std::string& name, const std::string& arguments_json,
                               const std::string& result_json) {
    std::string args_preview = arguments_json;
    if (args_preview.size() > 80) args_preview = args_preview.substr(0, 80) + "...";
    std::string status = (result_json.find("\"error\"") != std::string::npos) ? "error" : "ok";
    char buf[256];
    std::snprintf(buf, sizeof(buf), "%s(%s) -> %s (%zu bytes)", name.c_str(), args_preview.c_str(), status.c_str(),
                  result_json.size());
    return buf;
}

// Fase 7.2: `prompt_price`/`completion_price` en OpenRouterModel vienen
// como string (precio por token, ej. "0.000001") -- se combinan acá con
// el `usage` de un turno. Cualquier price vacío/no numérico cuenta como
// 0 en vez de tirar excepción, para no perder el turno de streaming por
// un campo de pricing raro.
double ComputeTurnCost(AgentState* state, const std::string& model_id, const OpenRouterUsage& usage) {
    std::lock_guard<std::mutex> lock(state->models_mutex);
    for (const auto& m : state->models) {
        if (m.id != model_id) continue;
        double prompt_price = 0.0;
        double completion_price = 0.0;
        try {
            if (!m.prompt_price.empty()) prompt_price = std::stod(m.prompt_price);
        } catch (...) {
        }
        try {
            if (!m.completion_price.empty()) completion_price = std::stod(m.completion_price);
        } catch (...) {
        }
        return usage.prompt_tokens * prompt_price + usage.completion_tokens * completion_price;
    }
    return 0.0;
}

void SendMessage(AgentState* state) {
    std::string text(state->input_buffer);
    if (text.empty()) return;
    state->input_buffer[0] = '\0';
    state->input_reset_counter++;

    // `history` is what actually gets sent to OpenRouter -- separate from
    // the display-only `state->messages` vector because it needs the
    // full OpenAI tool-calling shape (assistant tool_calls turns, tool
    // role results) that the chat UI has no reason to show verbatim.
    std::vector<OpenRouterMessage> history;
    // Siempre presente, independiente de auto_context: sin esto el
    // modelo no tiene ninguna senal de que AvaLang usa 'end'/'func'/
    // 'then' en vez de indentacion/'def'/':' como Python o JS (ver
    // avalang_reference.h). auto_context es sobre el proyecto abierto
    // (arbol de archivos, archivo activo); esto es sobre el lenguaje en
    // si, asi que no comparte ese toggle.
    std::string avalang_reference = BuildAvalangReferenceMessage(kAvalangReferenceCharBudget);
    if (!avalang_reference.empty()) history.push_back({"system", avalang_reference, "", {}});

    if (state->auto_context) {
        std::string context = BuildContextMessage(g_host, kContextCharBudget);
        if (!context.empty()) history.push_back({"system", context, "", {}});
    }
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        for (const auto& m : state->messages) {
            if (m.kind == MessageKind::User) history.push_back({"user", m.content, "", {}});
            else if (m.kind == MessageKind::Assistant) history.push_back({"assistant", m.content, "", {}});
            // ToolNotice entries from previous turns are not replayed --
            // the model already used their results to produce the
            // assistant turn that followed them.
        }
        state->messages.push_back({MessageKind::User, text});
        history.push_back({"user", text, "", {}});
    }

    if (state->worker.joinable()) state->worker.join();
    state->stop_requested.store(false);
    state->streaming.store(true);

    std::string model = state->current_model;
    std::vector<OpenRouterToolDef> tools = BuildReadOnlyToolDefs();
    std::vector<OpenRouterToolDef> write_tools = BuildWriteToolDefs();
    std::vector<OpenRouterToolDef> design_tools = BuildDesignToolDefs();
    tools.insert(tools.end(), write_tools.begin(), write_tools.end());
    tools.insert(tools.end(), design_tools.begin(), design_tools.end());

    state->worker = std::thread([state, history, model, tools]() mutable {
        for (int iteration = 0; iteration < kMaxToolIterations; ++iteration) {
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                state->messages.push_back({MessageKind::Assistant, ""});
            }
            // Nothing has come back yet for this turn -- show "thinking"
            // until either the first token streams in (DrawAgentPanel
            // then switches to the blinking-cursor bubble instead, see
            // below) or tool calls show up (status gets overwritten
            // further down).
            SetStatus(state, "Pensando");

            OpenRouterClient client(state->api_key);

            bool success = false;
            std::string error;
            std::vector<OpenRouterToolCall> tool_calls;
            OpenRouterUsage usage;

            client.StreamChatCompletion(
                model, history, tools,
                [state](const std::string& delta) {
                    std::lock_guard<std::mutex> lock(state->mutex);
                    if (!state->messages.empty()) state->messages.back().content += delta;
                },
                [&](bool ok, const std::string& err, std::vector<OpenRouterToolCall> calls, OpenRouterUsage u) {
                    success = ok;
                    error = err;
                    tool_calls = std::move(calls);
                    usage = u;
                },
                &state->stop_requested);

            if (success) state->accumulated_cost_usd += ComputeTurnCost(state, model, usage);

            if (!success) {
                SetStatus(state, "");
                std::lock_guard<std::mutex> lock(state->mutex);
                if (!state->messages.empty()) {
                    if (state->messages.back().content.empty()) {
                        state->messages.back().content = "[error] " + error;
                    } else {
                        state->messages.back().content += "\n[error] " + error;
                    }
                }
                break;
            }

            if (tool_calls.empty() || state->stop_requested.load()) {
                // Normal end of turn: the model answered with plain text
                // (or the user hit Detener) -- nothing left to resolve.
                // Fase 7.1: se persiste acá, no en cada delta de
                // streaming, para no pegarle a disco por cada fragmento.
                SetStatus(state, "");
                PersistMemory(state);
                break;
            }

            // The model wants to call tools: echo its tool-call turn back
            // into `history` exactly as OpenRouter sent it (content may be
            // empty, that's fine and expected), then resolve each one
            // locally and append its result as a "tool" message before
            // looping to let the model continue with that information.
            std::string assistant_text;
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                if (!state->messages.empty()) assistant_text = state->messages.back().content;
                // Models usually emit no visible text on a tool-call turn
                // (the "content" is empty, only tool_calls carry meaning)
                // -- drop the now-empty placeholder bubble so the chat
                // shows just the ToolNotice line(s) instead of a blank
                // "Agente:" before them.
                if (assistant_text.empty() && !state->messages.empty() &&
                    state->messages.back().kind == MessageKind::Assistant) {
                    state->messages.pop_back();
                }
            }
            history.push_back({"assistant", assistant_text, "", tool_calls});

            // Fase 7.3: cada tool_call corre en su propio thread -- las
            // tools read-only ya son thread-safe entre sí, apply_edit/
            // design_* serializan por pending_edits_mutex_ y run_project
            // serializa por su propio mailbox de un slot (ver
            // plugin_host.h), así que correr varias en paralelo como
            // mucho hace que una espere a otra, sin condición de carrera
            // nueva. Se preserva el orden original de tool_calls al
            // armar `history` y los ToolNotice, aunque los threads
            // terminen en otro orden.
            {
                std::string names;
                for (size_t i = 0; i < tool_calls.size(); ++i) {
                    if (i) names += ", ";
                    names += tool_calls[i].name;
                }
                SetStatus(state, "Ejecutando " + names);
            }

            std::vector<std::string> results(tool_calls.size());
            {
                std::vector<std::thread> tool_threads;
                tool_threads.reserve(tool_calls.size());
                for (size_t i = 0; i < tool_calls.size(); ++i) {
                    tool_threads.emplace_back([&, i]() {
                        results[i] = ExecuteAnyTool(g_host, tool_calls[i].name, tool_calls[i].arguments);
                    });
                }
                for (auto& t : tool_threads) t.join();
            }

            for (size_t i = 0; i < tool_calls.size(); ++i) {
                const auto& call = tool_calls[i];
                const std::string& result = results[i];
                history.push_back({"tool", result, call.id, {}});

                std::lock_guard<std::mutex> lock(state->mutex);
                state->messages.push_back({MessageKind::ToolNotice, SummarizeToolCall(call.name, call.arguments, result)});
            }
        }

        // Safety net: every explicit `break` above already clears the
        // status, but if the loop instead runs out of iterations
        // (kMaxToolIterations) it never hits one, and without this the
        // spinner would keep showing "Ejecutando ..." forever even
        // though the worker is done.
        SetStatus(state, "");
        state->streaming.store(false);
    });
}

void DrawConfigRow(AvaPanelContext* ctx, AgentState* state) {
    g_host->ui.input_text(ctx, "API Key##ai_agent_key", state->api_key_buffer, sizeof(state->api_key_buffer));
    g_host->ui.same_line(ctx);
    if (g_host->ui.button(ctx, "Guardar Key")) {
        state->api_key = state->api_key_buffer;
        state->has_api_key = !state->api_key.empty();
        PersistConfig(state);
        if (state->has_api_key) FetchModels(state);
    }

    std::lock_guard<std::mutex> lock(state->models_mutex);
    if (state->loading_models.load()) {
        std::string line = std::string(Spinner()) + "  Cargando modelos" + AnimatedDots();
        g_host->ui.label(ctx, line.c_str());
    } else if (!state->models_error.empty()) {
        g_host->ui.text_wrapped(ctx, ("Error al listar modelos: " + state->models_error).c_str());
        if (g_host->ui.button(ctx, "Reintentar")) FetchModels(state);
    } else if (state->models_loaded && !state->model_labels.empty()) {
        std::vector<const char*> items;
        items.reserve(state->model_labels.size());
        for (const auto& label : state->model_labels) items.push_back(label.c_str());

        int index = state->selected_model_index;
        if (g_host->ui.combo(ctx, "Modelo##ai_agent_model", &index, items.data(), static_cast<int>(items.size()))) {
            state->selected_model_index = index;
            state->current_model = state->models[index].id;
            PersistConfig(state);
        }
    } else if (state->has_api_key) {
        if (g_host->ui.button(ctx, "Cargar modelos")) FetchModels(state);
    }

    if (g_host->ui.button(ctx, state->auto_context ? "Contexto automático: ON" : "Contexto automático: OFF")) {
        state->auto_context = !state->auto_context;
    }
    g_host->ui.separator(ctx);
}

void DrawAgentPanel(AvaPanelContext* ctx, void* user_data) {
    auto* state = static_cast<AgentState*>(user_data);
    if (!g_host) return;

    if (!state->streaming.load()) SyncProjectMemory(state);

    DrawConfigRow(ctx, state);

    char cost_buf[64];
    std::snprintf(cost_buf, sizeof(cost_buf), "Costo total: $%.4f", state->accumulated_cost_usd);
    g_host->ui.label(ctx, cost_buf);

    if (g_host->ui.button(ctx, "Borrar historial")) {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->messages.clear();
        state->accumulated_cost_usd = 0.0;
        if (state->project_root_known) ClearAgentMemory(state->current_project_root);
    }

    if (!state->has_api_key) {
        g_host->ui.text_wrapped(ctx, "Ingresá tu OPENROUTER_API_KEY arriba y guardala para empezar a chatear.");
        return;
    }

    bool streaming = state->streaming.load();

    // Fase 9: the input box below grows with the message (see its own
    // comment) -- computed here, before the history child, so that
    // child can reserve exactly this much space and shrink instead of
    // the input box overflowing past a fixed -60px reservation and
    // pushing the Enviar/Detener row (and the panel's bottom edge) down
    // every time a line is added, like a chat app's message list
    // shrinking to make room for a taller compose box.
    int line_count = 1 + static_cast<int>(std::count(state->input_buffer, state->input_buffer + std::strlen(state->input_buffer), '\n'));
    float line_height = g_host->ui.text_line_height(ctx);
    float input_height = std::clamp(line_count * line_height + 10.0f, 32.0f, 160.0f);
    // Room below the history child for the input row itself plus the
    // streaming status line ("Pensando...") that can appear between
    // them -- reserved unconditionally so the layout doesn't jump the
    // instant streaming starts/stops.
    float controls_height = input_height + line_height + 12.0f;

    g_host->ui.begin_child(ctx, "ai_agent_history", -controls_height);
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        for (size_t i = 0; i < state->messages.size(); ++i) {
            const auto& msg = state->messages[i];
            bool is_last = (i == state->messages.size() - 1);
            // The empty placeholder pushed at the start of each turn
            // (see SendMessage) has nothing to show yet -- skip the
            // bubble entirely instead of rendering a bare "Agente: ".
            // The status line below ("Pensando...") is what tells the
            // user something is happening until the first token lands.
            if (msg.kind == MessageKind::Assistant && msg.content.empty()) continue;
            std::string line;
            switch (msg.kind) {
                case MessageKind::User: line = "Tú: " + msg.content; break;
                case MessageKind::Assistant:
                    line = "Agente: " + msg.content;
                    // Tokens are still streaming into this exact bubble --
                    // a blinking caret makes that visible instead of the
                    // text just sitting there looking stalled between
                    // deltas.
                    if (streaming && is_last) line += TypingCursor();
                    break;
                case MessageKind::ToolNotice: line = "  🔧 " + msg.content; break;
            }
            g_host->ui.text_wrapped(ctx, line.c_str());
            g_host->ui.spacing(ctx);
        }
    }
    if (streaming) g_host->ui.scroll_to_bottom(ctx);
    g_host->ui.end_child(ctx);

    // Fase 7.4: status/loading indicator. Covers the gaps a blinking
    // caret can't -- the wait for the first token, and tool execution,
    // where there's no partial text to show at all -- so the panel never
    // just looks frozen while something is actually happening.
    if (streaming) {
        std::string status_line = BuildStatusLine(state);
        if (!status_line.empty()) g_host->ui.text_wrapped(ctx, status_line.c_str());
    }

    // Fase 8: multiline input wired the way every chat app does it --
    // Enter sends, Shift+Enter adds a line -- instead of the old
    // single-line input_text (which had no way to write a multi-line
    // message at all). `submitted` is true only on the frame the user
    // hit plain Enter; the actual "click Enviar" path below still works
    // unchanged for mouse users.
    //
    // The id includes input_reset_counter (bumped by SendMessage right
    // when it clears input_buffer) instead of being a fixed string --
    // this is what actually makes the textbox visibly empty right after
    // sending, on both the Enter and "Enviar" paths: a fixed id would
    // let Dear ImGui keep reusing the same widget instance across
    // frames, which can go on showing its own internal copy of the text
    // instead of picking up that the buffer was cleared out from under
    // it. A new id each send forces a fresh instance that always
    // (re)initializes straight from input_buffer.
    std::string input_id = "##ai_agent_input_" + std::to_string(state->input_reset_counter);
    bool submitted = false;
    g_host->ui.input_text_multiline_submit_hint(ctx, input_id.c_str(), "Escribí un mensaje...",
                                                 state->input_buffer, sizeof(state->input_buffer),
                                                 input_height, &submitted);
    if (submitted && !streaming) SendMessage(state);
    g_host->ui.same_line(ctx);

    bool has_text = state->input_buffer[0] != '\0';
    if (!streaming) {
        if (g_host->ui.button_disabled(ctx, "Enviar", !has_text)) {
            SendMessage(state);
        }
    } else {
        if (g_host->ui.button(ctx, "Detener")) {
            state->stop_requested.store(true);
        }
    }
}

} // namespace

extern "C" int ava_plugin_abi_version() {
    return AVA_STUDIO_PLUGIN_ABI_VERSION;
}

extern "C" bool ava_plugin_init(AvaStudioHost* host) {
    if (!host) return false;
    g_host = host;

    curl_global_init(CURL_GLOBAL_DEFAULT);

    AgentConfig config = LoadAgentConfig();
    if (!config.api_key.empty()) {
        g_state.api_key = config.api_key;
        g_state.has_api_key = true;
    } else if (const char* key = std::getenv("OPENROUTER_API_KEY")) {
        if (key[0] != '\0') {
            g_state.api_key = key;
            g_state.has_api_key = true;
        }
    }
    std::snprintf(g_state.api_key_buffer, sizeof(g_state.api_key_buffer), "%s", g_state.api_key.c_str());

    if (!config.last_model.empty()) {
        g_state.current_model = config.last_model;
    }

    AvaPanelRegistration registration{};
    registration.name = "AI Agent (OpenRouter)";
    registration.draw = &DrawAgentPanel;
    registration.user_data = &g_state;
    registration.default_dock_slot = AVA_DOCK_RIGHT;

    const int panel_id = host->register_panel(host, &registration);
    if (panel_id < 0) return false;

    if (g_state.has_api_key) FetchModels(&g_state);

    host->services.log(host, g_state.has_api_key ? "ai_agent plugin initialized"
                                                  : "ai_agent plugin initialized (sin API key configurada)");
    return true;
}

extern "C" void ava_plugin_shutdown() {
    g_state.stop_requested.store(true);
    if (g_state.worker.joinable()) g_state.worker.join();
    if (g_state.models_worker.joinable()) g_state.models_worker.join();
    curl_global_cleanup();
    g_host = nullptr;
}

// Fase 9: optional metadata, shown in the "Plugins" menu (see
// titlebar_panel.cpp) next to this plugin's checkbox -- update
// ava_plugin_version() whenever this plugin's behavior changes in a
// way worth telling apart at a glance (new Fase, bug fix, etc.).
extern "C" const char* ava_plugin_display_name() {
    return "AI Agent (OpenRouter)";
}

extern "C" const char* ava_plugin_version() {
    return "1.0.0";
}

extern "C" const char* ava_plugin_author() {
    return "Ava Studio";
}
