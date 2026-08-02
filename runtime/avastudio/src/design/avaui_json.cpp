#include "design/avaui_json.h"

#include <cctype>
#include <sstream>

namespace studio::design {

namespace {

// --- Tiny generic JSON value, internal only ----------------------------
// Just enough to round-trip the .avaui shape: object, array, string,
// bool, and "no distinction needed" for number (kept as string too --
// nothing here does arithmetic on JSON numbers). Not exposed outside
// this file; see avaui_json.h's header comment for why this isn't a
// general JSON library.
struct JsonValue {
    enum class Kind { Null, Bool, String, Array, Object } kind = Kind::Null;
    bool b = false;
    std::string str; // used for both String and (verbatim, unquoted) Number tokens
    std::vector<JsonValue> arr;
    std::vector<std::pair<std::string, JsonValue>> obj; // insertion-ordered

    static JsonValue MakeString(std::string s) {
        JsonValue v;
        v.kind = Kind::String;
        v.str = std::move(s);
        return v;
    }
    static JsonValue MakeObject() {
        JsonValue v;
        v.kind = Kind::Object;
        return v;
    }
    static JsonValue MakeArray() {
        JsonValue v;
        v.kind = Kind::Array;
        return v;
    }

    const JsonValue* Find(const std::string& key) const {
        for (const auto& [k, val] : obj) {
            if (k == key) return &val;
        }
        return nullptr;
    }
};

// --- Parser --------------------------------------------------------------
// Small recursive-descent parser. On any malformed input it sets
// `error` and returns a default JsonValue -- callers check `error`
// after the top-level Parse() call rather than threading exceptions
// through, matching the rest of the codebase's no-exceptions style
// (see e.g. util/csv.cpp).
class JsonParser {
public:
    explicit JsonParser(const std::string& text) : text_(text) {}

    JsonValue Parse() {
        SkipWs();
        JsonValue v = ParseValue();
        if (!error_.empty()) return {};
        SkipWs();
        if (pos_ != text_.size()) {
            error_ = "texto sobrante después del JSON";
            return {};
        }
        return v;
    }

    const std::string& Error() const { return error_; }

private:
    const std::string& text_;
    size_t pos_ = 0;
    std::string error_;

    char Peek() const { return pos_ < text_.size() ? text_[pos_] : '\0'; }
    char Get() { return pos_ < text_.size() ? text_[pos_++] : '\0'; }

    void SkipWs() {
        while (pos_ < text_.size()) {
            char c = text_[pos_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                ++pos_;
            } else {
                break;
            }
        }
    }

    bool Expect(char c) {
        if (Peek() != c) {
            error_ = std::string("se esperaba '") + c + "'";
            return false;
        }
        ++pos_;
        return true;
    }

    JsonValue ParseValue() {
        if (!error_.empty()) return {};
        SkipWs();
        char c = Peek();
        if (c == '{') return ParseObject();
        if (c == '[') return ParseArray();
        if (c == '"') return JsonValue::MakeString(ParseRawString());
        if (c == 't' || c == 'f') return ParseBool();
        if (c == 'n') return ParseNull();
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return ParseNumber();
        error_ = "valor JSON inesperado";
        return {};
    }

    JsonValue ParseObject() {
        JsonValue v = JsonValue::MakeObject();
        if (!Expect('{')) return {};
        SkipWs();
        if (Peek() == '}') {
            ++pos_;
            return v;
        }
        while (true) {
            SkipWs();
            if (Peek() != '"') {
                error_ = "se esperaba una clave string";
                return {};
            }
            std::string key = ParseRawString();
            if (!error_.empty()) return {};
            SkipWs();
            if (!Expect(':')) return {};
            JsonValue val = ParseValue();
            if (!error_.empty()) return {};
            v.obj.emplace_back(std::move(key), std::move(val));
            SkipWs();
            char c = Get();
            if (c == ',') continue;
            if (c == '}') break;
            error_ = "se esperaba ',' o '}' en objeto";
            return {};
        }
        return v;
    }

    JsonValue ParseArray() {
        JsonValue v = JsonValue::MakeArray();
        if (!Expect('[')) return {};
        SkipWs();
        if (Peek() == ']') {
            ++pos_;
            return v;
        }
        while (true) {
            JsonValue val = ParseValue();
            if (!error_.empty()) return {};
            v.arr.push_back(std::move(val));
            SkipWs();
            char c = Get();
            if (c == ',') continue;
            if (c == ']') break;
            error_ = "se esperaba ',' o ']' en array";
            return {};
        }
        return v;
    }

    std::string ParseRawString() {
        if (!Expect('"')) return "";
        std::string out;
        while (true) {
            if (pos_ >= text_.size()) {
                error_ = "string sin cerrar";
                return "";
            }
            char c = Get();
            if (c == '"') break;
            if (c == '\\') {
                char esc = Get();
                switch (esc) {
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'n': out += '\n'; break;
                    case 'r': out += '\r'; break;
                    case 't': out += '\t'; break;
                    case 'u': {
                        // Basic \uXXXX -> UTF-8 (BMP only, no surrogate
                        // pairs -- .avaui content is Designer-generated
                        // or hand-edited AvaLang identifiers/text, not
                        // expected to need astral-plane escapes).
                        if (pos_ + 4 > text_.size()) {
                            error_ = "escape \\u incompleto";
                            return "";
                        }
                        unsigned int code = 0;
                        for (int i = 0; i < 4; ++i) {
                            char h = Get();
                            code <<= 4;
                            if (h >= '0' && h <= '9') code |= static_cast<unsigned int>(h - '0');
                            else if (h >= 'a' && h <= 'f') code |= static_cast<unsigned int>(h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F') code |= static_cast<unsigned int>(h - 'A' + 10);
                            else { error_ = "escape \\u inválido"; return ""; }
                        }
                        if (code < 0x80) {
                            out += static_cast<char>(code);
                        } else if (code < 0x800) {
                            out += static_cast<char>(0xC0 | (code >> 6));
                            out += static_cast<char>(0x80 | (code & 0x3F));
                        } else {
                            out += static_cast<char>(0xE0 | (code >> 12));
                            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                            out += static_cast<char>(0x80 | (code & 0x3F));
                        }
                        break;
                    }
                    default:
                        error_ = "escape inválido en string";
                        return "";
                }
                continue;
            }
            out += c;
        }
        return out;
    }

    JsonValue ParseBool() {
        JsonValue v;
        v.kind = JsonValue::Kind::Bool;
        if (text_.compare(pos_, 4, "true") == 0) {
            v.b = true;
            pos_ += 4;
        } else if (text_.compare(pos_, 5, "false") == 0) {
            v.b = false;
            pos_ += 5;
        } else {
            error_ = "literal booleano inválido";
        }
        return v;
    }

    JsonValue ParseNull() {
        if (text_.compare(pos_, 4, "null") == 0) {
            pos_ += 4;
            return {}; // default-constructed = Kind::Null
        }
        error_ = "literal 'null' inválido";
        return {};
    }

    JsonValue ParseNumber() {
        size_t start = pos_;
        if (Peek() == '-') ++pos_;
        while (std::isdigit(static_cast<unsigned char>(Peek()))) ++pos_;
        if (Peek() == '.') {
            ++pos_;
            while (std::isdigit(static_cast<unsigned char>(Peek()))) ++pos_;
        }
        if (Peek() == 'e' || Peek() == 'E') {
            ++pos_;
            if (Peek() == '+' || Peek() == '-') ++pos_;
            while (std::isdigit(static_cast<unsigned char>(Peek()))) ++pos_;
        }
        if (pos_ == start) {
            error_ = "número inválido";
            return {};
        }
        JsonValue v;
        v.kind = JsonValue::Kind::String; // stored verbatim, see struct comment
        v.str = text_.substr(start, pos_ - start);
        return v;
    }
};

// Stringifies any scalar JsonValue (String/Bool/Number/Null) the way
// PropertyRow::value expects -- "already stringified" per
// properties_panel.h. Objects/Arrays shouldn't reach here for a
// property/event value; returns "" for those rather than asserting,
// since a hand-edited .avaui is user input, not a programming error.
std::string ScalarToPropertyString(const JsonValue& v) {
    switch (v.kind) {
        case JsonValue::Kind::String: return v.str;
        case JsonValue::Kind::Bool:   return v.b ? "true" : "false";
        case JsonValue::Kind::Null:   return "";
        default:                      return "";
    }
}

// --- Writer ----------------------------------------------------------------
void WriteJsonString(std::ostringstream& out, const std::string& s) {
    out << '"';
    for (char c : s) {
        switch (c) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out << buf;
                } else {
                    out << c;
                }
        }
    }
    out << '"';
}

void WriteIndent(std::ostringstream& out, int depth) {
    for (int i = 0; i < depth; ++i) out << "  ";
}

void WritePropertyRows(std::ostringstream& out, const std::vector<PropertyRow>& rows, int depth) {
    out << "{";
    if (!rows.empty()) {
        out << "\n";
        for (size_t i = 0; i < rows.size(); ++i) {
            WriteIndent(out, depth + 1);
            WriteJsonString(out, rows[i].key);
            out << ": ";
            WriteJsonString(out, rows[i].value);
            if (i + 1 < rows.size()) out << ",";
            out << "\n";
        }
        WriteIndent(out, depth);
    }
    out << "}";
}

void WriteNode(std::ostringstream& out, const DesignNode& node, int depth) {
    out << "{\n";
    WriteIndent(out, depth + 1);
    out << "\"type\": ";
    WriteJsonString(out, node.type);
    out << ",\n";

    WriteIndent(out, depth + 1);
    out << "\"id\": ";
    WriteJsonString(out, node.id);
    out << ",\n";

    WriteIndent(out, depth + 1);
    out << "\"properties\": ";
    WritePropertyRows(out, node.properties, depth + 1);
    out << ",\n";

    WriteIndent(out, depth + 1);
    out << "\"events\": ";
    WritePropertyRows(out, node.events, depth + 1);
    out << ",\n";

    WriteIndent(out, depth + 1);
    out << "\"children\": [";
    if (!node.children.empty()) {
        out << "\n";
        for (size_t i = 0; i < node.children.size(); ++i) {
            WriteIndent(out, depth + 2);
            WriteNode(out, node.children[i], depth + 2);
            if (i + 1 < node.children.size()) out << ",";
            out << "\n";
        }
        WriteIndent(out, depth + 1);
    }
    out << "]\n";

    WriteIndent(out, depth);
    out << "}";
}

// Converts a parsed JsonValue object (the shape WriteNode produces)
// back into a DesignNode. node_uid is intentionally left empty here --
// see ComponentTreeFromJson, which assigns fresh uids in one place
// after the whole tree is built, rather than each recursive call
// generating its own (keeps uid assignment in a single, easy-to-find
// spot instead of scattered through the parser).
bool NodeFromJsonValue(const JsonValue& v, DesignNode& out, std::string& error) {
    if (v.kind != JsonValue::Kind::Object) {
        error = "se esperaba un objeto para el nodo";
        return false;
    }
    if (const JsonValue* type = v.Find("type")) out.type = ScalarToPropertyString(*type);
    if (const JsonValue* id = v.Find("id")) out.id = ScalarToPropertyString(*id);

    if (const JsonValue* props = v.Find("properties")) {
        if (props->kind == JsonValue::Kind::Object) {
            for (const auto& [key, val] : props->obj) {
                out.properties.push_back({key, ScalarToPropertyString(val)});
            }
        }
    }
    if (const JsonValue* events = v.Find("events")) {
        if (events->kind == JsonValue::Kind::Object) {
            for (const auto& [key, val] : events->obj) {
                out.events.push_back({key, ScalarToPropertyString(val)});
            }
        }
    }
    if (const JsonValue* children = v.Find("children")) {
        if (children->kind == JsonValue::Kind::Array) {
            for (const auto& child_val : children->arr) {
                DesignNode child;
                if (!NodeFromJsonValue(child_val, child, error)) return false;
                out.children.push_back(std::move(child));
            }
        }
    }
    return true;
}

// Walks a freshly-parsed tree and assigns a fresh node_uid to every
// node -- see DesignNode::node_uid, uids are never part of the file.
void AssignFreshUids(DesignNode& node) {
    node.node_uid = GenerateNodeUid();
    for (DesignNode& child : node.children) AssignFreshUids(child);
}

} // namespace

std::string ComponentTreeToJson(const DesignNode& root) {
    std::ostringstream out;
    WriteNode(out, root, 0);
    return out.str();
}

bool ComponentTreeFromJson(const std::string& json, DesignNode& out_root, std::string& out_error) {
    JsonParser parser(json);
    JsonValue root_value = parser.Parse();
    if (!parser.Error().empty()) {
        out_error = parser.Error();
        return false;
    }
    DesignNode node;
    if (!NodeFromJsonValue(root_value, node, out_error)) return false;
    AssignFreshUids(node);
    out_root = std::move(node);
    return true;
}

std::string WriteAvauiText(const DesignNode& root, const std::string& code_behind) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"design\": ";
    // Re-indent WriteNode's output isn't worth the complexity here --
    // it's already produced at depth 0, which reads fine nested one
    // level in under "design" too (this is a JSON file meant to be
    // diffed/read by a human occasionally, not pretty-printed to a
    // strict style guide).
    {
        std::ostringstream design_out;
        WriteNode(design_out, root, 1);
        out << design_out.str();
    }
    out << ",\n  \"code\": ";
    WriteJsonString(out, code_behind);
    out << "\n}\n";
    return out.str();
}

bool ParseAvauiText(const std::string& text, DesignNode& out_root, std::string& out_code_behind,
                     std::string& out_error) {
    JsonParser parser(text);
    JsonValue root_value = parser.Parse();
    if (!parser.Error().empty()) {
        out_error = parser.Error();
        return false;
    }
    if (root_value.kind != JsonValue::Kind::Object) {
        out_error = "el archivo .avaui no es un objeto JSON";
        return false;
    }
    const JsonValue* design = root_value.Find("design");
    if (!design) {
        out_error = "falta la clave \"design\"";
        return false;
    }
    DesignNode node;
    if (!NodeFromJsonValue(*design, node, out_error)) return false;
    AssignFreshUids(node);

    std::string code;
    if (const JsonValue* code_val = root_value.Find("code")) {
        code = ScalarToPropertyString(*code_val);
    }

    out_root = std::move(node);
    out_code_behind = std::move(code);
    return true;
}

} // namespace studio::design
