#include "design/avaui_text.h"

#include <algorithm>
#include <cctype>
#include <sstream>

#include "avalang.h"

namespace studio::design {

namespace {

// ---------------------------------------------------------------------
// This file used to carry its own hand-ported copy of the .avaui
// grammar (indentation splitter, block parser, property-value quoting,
// etc. -- a C++ port of AvaComponentParser.cs). That grammar now lives
// once, in core/src/ui/avaui_text.{h,cpp} (compiled into avalang.dll),
// exposed here as ava_ui_parse_avaui_text/ava_ui_write_avaui_text --
// see docs/architecture/08_DESIGNER_VIEW_PLAN.md section 9.2/9.3 and
// avalang.h's own comment above those two functions for the reasoning.
// This file is now just a thin adapter: convert an AvaComponentTree
// (avalang.h's C API model) to/from this Designer's own DesignNode
// tree, and convert the state/imports side-channels to/from the flat
// JSON shapes ava_ui_parse_avaui_text/ava_ui_write_avaui_text use.
// ---------------------------------------------------------------------

// Event-prop names stay here (not delegated) since IsEventPropertyName
// is part of this file's own public API -- used by
// properties_panel.cpp/designer_canvas.cpp to tell an event binding
// apart from a style property on a DesignNode already in memory, which
// has nothing to do with text parsing. Must stay in sync with the
// core parser's own list (core/src/ui/avaui_text.cpp) and the .NET
// reference's AvaComponentParser.cs::EventProps.
const std::vector<std::string>& EventPropertyNames() {
    // Kept identical to the core parser's own EventPropNames()
    // (core/src/ui/avaui_text.cpp) -- these two lists had drifted
    // apart (this one was missing onmouseenter/onmouseleave/onload/
    // onerror and had a stray "onhover", which isn't a real DOM event
    // name -- onmouseenter/onmouseleave are the correct pair).
    static const std::vector<std::string> names = {
        "click", "onclick", "onchange", "oninput", "onfocus", "onblur",
        "onkeydown", "onkeyup", "onmouseenter", "onmouseleave",
        "onsubmit", "onload", "onerror",
    };
    return names;
}

std::string LowerCopy(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

// --- Minimal flat-JSON encode/decode for the two side-channels ------
// ava_ui_parse_avaui_text/ava_ui_write_avaui_text exchange `state` and
// `imports` as flat JSON (a {"key":"value",...} object and a
// ["a","b",...] array of strings -- see avalang.h's comment on those
// two functions). Both shapes are simple enough that a small dedicated
// encoder/decoder here is clearer than pulling in a general JSON
// library for two flat cases; escaping matches the core's own
// AppendJsonString/ParseJsonStringAt exactly (\" \\ \n \r \t) so this
// round-trips losslessly through avalang.dll.

void AppendJsonString(std::ostringstream& out, const std::string& s) {
    out << '"';
    for (char c : s) {
        switch (c) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default: out << c; break;
        }
    }
    out << '"';
}

std::string ParseJsonStringAt(const std::string& text, size_t& i) {
    std::string out;
    if (i >= text.size() || text[i] != '"') return out;
    ++i; // opening quote
    while (i < text.size() && text[i] != '"') {
        if (text[i] == '\\' && i + 1 < text.size()) {
            char next = text[i + 1];
            switch (next) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                default: out.push_back(next); break;
            }
            i += 2;
        } else {
            out.push_back(text[i]);
            ++i;
        }
    }
    if (i < text.size() && text[i] == '"') ++i; // closing quote
    return out;
}

std::string StateToJson(const std::vector<PropertyRow>& state) {
    std::ostringstream out;
    out << "{";
    bool first = true;
    for (const auto& row : state) {
        if (!first) out << ",";
        first = false;
        AppendJsonString(out, row.key);
        out << ":";
        AppendJsonString(out, row.value);
    }
    out << "}";
    return out.str();
}

std::vector<PropertyRow> StateFromJson(const std::string& json) {
    std::vector<PropertyRow> result;
    size_t i = 0;
    while (i < json.size() && json[i] != '{') ++i;
    if (i >= json.size()) return result;
    ++i; // '{'
    while (i < json.size()) {
        while (i < json.size() && (json[i] == ',' || std::isspace(static_cast<unsigned char>(json[i])))) ++i;
        if (i >= json.size() || json[i] == '}') break;
        std::string key = ParseJsonStringAt(json, i);
        while (i < json.size() && (json[i] == ':' || std::isspace(static_cast<unsigned char>(json[i])))) ++i;
        std::string value = ParseJsonStringAt(json, i);
        result.push_back(PropertyRow{std::move(key), std::move(value)});
    }
    return result;
}

std::string ImportsToJson(const std::vector<std::string>& imports) {
    std::ostringstream out;
    out << "[";
    bool first = true;
    for (const auto& imp : imports) {
        if (!first) out << ",";
        first = false;
        AppendJsonString(out, imp);
    }
    out << "]";
    return out.str();
}

std::vector<std::string> ImportsFromJson(const std::string& json) {
    std::vector<std::string> result;
    size_t i = 0;
    while (i < json.size() && json[i] != '[') ++i;
    if (i >= json.size()) return result;
    ++i; // '['
    while (i < json.size()) {
        while (i < json.size() && (json[i] == ',' || std::isspace(static_cast<unsigned char>(json[i])))) ++i;
        if (i >= json.size() || json[i] == ']') break;
        result.push_back(ParseJsonStringAt(json, i));
    }
    return result;
}

// --- ava_value_t <-> std::string, string-only -----------------------
// Every property/event value the core parser produces is a
// Value::String holding display-ready text (numbers/bools/expressions
// included, verbatim as written -- see core/src/ui/avaui_text.h's
// header comment). ava_string_create/ava_string_data's AvaVM*
// parameter is accepted but unused by the C API (see c_api.cpp) --
// passing nullptr is safe and avoids requiring a live VM just to
// shuttle property strings through the C boundary.

ava_value_t MakeStringValue(const std::string& s) {
    return ava_string_create(nullptr, s.c_str(), s.size());
}

std::string ValueToString(ava_value_t v) {
    if (v.type != AVA_STRING) return "";
    size_t len = 0;
    const char* data = ava_string_data(nullptr, v, &len);
    if (!data) return "";
    return std::string(data, len);
}

// --- AvaComponent (C API) -> DesignNode ------------------------------

DesignNode ComponentToDesignNode(AvaComponent* comp) {
    DesignNode node;
    node.node_uid = GenerateNodeUid();
    if (!comp) return node;

    const char* type = ava_ui_get_component_type(comp);
    node.type = type ? type : "";
    const char* id = ava_ui_get_id(comp);
    node.id = id ? id : "";

    size_t prop_count = ava_ui_property_count(comp);
    for (size_t i = 0; i < prop_count; ++i) {
        const char* key = ava_ui_property_key_at(comp, i);
        if (!key) continue;
        std::string key_str = key; // copy -- ava_ui_get_property below reuses the same thread-local buffer style
        ava_value_t value = ava_ui_get_property(comp, key_str.c_str());
        node.properties.push_back(PropertyRow{key_str, ValueToString(value)});
    }

    size_t event_count = ava_ui_event_count(comp);
    for (size_t i = 0; i < event_count; ++i) {
        const char* key = ava_ui_event_key_at(comp, i);
        if (!key) continue;
        std::string key_str = key;
        ava_value_t value = ava_ui_get_event(comp, key_str.c_str());
        node.events.push_back(PropertyRow{key_str, ValueToString(value)});
    }

    size_t child_count = ava_ui_child_count(comp);
    for (size_t i = 0; i < child_count; ++i) {
        AvaComponent* child = ava_ui_get_child(comp, i);
        if (!child) continue;
        node.children.push_back(ComponentToDesignNode(child));
        // ava_ui_get_child hands back a fresh wrapper around the
        // child's shared_ptr (see c_api.cpp) -- the tree keeps its own
        // reference, so destroying this wrapper here just frees the
        // wrapper, not the underlying component.
        ava_ui_destroy_component(child);
    }

    return node;
}

// --- DesignNode -> AvaComponent (C API), building a fresh tree -------

AvaComponent* DesignNodeToComponent(const DesignNode& node) {
    AvaComponent* comp = ava_ui_create_component(node.type.c_str());
    if (!node.id.empty()) ava_ui_set_id(comp, node.id.c_str());
    for (const auto& prop : node.properties) {
        ava_ui_set_property(comp, prop.key.c_str(), MakeStringValue(prop.value));
    }
    for (const auto& ev : node.events) {
        ava_ui_set_event(comp, ev.key.c_str(), MakeStringValue(ev.value));
    }
    for (const auto& child : node.children) {
        AvaComponent* child_comp = DesignNodeToComponent(child);
        ava_ui_add_child(comp, child_comp);
        // AddChild copies its own shared_ptr into the parent's
        // children list (see component.h), so the wrapper can go away
        // right after.
        ava_ui_destroy_component(child_comp);
    }
    return comp;
}

} // namespace

bool IsEventPropertyName(const std::string& name) {
    std::string lower = LowerCopy(name);
    for (const auto& n : EventPropertyNames()) {
        if (n == lower) return true;
    }
    return false;
}

std::string WriteAvauiText(const DesignNode& root, const std::string& code_behind,
                            const std::vector<PropertyRow>& initial_state,
                            const std::vector<std::string>& imports) {
    AvaComponentTree* tree = ava_ui_create_tree();
    AvaComponent* root_comp = DesignNodeToComponent(root);
    ava_ui_set_root(tree, root_comp);
    ava_ui_destroy_component(root_comp); // SetRoot retains its own shared_ptr, same as AddChild

    std::string state_json = StateToJson(initial_state);
    std::string imports_json = ImportsToJson(imports);

    char* text = ava_ui_write_avaui_text(tree, state_json.c_str(), imports_json.c_str(), code_behind.c_str(),
                                          /*extends=*/nullptr, /*routes_json=*/nullptr);
    std::string result = text ? text : "";
    ava_ui_text_free(text);
    ava_ui_destroy_tree(tree);
    return result;
}

bool ParseAvauiText(const std::string& text, DesignNode& out_root, std::string& out_code_behind,
                     std::vector<PropertyRow>& out_initial_state, std::vector<std::string>& out_imports,
                     std::string& out_error) {
    char* state_json = nullptr;
    char* imports_json = nullptr;
    char* methods_text = nullptr;
    char* error = nullptr;

    AvaComponentTree* tree =
        ava_ui_parse_avaui_text(text.c_str(), &state_json, &imports_json, &methods_text, &error,
                                 /*out_extends=*/nullptr, /*out_routes_json=*/nullptr);

    out_error = error ? error : "";
    out_code_behind = methods_text ? methods_text : "";
    out_initial_state = StateFromJson(state_json ? state_json : "{}");
    out_imports = ImportsFromJson(imports_json ? imports_json : "[]");

    if (tree) {
        AvaComponent* root = ava_ui_get_root(tree);
        if (root) {
            out_root = ComponentToDesignNode(root);
            ava_ui_destroy_component(root);
        } else {
            out_root = DesignNode{};
            out_root.node_uid = GenerateNodeUid();
        }
    } else {
        out_root = DesignNode{};
        out_root.node_uid = GenerateNodeUid();
    }

    ava_ui_text_free(state_json);
    ava_ui_text_free(imports_json);
    ava_ui_text_free(methods_text);
    ava_ui_text_free(error);
    if (tree) ava_ui_destroy_tree(tree);

    return true; // forgiving parser, same contract as before -- see avaui_text.h
}

} // namespace studio::design
