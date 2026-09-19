#pragma once

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ava {
namespace dap {

class JsonValue;
using JsonArray = std::vector<JsonValue>;
using JsonObject = std::vector<std::pair<std::string, JsonValue>>;

enum class JsonType {
    Null,
    Bool,
    Int,
    Double,
    String,
    Array,
    Object,
};

class JsonValue {
public:
    JsonValue() : type_(JsonType::Null) {}
    JsonValue(std::nullptr_t) : type_(JsonType::Null) {}
    JsonValue(bool value) : type_(JsonType::Bool), bool_(value) {}
    JsonValue(int value) : type_(JsonType::Int), int_(value) {}
    JsonValue(int64_t value) : type_(JsonType::Int), int_(value) {}
    JsonValue(double value) : type_(JsonType::Double), double_(value) {}
    JsonValue(const char* value) : type_(JsonType::String), string_(value) {}
    JsonValue(std::string value) : type_(JsonType::String), string_(std::move(value)) {}
    JsonValue(JsonArray value) : type_(JsonType::Array), array_(std::move(value)) {}
    JsonValue(JsonObject value) : type_(JsonType::Object), object_(std::move(value)) {}

    static JsonValue MakeArray() { return JsonValue(JsonArray{}); }
    static JsonValue MakeObject() { return JsonValue(JsonObject{}); }

    JsonType type() const { return type_; }
    bool is_null() const { return type_ == JsonType::Null; }
    bool is_bool() const { return type_ == JsonType::Bool; }
    bool is_number() const { return type_ == JsonType::Int || type_ == JsonType::Double; }
    bool is_string() const { return type_ == JsonType::String; }
    bool is_array() const { return type_ == JsonType::Array; }
    bool is_object() const { return type_ == JsonType::Object; }

    bool as_bool(bool fallback = false) const {
        return type_ == JsonType::Bool ? bool_ : fallback;
    }

    int64_t as_int(int64_t fallback = 0) const {
        if (type_ == JsonType::Int) return int_;
        if (type_ == JsonType::Double) return static_cast<int64_t>(double_);
        return fallback;
    }

    double as_double(double fallback = 0.0) const {
        if (type_ == JsonType::Double) return double_;
        if (type_ == JsonType::Int) return static_cast<double>(int_);
        return fallback;
    }

    std::string as_string(std::string fallback = std::string()) const {
        return type_ == JsonType::String ? string_ : std::move(fallback);
    }

    const JsonArray& as_array() const {
        static const JsonArray empty;
        return type_ == JsonType::Array ? array_ : empty;
    }

    JsonArray& as_array() {
        if (type_ != JsonType::Array) {
            type_ = JsonType::Array;
            array_.clear();
        }
        return array_;
    }

    const JsonObject& as_object() const {
        static const JsonObject empty;
        return type_ == JsonType::Object ? object_ : empty;
    }

    JsonObject& as_object() {
        if (type_ != JsonType::Object) {
            type_ = JsonType::Object;
            object_.clear();
        }
        return object_;
    }

    bool has(const std::string& key) const {
        if (type_ != JsonType::Object) return false;
        for (const auto& entry : object_) {
            if (entry.first == key) return true;
        }
        return false;
    }

    const JsonValue& get(const std::string& key) const {
        static const JsonValue null_value;
        if (type_ == JsonType::Object) {
            for (const auto& entry : object_) {
                if (entry.first == key) return entry.second;
            }
        }
        return null_value;
    }

    void set(const std::string& key, JsonValue value) {
        if (type_ != JsonType::Object) {
            type_ = JsonType::Object;
            object_.clear();
        }
        for (auto& entry : object_) {
            if (entry.first == key) {
                entry.second = std::move(value);
                return;
            }
        }
        object_.emplace_back(key, std::move(value));
    }

    void push_back(JsonValue value) {
        if (type_ != JsonType::Array) {
            type_ = JsonType::Array;
            array_.clear();
        }
        array_.push_back(std::move(value));
    }

    std::string Dump() const {
        std::string out;
        DumpTo(out);
        return out;
    }

private:
    void DumpTo(std::string& out) const {
        switch (type_) {
            case JsonType::Null:
                out += "null";
                break;
            case JsonType::Bool:
                out += bool_ ? "true" : "false";
                break;
            case JsonType::Int: {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(int_));
                out += buf;
                break;
            }
            case JsonType::Double: {
                char buf[64];
                std::snprintf(buf, sizeof(buf), "%.17g", double_);
                out += buf;
                break;
            }
            case JsonType::String:
                out += '"';
                EscapeInto(string_, out);
                out += '"';
                break;
            case JsonType::Array: {
                out += '[';
                for (size_t i = 0; i < array_.size(); ++i) {
                    if (i) out += ',';
                    array_[i].DumpTo(out);
                }
                out += ']';
                break;
            }
            case JsonType::Object: {
                out += '{';
                for (size_t i = 0; i < object_.size(); ++i) {
                    if (i) out += ',';
                    out += '"';
                    EscapeInto(object_[i].first, out);
                    out += "\":";
                    object_[i].second.DumpTo(out);
                }
                out += '}';
                break;
            }
        }
    }

    static void EscapeInto(const std::string& raw, std::string& out) {
        for (unsigned char c : raw) {
            switch (c) {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                case '\b': out += "\\b"; break;
                case '\f': out += "\\f"; break;
                default:
                    if (c < 0x20) {
                        char buf[8];
                        std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                        out += buf;
                    } else {
                        out += static_cast<char>(c);
                    }
            }
        }
    }

    JsonType type_;
    bool bool_ = false;
    int64_t int_ = 0;
    double double_ = 0.0;
    std::string string_;
    JsonArray array_;
    JsonObject object_;
};

class JsonParseError : public std::runtime_error {
public:
    explicit JsonParseError(const std::string& message) : std::runtime_error(message) {}
};

namespace detail {

class JsonParser {
public:
    explicit JsonParser(const std::string& text) : text_(text), pos_(0) {}

    JsonValue Parse() {
        SkipWhitespace();
        JsonValue value = ParseValue();
        SkipWhitespace();
        if (pos_ != text_.size()) {
            throw JsonParseError("trailing data after JSON value");
        }
        return value;
    }

private:
    JsonValue ParseValue() {
        if (pos_ >= text_.size()) throw JsonParseError("unexpected end of input");
        char c = text_[pos_];
        switch (c) {
            case '{': return ParseObject();
            case '[': return ParseArray();
            case '"': return JsonValue(ParseString());
            case 't':
            case 'f': return ParseBool();
            case 'n': return ParseNull();
            default: return ParseNumber();
        }
    }

    JsonValue ParseObject() {
        Expect('{');
        JsonObject object;
        SkipWhitespace();
        if (Peek() == '}') {
            ++pos_;
            return JsonValue(std::move(object));
        }
        while (true) {
            SkipWhitespace();
            std::string key = ParseString();
            SkipWhitespace();
            Expect(':');
            SkipWhitespace();
            JsonValue value = ParseValue();
            object.emplace_back(std::move(key), std::move(value));
            SkipWhitespace();
            char next = Peek();
            if (next == ',') {
                ++pos_;
                continue;
            }
            if (next == '}') {
                ++pos_;
                break;
            }
            throw JsonParseError("expected ',' or '}' in object");
        }
        return JsonValue(std::move(object));
    }

    JsonValue ParseArray() {
        Expect('[');
        JsonArray array;
        SkipWhitespace();
        if (Peek() == ']') {
            ++pos_;
            return JsonValue(std::move(array));
        }
        while (true) {
            SkipWhitespace();
            array.push_back(ParseValue());
            SkipWhitespace();
            char next = Peek();
            if (next == ',') {
                ++pos_;
                continue;
            }
            if (next == ']') {
                ++pos_;
                break;
            }
            throw JsonParseError("expected ',' or ']' in array");
        }
        return JsonValue(std::move(array));
    }

    std::string ParseString() {
        Expect('"');
        std::string out;
        while (true) {
            if (pos_ >= text_.size()) throw JsonParseError("unterminated string");
            char c = text_[pos_++];
            if (c == '"') break;
            if (c == '\\') {
                if (pos_ >= text_.size()) throw JsonParseError("unterminated escape");
                char esc = text_[pos_++];
                switch (esc) {
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/'; break;
                    case 'n': out += '\n'; break;
                    case 't': out += '\t'; break;
                    case 'r': out += '\r'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'u': out += EncodeUtf8(ParseUnicodeEscape()); break;
                    default: throw JsonParseError("invalid escape sequence");
                }
            } else {
                out += c;
            }
        }
        return out;
    }

    uint32_t ParseUnicodeEscape() {
        uint32_t code = ParseHex4();
        if (code >= 0xD800 && code <= 0xDBFF) {
            if (pos_ + 1 < text_.size() && text_[pos_] == '\\' && text_[pos_ + 1] == 'u') {
                size_t save = pos_;
                pos_ += 2;
                uint32_t low = ParseHex4();
                if (low >= 0xDC00 && low <= 0xDFFF) {
                    return 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                }
                pos_ = save;
            }
            return 0xFFFD;
        }
        return code;
    }

    uint32_t ParseHex4() {
        if (pos_ + 4 > text_.size()) throw JsonParseError("invalid unicode escape");
        uint32_t value = 0;
        for (int i = 0; i < 4; ++i) {
            char c = text_[pos_++];
            value <<= 4;
            if (c >= '0' && c <= '9') value |= static_cast<uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f') value |= static_cast<uint32_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') value |= static_cast<uint32_t>(c - 'A' + 10);
            else throw JsonParseError("invalid hex digit in unicode escape");
        }
        return value;
    }

    static std::string EncodeUtf8(uint32_t code) {
        std::string out;
        if (code <= 0x7F) {
            out += static_cast<char>(code);
        } else if (code <= 0x7FF) {
            out += static_cast<char>(0xC0 | (code >> 6));
            out += static_cast<char>(0x80 | (code & 0x3F));
        } else if (code <= 0xFFFF) {
            out += static_cast<char>(0xE0 | (code >> 12));
            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (code & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (code >> 18));
            out += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (code & 0x3F));
        }
        return out;
    }

    JsonValue ParseBool() {
        if (text_.compare(pos_, 4, "true") == 0) {
            pos_ += 4;
            return JsonValue(true);
        }
        if (text_.compare(pos_, 5, "false") == 0) {
            pos_ += 5;
            return JsonValue(false);
        }
        throw JsonParseError("invalid literal");
    }

    JsonValue ParseNull() {
        if (text_.compare(pos_, 4, "null") == 0) {
            pos_ += 4;
            return JsonValue();
        }
        throw JsonParseError("invalid literal");
    }

    JsonValue ParseNumber() {
        size_t start = pos_;
        bool is_double = false;
        if (Peek() == '-') ++pos_;
        while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) ++pos_;
        if (pos_ < text_.size() && text_[pos_] == '.') {
            is_double = true;
            ++pos_;
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) ++pos_;
        }
        if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E')) {
            is_double = true;
            ++pos_;
            if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-')) ++pos_;
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) ++pos_;
        }
        if (pos_ == start) throw JsonParseError("invalid number");
        std::string token = text_.substr(start, pos_ - start);
        if (is_double) return JsonValue(std::stod(token));
        return JsonValue(static_cast<int64_t>(std::stoll(token)));
    }

    void SkipWhitespace() {
        while (pos_ < text_.size()) {
            char c = text_[pos_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++pos_;
            else break;
        }
    }

    char Peek() const { return pos_ < text_.size() ? text_[pos_] : '\0'; }

    void Expect(char c) {
        if (pos_ >= text_.size() || text_[pos_] != c) {
            throw JsonParseError(std::string("expected '") + c + "'");
        }
        ++pos_;
    }

    const std::string& text_;
    size_t pos_;
};

}  // namespace detail

inline JsonValue ParseJson(const std::string& text) {
    detail::JsonParser parser(text);
    return parser.Parse();
}

}  // namespace dap
}  // namespace ava
