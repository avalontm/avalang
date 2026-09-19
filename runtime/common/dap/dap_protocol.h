#pragma once

#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>

#include "dap/json_value.h"

namespace ava {
namespace dap {

inline std::string EncodeMessage(const JsonValue& message) {
    std::string body = message.Dump();
    return "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
}

inline std::optional<JsonValue> TryExtractMessage(std::string& buffer) {
    size_t header_end = buffer.find("\r\n\r\n");
    if (header_end == std::string::npos) return std::nullopt;

    std::string header = buffer.substr(0, header_end);
    int64_t content_length = -1;
    size_t line_start = 0;
    while (line_start < header.size()) {
        size_t line_end = header.find("\r\n", line_start);
        if (line_end == std::string::npos) line_end = header.size();
        std::string line = header.substr(line_start, line_end - line_start);
        const std::string key = "Content-Length:";
        if (line.compare(0, key.size(), key) == 0) {
            size_t value_start = key.size();
            while (value_start < line.size() && line[value_start] == ' ') ++value_start;
            content_length = std::atoll(line.c_str() + value_start);
        }
        line_start = line_end + 2;
    }

    if (content_length < 0) {
        buffer.erase(0, header_end + 4);
        throw JsonParseError("missing Content-Length header");
    }

    size_t body_start = header_end + 4;
    if (buffer.size() < body_start + static_cast<size_t>(content_length)) {
        return std::nullopt;
    }

    std::string body = buffer.substr(body_start, static_cast<size_t>(content_length));
    buffer.erase(0, body_start + static_cast<size_t>(content_length));
    return ParseJson(body);
}

class SeqCounter {
public:
    int Next() { return ++value_; }

private:
    int value_ = 0;
};

struct DapRequest {
    int seq = 0;
    std::string command;
    JsonValue arguments;
};

inline bool IsRequest(const JsonValue& message) {
    return message.is_object() && message.get("type").as_string() == "request";
}

inline bool IsResponse(const JsonValue& message) {
    return message.is_object() && message.get("type").as_string() == "response";
}

inline bool IsEvent(const JsonValue& message) {
    return message.is_object() && message.get("type").as_string() == "event";
}

inline DapRequest ParseRequest(const JsonValue& message) {
    DapRequest request;
    request.seq = static_cast<int>(message.get("seq").as_int());
    request.command = message.get("command").as_string();
    request.arguments = message.get("arguments");
    return request;
}

inline JsonValue MakeResponse(SeqCounter& seq, const DapRequest& request, JsonValue body = JsonValue()) {
    JsonValue response = JsonValue::MakeObject();
    response.set("seq", JsonValue(static_cast<int64_t>(seq.Next())));
    response.set("type", JsonValue("response"));
    response.set("request_seq", JsonValue(static_cast<int64_t>(request.seq)));
    response.set("success", JsonValue(true));
    response.set("command", JsonValue(request.command));
    if (!body.is_null()) response.set("body", std::move(body));
    return response;
}

inline JsonValue MakeErrorResponse(SeqCounter& seq, const DapRequest& request, const std::string& message) {
    JsonValue response = JsonValue::MakeObject();
    response.set("seq", JsonValue(static_cast<int64_t>(seq.Next())));
    response.set("type", JsonValue("response"));
    response.set("request_seq", JsonValue(static_cast<int64_t>(request.seq)));
    response.set("success", JsonValue(false));
    response.set("command", JsonValue(request.command));
    response.set("message", JsonValue(message));
    return response;
}

inline JsonValue MakeRequest(SeqCounter& seq, const std::string& command, JsonValue arguments = JsonValue()) {
    JsonValue request = JsonValue::MakeObject();
    request.set("seq", JsonValue(static_cast<int64_t>(seq.Next())));
    request.set("type", JsonValue("request"));
    request.set("command", JsonValue(command));
    if (!arguments.is_null()) request.set("arguments", std::move(arguments));
    return request;
}

inline JsonValue MakeEvent(SeqCounter& seq, const std::string& event_name, JsonValue body = JsonValue()) {
    JsonValue event = JsonValue::MakeObject();
    event.set("seq", JsonValue(static_cast<int64_t>(seq.Next())));
    event.set("type", JsonValue("event"));
    event.set("event", JsonValue(event_name));
    if (!body.is_null()) event.set("body", std::move(body));
    return event;
}

}  // namespace dap
}  // namespace ava
