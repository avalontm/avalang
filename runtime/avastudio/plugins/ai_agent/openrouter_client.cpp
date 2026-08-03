#include "openrouter_client.h"

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <map>

using json = nlohmann::json;

namespace {

// Accumulates one in-progress tool call across however many SSE deltas
// its `arguments` string arrives split over (OpenAI/OpenRouter stream
// function arguments as incremental string fragments, not as one chunk).
struct PendingToolCall {
    std::string id;
    std::string name;
    std::string arguments;
};

struct SseContext {
    std::string leftover;
    std::function<void(const std::string&)> on_delta;
    std::string error;

    // Keyed by the `index` field OpenAI-style deltas use to say which
    // tool call (of possibly several in parallel) a fragment belongs to.
    std::map<int, PendingToolCall> tool_calls;
    bool saw_tool_calls = false;

    // Fase 7.2: el chunk final del stream (cuando se pide
    // stream_options.include_usage) trae un objeto "usage" a nivel raíz,
    // no dentro de "choices" -- se guarda tal cual llega, se sobreescribe
    // si por algún motivo llegara más de una vez.
    OpenRouterUsage usage;
};

void ApplyToolCallDelta(SseContext* ctx, const json& delta_tool_calls) {
    if (!delta_tool_calls.is_array()) return;
    ctx->saw_tool_calls = true;
    for (const auto& tc : delta_tool_calls) {
        int index = tc.value("index", 0);
        auto& pending = ctx->tool_calls[index];
        if (tc.contains("id") && tc["id"].is_string()) pending.id = tc["id"].get<std::string>();
        if (tc.contains("function") && tc["function"].is_object()) {
            const auto& fn = tc["function"];
            if (fn.contains("name") && fn["name"].is_string()) pending.name += fn["name"].get<std::string>();
            if (fn.contains("arguments") && fn["arguments"].is_string()) {
                pending.arguments += fn["arguments"].get<std::string>();
            }
        }
    }
}

size_t WriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    size_t total = size * nmemb;
    auto* ctx = static_cast<SseContext*>(userdata);
    ctx->leftover.append(ptr, total);

    size_t pos;
    while ((pos = ctx->leftover.find('\n')) != std::string::npos) {
        std::string line = ctx->leftover.substr(0, pos);
        ctx->leftover.erase(0, pos + 1);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        if (line.rfind("data: ", 0) != 0) continue;

        std::string payload = line.substr(6);
        if (payload == "[DONE]") continue;

        try {
            json parsed = json::parse(payload);
            if (parsed.contains("error")) {
                ctx->error = parsed["error"].value("message", "unknown error");
                continue;
            }
            if (parsed.contains("usage") && parsed["usage"].is_object()) {
                ctx->usage.prompt_tokens = parsed["usage"].value("prompt_tokens", 0L);
                ctx->usage.completion_tokens = parsed["usage"].value("completion_tokens", 0L);
            }
            if (!parsed.contains("choices") || parsed["choices"].empty()) continue;
            auto& choice = parsed["choices"][0];
            if (!choice.contains("delta")) continue;
            auto& delta = choice["delta"];

            if (delta.contains("tool_calls")) {
                ApplyToolCallDelta(ctx, delta["tool_calls"]);
            }
            if (delta.contains("content") && delta["content"].is_string()) {
                std::string text = delta["content"].get<std::string>();
                if (!text.empty()) ctx->on_delta(text);
            }
        } catch (...) {
        }
    }
    return total;
}

int ProgressCallback(void* clientp, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
    auto* stop_flag = static_cast<std::atomic<bool>*>(clientp);
    return (stop_flag && stop_flag->load()) ? 1 : 0;
}

size_t PlainWriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    size_t total = size * nmemb;
    static_cast<std::string*>(userdata)->append(ptr, total);
    return total;
}

json ToolCallsToJson(const std::vector<OpenRouterToolCall>& tool_calls) {
    json arr = json::array();
    for (const auto& tc : tool_calls) {
        json item;
        item["id"] = tc.id;
        item["type"] = "function";
        item["function"] = {{"name", tc.name}, {"arguments", tc.arguments}};
        arr.push_back(std::move(item));
    }
    return arr;
}

} // namespace

OpenRouterClient::OpenRouterClient(std::string api_key) : api_key_(std::move(api_key)) {}

void OpenRouterClient::StreamChatCompletion(
    const std::string& model, const std::vector<OpenRouterMessage>& history,
    const std::vector<OpenRouterToolDef>& tools, const std::function<void(const std::string&)>& on_delta,
    const std::function<void(bool success, const std::string& error, std::vector<OpenRouterToolCall> tool_calls,
                              OpenRouterUsage usage)>&
        on_done,
    std::atomic<bool>* stop_flag) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        on_done(false, "no se pudo inicializar libcurl", {}, {});
        return;
    }

    json body;
    body["model"] = model;
    body["stream"] = true;
    // Fase 7.2: sin esto OpenRouter no manda el chunk final de "usage"
    // en modo streaming (varía por proveedor, pero pedirlo no rompe a
    // los que ya lo mandan solos).
    body["stream_options"] = json{{"include_usage", true}};
    body["messages"] = json::array();
    for (const auto& m : history) {
        json msg;
        msg["role"] = m.role;
        if (m.role == "assistant" && !m.tool_calls.empty()) {
            // OpenAI-compatible tool-call turn: content can legitimately
            // be empty/null when the model only emitted tool calls.
            if (m.content.empty()) {
                msg["content"] = nullptr;
            } else {
                msg["content"] = m.content;
            }
            msg["tool_calls"] = ToolCallsToJson(m.tool_calls);
        } else {
            msg["content"] = m.content;
        }
        if (m.role == "tool") {
            msg["tool_call_id"] = m.tool_call_id;
        }
        body["messages"].push_back(std::move(msg));
    }

    if (!tools.empty()) {
        json tools_json = json::array();
        for (const auto& t : tools) {
            json entry;
            entry["type"] = "function";
            json fn;
            fn["name"] = t.name;
            fn["description"] = t.description;
            try {
                fn["parameters"] = json::parse(t.parameters_json_schema);
            } catch (...) {
                fn["parameters"] = json{{"type", "object"}, {"properties", json::object()}};
            }
            entry["function"] = std::move(fn);
            tools_json.push_back(std::move(entry));
        }
        body["tools"] = std::move(tools_json);
    }

    std::string body_str = body.dump();

    SseContext ctx;
    ctx.on_delta = on_delta;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    std::string auth_header = "Authorization: Bearer " + api_key_;
    headers = curl_slist_append(headers, auth_header.c_str());
    headers = curl_slist_append(headers, "HTTP-Referer: https://avalang.dev");
    headers = curl_slist_append(headers, "X-Title: Ava Studio AI Agent");

    curl_easy_setopt(curl, CURLOPT_URL, "https://openrouter.ai/api/v1/chat/completions");
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_str.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body_str.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, &ProgressCallback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, stop_flag);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 20L);

    CURLcode res = curl_easy_perform(curl);

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res == CURLE_ABORTED_BY_CALLBACK) {
        on_done(false, "detenido por el usuario", {}, {});
        return;
    }
    if (res != CURLE_OK) {
        on_done(false, curl_easy_strerror(res), {}, {});
        return;
    }
    if (!ctx.error.empty()) {
        on_done(false, ctx.error, {}, {});
        return;
    }
    if (http_code >= 400) {
        on_done(false, "HTTP " + std::to_string(http_code), {}, {});
        return;
    }

    std::vector<OpenRouterToolCall> tool_calls;
    if (ctx.saw_tool_calls) {
        // ctx.tool_calls is keyed by the `index` OpenAI-style deltas use --
        // std::map iterates in ascending key order, so this naturally
        // preserves the order the model emitted the calls in.
        for (auto& [index, pending] : ctx.tool_calls) {
            (void)index;
            if (pending.name.empty()) continue; // malformed/partial fragment, skip
            OpenRouterToolCall tc;
            tc.id = pending.id;
            tc.name = pending.name;
            tc.arguments = pending.arguments;
            tool_calls.push_back(std::move(tc));
        }
    }

    on_done(true, "", std::move(tool_calls), ctx.usage);
}

void OpenRouterClient::ListModels(
    const std::function<void(bool, std::vector<OpenRouterModel>, const std::string&)>& on_done) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        on_done(false, {}, "no se pudo inicializar libcurl");
        return;
    }

    std::string response;
    struct curl_slist* headers = nullptr;
    if (!api_key_.empty()) {
        std::string auth_header = "Authorization: Bearer " + api_key_;
        headers = curl_slist_append(headers, auth_header.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, "https://openrouter.ai/api/v1/models");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &PlainWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);

    CURLcode res = curl_easy_perform(curl);

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        on_done(false, {}, curl_easy_strerror(res));
        return;
    }
    if (http_code >= 400) {
        on_done(false, {}, "HTTP " + std::to_string(http_code));
        return;
    }

    try {
        json parsed = json::parse(response);
        std::vector<OpenRouterModel> models;
        for (auto& entry : parsed.value("data", json::array())) {
            OpenRouterModel model;
            model.id = entry.value("id", "");
            model.name = entry.value("name", model.id);
            model.context_length = entry.value("context_length", 0L);
            if (entry.contains("pricing")) {
                model.prompt_price = entry["pricing"].value("prompt", "");
                model.completion_price = entry["pricing"].value("completion", "");
            }
            if (!model.id.empty()) models.push_back(std::move(model));
        }
        on_done(true, std::move(models), "");
    } catch (const std::exception& e) {
        on_done(false, {}, e.what());
    }
}
