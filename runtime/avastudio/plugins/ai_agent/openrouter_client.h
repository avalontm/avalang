#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <vector>

// A single tool call the model asked for (either mid-stream, accumulated
// from incremental `delta.tool_calls` fragments, or fully materialized
// once the stream finishes). `arguments` is the raw JSON object the model
// produced for the function's parameters -- the caller (ai_agent_plugin)
// is the one that knows how to interpret it per tool name.
struct OpenRouterToolCall {
    std::string id;
    std::string name;
    std::string arguments; // raw JSON string, e.g. {"path":"src/main.ava"}
};

// One entry of the conversation sent to /chat/completions. Only the
// fields relevant to `role` are meaningful:
//  - "system"/"user": just `content`.
//  - "assistant": `content` (may be empty) plus `tool_calls` when this is
//    the turn where the model asked to call tools -- both are echoed back
//    verbatim so OpenRouter can match the follow-up "tool" messages to
//    the call that produced them.
//  - "tool": `content` is the tool's result (JSON or plain text) and
//    `tool_call_id` must match the id from the assistant's tool_calls
//    entry it answers.
struct OpenRouterMessage {
    std::string role;
    std::string content;
    std::string tool_call_id;                    // only for role == "tool"
    std::vector<OpenRouterToolCall> tool_calls;   // only for role == "assistant" tool-call turns
};

// A tool definition advertised to the model, OpenAI-compatible
// "function" tool shape. `parameters_json_schema` is a raw JSON object
// string (e.g. {"type":"object","properties":{...},"required":[...]}) --
// kept as a string here so this header doesn't need a JSON dependency.
struct OpenRouterToolDef {
    std::string name;
    std::string description;
    std::string parameters_json_schema;
};

// Fase 7.2: tokens consumidos en un turno, tal cual los reporta el
// campo `usage` de OpenRouter. Ambos quedan en 0 si el turno terminó sin
// que el servidor mandara ese chunk (no todos los proveedores lo hacen
// incluso con stream_options.include_usage).
struct OpenRouterUsage {
    long prompt_tokens = 0;
    long completion_tokens = 0;
};

struct OpenRouterModel {
    std::string id;
    std::string name;
    long context_length = 0;
    std::string prompt_price;
    std::string completion_price;
};

class OpenRouterClient {
public:
    explicit OpenRouterClient(std::string api_key);

    // Streams one assistant turn. `tools` may be empty (no tool calling
    // offered at all, e.g. still used as-is by Fases 1-3). This call is
    // synchronous -- it blocks the calling thread until the HTTP request
    // finishes, `on_done` fires exactly once right before it returns.
    //
    // `on_delta` fires for each fragment of visible assistant text (same
    // as before Fase 4). `on_done`'s `tool_calls` is non-empty exactly
    // when the model's turn ended because it wants to call tools (OpenAI
    // `finish_reason == "tool_calls"`) -- the caller is then expected to
    // resolve them and make another StreamChatCompletion call with the
    // tool results appended to `history`, repeating until `tool_calls`
    // comes back empty.
    void StreamChatCompletion(const std::string& model, const std::vector<OpenRouterMessage>& history,
                               const std::vector<OpenRouterToolDef>& tools,
                               const std::function<void(const std::string&)>& on_delta,
                               const std::function<void(bool success, const std::string& error,
                                                         std::vector<OpenRouterToolCall> tool_calls,
                                                         OpenRouterUsage usage)>& on_done,
                               std::atomic<bool>* stop_flag);

    void ListModels(const std::function<void(bool, std::vector<OpenRouterModel>, const std::string&)>& on_done);

private:
    std::string api_key_;
};
