#include "plugin_api.h"
#include "openrouter_client.h"
#include "avalang_reference.h"
#include "config.h"
#include "context_builder.h"
#include "memory.h"
#include "tools.h"

#include <curl/curl.h>

#include <algorithm>
#include <atomic>
#include <cctype>
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

    // Fase 10: `content` is raw text (possibly Markdown, for
    // User/Assistant) as sent to/received from the model -- what's
    // actually persisted/sent as history. `display_cache` is the
    // cleaned-up, styled-for-a-plain-text-widget version DrawAgentPanel
    // hands to ui.selectable_message() (see FormatMarkdownForDisplay).
    // Recomputed lazily, only when `content` has grown since the last
    // draw (streaming appends to the last message every frame; every
    // other message's content is stable after it's added) -- avoids
    // re-parsing every message's Markdown on every single frame just
    // because one of them is still streaming.
    std::string display_cache;
    size_t display_cache_len = 0;
    bool display_cache_valid = false;
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

    // Proveedor activo + su config, ver config.h. `provider` es lo único
    // que decide qué mitad de este struct usa SendMessage/FetchModels --
    // api_key/current_model de arriba quedan intactos al cambiar a
    // Custom y volver (no se pisan entre sí).
    AgentProvider provider = AgentProvider::OpenRouter;
    char custom_base_url_buffer[256] = "";
    char custom_api_key_buffer[256] = "";
    char custom_model_buffer[256] = "";
    std::string custom_base_url;
    std::string custom_api_key;
    std::string custom_model;

    bool IsCustom() const { return provider == AgentProvider::Custom; }

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

    // Modelo de OpenRouter escrito a mano -- el combo de arriba solo
    // ofrece lo que trajo /models, que no siempre lista todo (variantes
    // ":free" en particular tardan en aparecer o quedan afuera según el
    // momento). Este campo no se persiste por sí mismo: al tocar "Usar"
    // pisa state->current_model (que sí se persiste con PersistConfig,
    // igual que si se hubiera elegido del combo).
    char manual_model_buffer[256] = "";

    // Filtro de texto sobre el combo de modelos -- OpenRouter lista
    // varios cientos, no es viable buscar a ojo. No se persiste, es
    // puramente de UI (se resetea solo si el plugin se recarga, lo cual
    // está bien).
    char model_filter_buffer[128] = "";
};

AgentState g_state;
AvaStudioHost* g_host = nullptr;

std::string FormatModelLabel(const OpenRouterModel& model) {
    char buffer[320];
    std::snprintf(buffer, sizeof(buffer), "%s (%ldk ctx)", model.id.c_str(), model.context_length / 1000);
    return buffer;
}

// Substring case-insensitive, sin acentos ni fuzzy matching -- pieza
// base de MatchesModelFilter de abajo.
bool ContainsCaseInsensitive(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return true;
    auto it = std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end(),
                           [](unsigned char a, unsigned char b) { return std::tolower(a) == std::tolower(b); });
    return it != haystack.end();
}

// A diferencia de un substring literal, separa el filtro en palabras
// (por espacios) y pide que TODAS aparezcan en algún lugar del label,
// en cualquier orden -- así escribir "gpt oss free" encuentra
// "openai/gpt-oss-20b:free" sin que el usuario tenga que acordarse de
// los guiones/barras/dos puntos exactos como están en el id real.
bool MatchesModelFilter(const std::string& haystack, const std::string& filter) {
    size_t start = 0;
    while (start <= filter.size()) {
        size_t space = filter.find(' ', start);
        std::string word = filter.substr(start, space == std::string::npos ? std::string::npos : space - start);
        if (!word.empty() && !ContainsCaseInsensitive(haystack, word)) return false;
        if (space == std::string::npos) break;
        start = space + 1;
    }
    return true;
}

// --- Proveedor activo -----------------------------------------------------
// SendMessage/FetchModels no tocan state->api_key/current_model ni
// state->custom_* directamente -- pasan siempre por estos cuatro
// helpers, así que agregar un proveedor nuevo el día de mañana es
// cuestión de tocarlos acá, no de perseguir cada uso.

std::string ActiveApiKey(AgentState* state) {
    return state->IsCustom() ? state->custom_api_key : state->api_key;
}

std::string ActiveBaseUrl(AgentState* state) {
    // Vacío para OpenRouter -- OpenRouterClient interpreta un base_url
    // vacío como "usar el default de OpenRouter", ver su constructor.
    return state->IsCustom() ? state->custom_base_url : "";
}

std::string ActiveModel(AgentState* state) {
    return state->IsCustom() ? state->custom_model : state->current_model;
}

OpenRouterClient MakeClient(AgentState* state) {
    return OpenRouterClient(ActiveApiKey(state), ActiveBaseUrl(state));
}

// Lo mínimo para intentar hablar con el proveedor activo -- gatea
// FetchModels (que se llama automáticamente al abrir el plugin y al
// cambiar de config). Para OpenRouter es la api_key de siempre; para
// Custom alcanza con la base_url (la api_key es opcional y varios
// servidores locales exponen /v1/models sin pedirla).
bool CanTalkToProvider(AgentState* state) {
    return state->IsCustom() ? !state->custom_base_url.empty() : state->has_api_key;
}

// Lo mínimo para habilitar el chat -- gatea el panel principal. A
// diferencia de CanTalkToProvider, en modo Custom también exige un
// modelo escrito a mano: no hay combo automático acá (ver comentario
// de custom_model en config.h), así que sin eso StreamChatCompletion
// se mandaría con un `model` vacío.
bool IsProviderReady(AgentState* state) {
    if (state->IsCustom()) return !state->custom_base_url.empty() && !state->custom_model.empty();
    return state->has_api_key;
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

// --- Chat bubble styling (Fase 10) ---------------------------------------
// AvaUiApi has no rich-text widget (see plugin_api.h) -- a chat bubble is
// one single-style read-only text box (selectable_message), so "Markdown
// support" here means turning the raw **bold**/`code`/# headers/- lists the
// model writes into something that reads cleanly as plain text, not real
// per-run bold/color rendering. Two roles still read as visually distinct
// bubbles via selectable_message's text/background color, which is the
// other half of what was asked for.

// Roles are told apart only by the small colored "Tú"/"Agente" label
// above each message -- no filled background behind the text itself
// (that read as a chat-bubble box, which wasn't wanted). Background is
// fully transparent (alpha 0) so each message just sits in the panel
// like plain text; text color also stays neutral so nothing about the
// message body itself looks tinted or boxed.
constexpr float kMessageTextColor[4] = {0.92f, 0.92f, 0.92f, 1.0f};
constexpr float kMessageBgColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
constexpr float kUserLabelColor[4] = {0.63f, 0.80f, 1.00f, 1.0f};
constexpr float kAssistantLabelColor[4] = {0.78f, 0.86f, 0.62f, 1.0f};
constexpr float kToolNoticeColor[4] = {0.62f, 0.62f, 0.62f, 1.0f};

// Removes a `marker` pair (e.g. "**") from `line`, keeping the text
// between them, for every such pair found left-to-right. Non-greedy:
// each opening marker pairs with the very next one. Used for
// bold/italic, where the surrounding style can't actually be rendered
// in a single-style text box, so the best a plain-text view can do is
// not show the asterisks/underscores themselves.
void StripMarkerPairs(std::string& line, const std::string& marker) {
    size_t pos = 0;
    while (true) {
        size_t open = line.find(marker, pos);
        if (open == std::string::npos) break;
        size_t close = line.find(marker, open + marker.size());
        if (close == std::string::npos) break;
        line.erase(close, marker.size());
        line.erase(open, marker.size());
        pos = open; // re-scan from here in case more pairs follow on the same line
    }
}

// `code` -> «code»: inline code can't be shown in a different font/color
// in a single-style widget either, so it's set apart with guillemets
// instead of just silently dropping the backticks (which would make it
// indistinguishable from prose).
void StripInlineCode(std::string& line) {
    size_t pos = 0;
    while (true) {
        size_t open = line.find('`', pos);
        if (open == std::string::npos) break;
        size_t close = line.find('`', open + 1);
        if (close == std::string::npos) break;
        line.replace(close, 1, "\xC2\xBB");            // »
        line.replace(open, 1, "\xC2\xAB");              // «
        pos = close + 1;
    }
}

// Left-trim helper for detecting line-leading markers (#, -, *) without
// losing the original indentation of everything else.
size_t FirstNonSpace(const std::string& line) {
    size_t i = 0;
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
    return i;
}

// Fase 10: cleans up raw Markdown into something readable inside a
// single-style, selectable text box -- see the block comment above.
// Handles: fenced code blocks (```), ATX headers (#..######), unordered
// list markers (-, *, +), bold/italic markers, inline code, and
// horizontal rules (---/***). Anything else passes through unchanged.
std::string FormatMarkdownForDisplay(const std::string& raw) {
    std::string out;
    out.reserve(raw.size());
    bool in_code_block = false;

    size_t line_start = 0;
    while (line_start <= raw.size()) {
        size_t newline = raw.find('\n', line_start);
        std::string line = raw.substr(line_start, newline == std::string::npos ? std::string::npos : newline - line_start);

        size_t first = FirstNonSpace(line);
        std::string trimmed = line.substr(first);

        if (trimmed.rfind("```", 0) == 0) {
            // Toggle without emitting the fence line itself -- the
            // language tag after ``` (if any) isn't useful without real
            // syntax highlighting anyway.
            in_code_block = !in_code_block;
        } else if (in_code_block) {
            out += "  \xE2\x94\x82 " + line; // "  │ " -- visually sets the block apart, keeps original spacing inside it
            out += '\n';
        } else if (!trimmed.empty() && trimmed[0] == '#') {
            size_t hashes = 0;
            while (hashes < trimmed.size() && trimmed[hashes] == '#') ++hashes;
            if (hashes <= 6 && hashes < trimmed.size() && trimmed[hashes] == ' ') {
                std::string heading = trimmed.substr(hashes + 1);
                out += "\xE2\x96\x8E " + heading; // "▎ " -- a small marker standing in for a header rule
                out += '\n';
            } else {
                out += line;
                out += '\n';
            }
        } else if (trimmed == "---" || trimmed == "***" || trimmed == "___") {
            out += "\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80"; // "────────"
            out += '\n';
        } else {
            bool is_bullet = trimmed.size() >= 2 && (trimmed[0] == '-' || trimmed[0] == '*' || trimmed[0] == '+') &&
                              trimmed[1] == ' ';
            std::string body = is_bullet ? (line.substr(0, first) + "\xE2\x80\xA2 " + trimmed.substr(2)) : line;
            StripMarkerPairs(body, "**");
            StripMarkerPairs(body, "__");
            StripMarkerPairs(body, "*");
            StripMarkerPairs(body, "_");
            StripInlineCode(body);
            out += body;
            out += '\n';
        }

        if (newline == std::string::npos) break;
        line_start = newline + 1;
    }

    // The loop always appends a trailing '\n' for the last line too --
    // drop it so selectable_message's height measurement doesn't count
    // one extra blank line that was never in the original message.
    if (!out.empty() && out.back() == '\n') out.pop_back();
    return out;
}

// Recomputes ChatMessage::display_cache only if `content` changed since
// the last call (see the field's comment) -- cheap check, avoids
// reformatting every message's Markdown on every frame just because the
// last one is still streaming in.
void RefreshDisplayCache(ChatMessage& msg) {
    if (msg.display_cache_valid && msg.display_cache_len == msg.content.size()) return;
    msg.display_cache = FormatMarkdownForDisplay(msg.content);
    msg.display_cache_len = msg.content.size();
    msg.display_cache_valid = true;
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
    config.provider = state->provider;
    config.custom_base_url = state->custom_base_url;
    config.custom_api_key = state->custom_api_key;
    config.custom_model = state->custom_model;
    SaveAgentConfig(config);
}

// Fase 7.1: reconstruye AgentMemory a partir de lo que hoy se muestra en
// el panel -- solo turnos User/Assistant, igual que SendMessage arma
// `history` para mandar a OpenRouter (ToolNotice no se persiste, se
// reconstruiría llamando a las tools de nuevo si hiciera falta).
void PersistMemory(AgentState* state) {
    if (!state->project_root_known) return;
    AgentMemory memory;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        memory.accumulated_cost_usd = state->accumulated_cost_usd;
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
    std::lock_guard<std::mutex> lock(state->mutex);
    state->accumulated_cost_usd = memory.accumulated_cost_usd;
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
        OpenRouterClient client = MakeClient(state);
        std::string active_model = ActiveModel(state);
        client.ListModels([state, active_model](bool success, std::vector<OpenRouterModel> models, const std::string& error) {
            std::lock_guard<std::mutex> lock(state->models_mutex);
            if (success) {
                state->models = std::move(models);
                state->model_labels.clear();
                state->selected_model_index = -1;
                for (size_t i = 0; i < state->models.size(); ++i) {
                    state->model_labels.push_back(FormatModelLabel(state->models[i]));
                    if (state->models[i].id == active_model) {
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

    std::string model = ActiveModel(state);
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

            OpenRouterClient client = MakeClient(state);

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

            if (success) {
                double turn_cost = ComputeTurnCost(state, model, usage);
                std::lock_guard<std::mutex> lock(state->mutex);
                state->accumulated_cost_usd += turn_cost;
            }

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

// Combo de arriba de todo del panel: elige entre el OpenRouter de
// siempre y un endpoint "Custom" compatible con OpenAI (ver config.h).
// Fuera de la función para no reconstruirla cada frame -- son punteros
// a literales, no hace falta.
const char* kProviderLabels[] = {"OpenRouter", "Personalizado (compatible con OpenAI)"};

void DrawOpenRouterSettings(AvaPanelContext* ctx, AgentState* state) {
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
        g_host->ui.input_text(ctx, "Filtrar##ai_agent_model_filter", state->model_filter_buffer,
                               sizeof(state->model_filter_buffer));

        // Índices dentro de state->models/model_labels que matchean el
        // filtro -- se recalcula cada frame (son a lo sumo unos cientos
        // de strings cortos, no vale la pena cachearlo).
        std::vector<int> filtered_indices;
        filtered_indices.reserve(state->model_labels.size());
        for (size_t i = 0; i < state->model_labels.size(); ++i) {
            if (MatchesModelFilter(state->model_labels[i], state->model_filter_buffer)) {
                filtered_indices.push_back(static_cast<int>(i));
            }
        }

        if (filtered_indices.empty()) {
            g_host->ui.text_wrapped(ctx, "Ningún modelo coincide con el filtro.");
        } else {
            std::vector<const char*> items;
            items.reserve(filtered_indices.size());
            for (int idx : filtered_indices) items.push_back(state->model_labels[idx].c_str());

            // El combo trabaja con índices dentro de la lista filtrada,
            // no contra state->models -- hay que traducir para adentro
            // (buscar dónde cayó selected_model_index en la lista
            // filtrada de este frame) y para afuera (mapear de vuelta
            // al índice real al aplicar la selección). Si el modelo
            // elegido quedó afuera del filtro actual, el combo
            // simplemente no muestra nada seleccionado -- no se pierde,
            // sigue siendo state->current_model hasta que se elija otro.
            int display_index = -1;
            for (size_t i = 0; i < filtered_indices.size(); ++i) {
                if (filtered_indices[i] == state->selected_model_index) {
                    display_index = static_cast<int>(i);
                    break;
                }
            }

            if (g_host->ui.combo(ctx, "Modelo##ai_agent_model", &display_index, items.data(),
                                  static_cast<int>(items.size()))) {
                int real_index = filtered_indices[display_index];
                state->selected_model_index = real_index;
                state->current_model = state->models[real_index].id;
                PersistConfig(state);
            }
        }
    } else if (state->has_api_key) {
        if (g_host->ui.button(ctx, "Cargar modelos")) FetchModels(state);
    }

    g_host->ui.separator(ctx);
    g_host->ui.text_wrapped(ctx, "¿No está en la lista? Escribilo tal cual lo usa OpenRouter (ej. "
                                  "openai/gpt-oss-20b:free):");
    g_host->ui.input_text(ctx, "Modelo manual##ai_agent_manual_model", state->manual_model_buffer,
                           sizeof(state->manual_model_buffer));
    g_host->ui.same_line(ctx);
    if (g_host->ui.button_disabled(ctx, "Usar", state->manual_model_buffer[0] == '\0')) {
        state->current_model = state->manual_model_buffer;
        // Ya no corresponde a ningún índice del combo (o puede que sí,
        // si coincide con algo ya listado -- no vale la pena buscarlo,
        // el combo simplemente se muestra sin selección hasta el
        // próximo FetchModels).
        state->selected_model_index = -1;
        PersistConfig(state);
    }
}

// Sin combo de modelos acá a propósito -- a diferencia de OpenRouter,
// un endpoint Custom puede no exponer /v1/models, o exponerlo pero sin
// listar el modelo que el usuario realmente quiere usar (p.ej. LM
// Studio a veces solo lista el que está cargado en ese momento). El
// modelo se escribe a mano, igual que en cualquier cliente OpenAI-
// compatible genérico (curl, Postman, etc).
void DrawCustomSettings(AvaPanelContext* ctx, AgentState* state) {
    g_host->ui.text_wrapped(ctx, "Endpoint compatible con la API de OpenAI (LM Studio, Ollama, vLLM, "
                                  "un proxy propio, etc). La API key es opcional.");
    g_host->ui.input_text(ctx, "Base URL##ai_agent_custom_url", state->custom_base_url_buffer,
                           sizeof(state->custom_base_url_buffer));
    g_host->ui.input_text(ctx, "API Key (opcional)##ai_agent_custom_key", state->custom_api_key_buffer,
                           sizeof(state->custom_api_key_buffer));
    g_host->ui.input_text(ctx, "Modelo##ai_agent_custom_model", state->custom_model_buffer,
                           sizeof(state->custom_model_buffer));
    if (g_host->ui.button(ctx, "Guardar configuración")) {
        state->custom_base_url = state->custom_base_url_buffer;
        state->custom_api_key = state->custom_api_key_buffer;
        state->custom_model = state->custom_model_buffer;
        PersistConfig(state);
        if (!state->custom_base_url.empty()) FetchModels(state);
    }
}

void DrawAgentSettingsPanel(AvaPanelContext* ctx, void* user_data) {
    auto* state = static_cast<AgentState*>(user_data);
    if (!g_host) return;

    int provider_index = state->IsCustom() ? 1 : 0;
    if (g_host->ui.combo(ctx, "Proveedor##ai_agent_provider", &provider_index, kProviderLabels, 2)) {
        state->provider = (provider_index == 1) ? AgentProvider::Custom : AgentProvider::OpenRouter;
        PersistConfig(state);
    }
    g_host->ui.separator(ctx);

    if (state->IsCustom()) {
        DrawCustomSettings(ctx, state);
    } else {
        DrawOpenRouterSettings(ctx, state);
    }

    g_host->ui.separator(ctx);
    if (g_host->ui.button(ctx, state->auto_context ? "Contexto automático: ON" : "Contexto automático: OFF")) {
        state->auto_context = !state->auto_context;
    }
    g_host->ui.separator(ctx);
}

void DrawAgentPanel(AvaPanelContext* ctx, void* user_data) {
    auto* state = static_cast<AgentState*>(user_data);
    if (!g_host) return;

    if (!state->streaming.load()) SyncProjectMemory(state);

    double accumulated_cost_usd;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        accumulated_cost_usd = state->accumulated_cost_usd;
    }
    char cost_buf[64];
    std::snprintf(cost_buf, sizeof(cost_buf), "Costo total: $%.4f", accumulated_cost_usd);
    g_host->ui.label(ctx, cost_buf);

    if (g_host->ui.button(ctx, "Borrar historial")) {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->messages.clear();
        state->accumulated_cost_usd = 0.0;
        if (state->project_root_known) ClearAgentMemory(state->current_project_root);
    }

    if (!IsProviderReady(state)) {
        g_host->ui.text_wrapped(ctx, state->IsCustom()
            ? "Configurá la Base URL y el Modelo del proveedor personalizado en Settings > AI Agent (OpenRouter) para empezar a chatear."
            : "Configurá tu OPENROUTER_API_KEY en Settings > AI Agent (OpenRouter) para empezar a chatear.");
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
            auto& msg = state->messages[i]; // non-const: RefreshDisplayCache may update display_cache
            bool is_last = (i == state->messages.size() - 1);
            // The empty placeholder pushed at the start of each turn
            // (see SendMessage) has nothing to show yet -- skip the
            // bubble entirely instead of rendering an empty one. The
            // status line below ("Pensando...") is what tells the user
            // something is happening until the first token lands.
            if (msg.kind == MessageKind::Assistant && msg.content.empty()) continue;

            if (msg.kind == MessageKind::ToolNotice) {
                std::string line = "  \xF0\x9F\x94\xA7 " + msg.content; // "  🔧 "
                g_host->ui.text_colored(ctx, line.c_str(), kToolNoticeColor[0], kToolNoticeColor[1],
                                         kToolNoticeColor[2], kToolNoticeColor[3]);
                g_host->ui.spacing(ctx);
                continue;
            }

            bool is_user = (msg.kind == MessageKind::User);
            const float* label_color = is_user ? kUserLabelColor : kAssistantLabelColor;

            g_host->ui.text_colored(ctx, is_user ? "Tú" : "Agente", label_color[0], label_color[1], label_color[2],
                                     label_color[3]);

            RefreshDisplayCache(msg);
            std::string display_text = msg.display_cache;
            // Tokens are still streaming into this exact bubble -- a
            // blinking caret makes that visible instead of the text just
            // sitting there looking stalled between deltas. Appended
            // after the cache (not part of it) since it blinks every
            // frame regardless of whether content actually changed.
            if (msg.kind == MessageKind::Assistant && streaming && is_last) display_text += TypingCursor();

            std::string widget_id = "##ai_agent_msg_" + std::to_string(i);
            g_host->ui.selectable_message(ctx, widget_id.c_str(), display_text.c_str(), kMessageTextColor[0],
                                           kMessageTextColor[1], kMessageTextColor[2], kMessageTextColor[3],
                                           kMessageBgColor[0], kMessageBgColor[1], kMessageBgColor[2],
                                           kMessageBgColor[3]);
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

    g_state.provider = config.provider;
    g_state.custom_base_url = config.custom_base_url;
    g_state.custom_api_key = config.custom_api_key;
    g_state.custom_model = config.custom_model;
    std::snprintf(g_state.custom_base_url_buffer, sizeof(g_state.custom_base_url_buffer), "%s",
                  g_state.custom_base_url.c_str());
    std::snprintf(g_state.custom_api_key_buffer, sizeof(g_state.custom_api_key_buffer), "%s",
                  g_state.custom_api_key.c_str());
    std::snprintf(g_state.custom_model_buffer, sizeof(g_state.custom_model_buffer), "%s",
                  g_state.custom_model.c_str());

    AvaPanelRegistration registration{};
    registration.name = "AI Agent (OpenRouter)";
    registration.draw = &DrawAgentPanel;
    registration.user_data = &g_state;
    registration.default_dock_slot = AVA_DOCK_RIGHT;

    const int panel_id = host->register_panel(host, &registration);
    if (panel_id < 0) return false;

    AvaPanelRegistration settings_registration{};
    settings_registration.name = "AI Agent (OpenRouter)";
    settings_registration.draw = &DrawAgentSettingsPanel;
    settings_registration.user_data = &g_state;
    settings_registration.is_settings = true;
    const int settings_panel_id = host->register_panel(host, &settings_registration);
    if (settings_panel_id < 0) return false;

    if (CanTalkToProvider(&g_state)) FetchModels(&g_state);

    host->services.log(host, IsProviderReady(&g_state) ? "ai_agent plugin initialized"
                                                         : "ai_agent plugin initialized (sin proveedor configurado)");
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
    return "1.1.0";
}

extern "C" const char* ava_plugin_author() {
    return "Ava Studio";
}
