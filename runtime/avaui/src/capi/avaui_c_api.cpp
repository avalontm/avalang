#include "avaui_c_api.h"

#include "components/IComponent.h"
#include "components/ComponentTree.h"
#include "components/PropertyValue.h"
#include "parser/AvauiParser.h"
#include "parser/AvauiWriter.h"
#include "parser/AvauiPropertyCoercion.h"
#include "events/AutoBind.h"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace avalang::ui;

namespace {

char* DupString(const std::string& s) {
    char* out = static_cast<char*>(std::malloc(s.size() + 1));
    std::memcpy(out, s.c_str(), s.size() + 1);
    return out;
}

int LayoutNameToId(const std::string& type) {
    if (type == "Column") return 1;
    if (type == "Row") return 2;
    if (type == "Stack") return 3;
    if (type == "Grid") return 4;
    if (type == "Flex") return 5;
    return 0;
}

PropertyValue ToPropertyValue(AvaVM* vm, ava_value_t v) {
    switch (v.type) {
        case AVA_BOOL:
            return PropertyValue(v.as.b != 0);
        case AVA_NUMBER:
            return PropertyValue(v.as.n);
        case AVA_STRING: {
            size_t len = 0;
            const char* data = ava_string_data(vm, v, &len);
            return PropertyValue(std::string(data ? data : "", len));
        }
        default:
            return PropertyValue(std::string());
    }
}

ava_value_t ToAvaValue(AvaVM* vm, const PropertyValue& pv) {
    switch (pv.Type()) {
        case PropertyType::Bool:
            return ava_value_t{AVA_BOOL, {.b = pv.AsBool() ? 1 : 0}};
        case PropertyType::Number:
            return ava_value_t{AVA_NUMBER, {.n = pv.AsNumber()}};
        case PropertyType::String: {
            const std::string& s = pv.AsString();
            return ava_string_create(vm, s.c_str(), s.size());
        }
        default:
            return ava_value_t{AVA_NIL, {0}};
    }
}

std::string StateMapToJson(const std::unordered_map<std::string, std::string>& state) {
    std::ostringstream oss;
    oss << "{";
    bool first = true;
    for (const auto& [k, v] : state) {
        if (!first) oss << ", ";
        oss << "\"" << k << "\": \"" << v << "\"";
        first = false;
    }
    oss << "}";
    return oss.str();
}

std::string ImportsToJson(const std::vector<std::string>& imports) {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < imports.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << "\"" << imports[i] << "\"";
    }
    oss << "]";
    return oss.str();
}

std::string RoutesToJson(const std::vector<parser::RouteDeclaration>& routes) {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < routes.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << "{\"template\": \"" << routes[i].route_template << "\", \"parameters\": [";
        for (size_t j = 0; j < routes[i].parameters.size(); ++j) {
            if (j > 0) oss << ", ";
            oss << "{\"name\": \"" << routes[i].parameters[j].name << "\"";
            oss << ", \"optional\": " << (routes[i].parameters[j].kind == parser::RouteParameterKind::Optional ? "true" : "false");
            if (!routes[i].parameters[j].constraint.empty()) {
                oss << ", \"constraint\": \"" << routes[i].parameters[j].constraint << "\"";
            }
            oss << "}";
        }
        oss << "]}";
    }
    oss << "]";
    return oss.str();
}

void ComponentToJson(std::ostream& os, IComponent* comp, int indent) {
    if (!comp) return;
    std::string pad(indent * 2, ' ');
    os << pad << "{\n";
    os << pad << "  \"type\": \"" << comp->TypeName() << "\"";
    const auto* idProp = comp->GetProperty("id");
    if (idProp && idProp->Type() == PropertyType::String && !idProp->AsString().empty()) {
        os << ",\n" << pad << "  \"id\": \"" << idProp->AsString() << "\"";
    }
    int layout = 0;
    const auto* layoutProp = comp->GetProperty("__layout");
    if (layoutProp && layoutProp->Type() == PropertyType::Number) {
        layout = static_cast<int>(layoutProp->AsNumber());
    } else {
        layout = LayoutNameToId(comp->TypeName());
    }
    os << ",\n" << pad << "  \"layout\": " << layout;
    auto names = comp->PropertyNames();
    bool has_props = false;
    for (const auto& name : names) {
        if (name == "id" || name == "__layout") continue;
        has_props = true;
        break;
    }
    if (has_props) {
        os << ",\n" << pad << "  \"properties\": {";
        bool first = true;
        for (const auto& name : names) {
            if (name == "id" || name == "__layout") continue;
            const auto* pv = comp->GetProperty(name);
            if (!pv) continue;
            if (!first) os << ", ";
            os << "\"" << name << "\": ";
            switch (pv->Type()) {
                case PropertyType::String: {
                    os << "\"";
                    for (char c : pv->AsString()) {
                        if (c == '"' || c == '\\') os << '\\';
                        os << c;
                    }
                    os << "\"";
                    break;
                }
                case PropertyType::Number:
                    os << pv->AsNumber();
                    break;
                case PropertyType::Bool:
                    os << (pv->AsBool() ? "true" : "false");
                    break;
                default:
                    os << "null";
                    break;
            }
            first = false;
        }
        os << "}";
    }
    auto children = comp->Children();
    if (!children.empty()) {
        os << ",\n" << pad << "  \"children\": [\n";
        for (size_t i = 0; i < children.size(); ++i) {
            ComponentToJson(os, children[i], indent + 2);
            if (i < children.size() - 1) os << ",";
            os << "\n";
        }
        os << pad << "  ]";
    }
    os << "\n" << pad << "}";
}

struct StoredCallback {
    AvaVM* vm;
    ava_value_t value;
};

std::unordered_map<IComponent*, std::unordered_map<std::string, StoredCallback>>& EventFunctions() {
    static std::unordered_map<IComponent*, std::unordered_map<std::string, StoredCallback>> table;
    return table;
}

void ReleaseStoredEvent(IComponent* comp, const std::string& event) {
    auto compIt = EventFunctions().find(comp);
    if (compIt == EventFunctions().end()) return;
    auto evIt = compIt->second.find(event);
    if (evIt == compIt->second.end()) return;
    ava_value_release(evIt->second.vm, evIt->second.value);
    compIt->second.erase(evIt);
    if (compIt->second.empty()) EventFunctions().erase(compIt);
}

}

extern "C" {

struct AvaComponent {
    IComponent* comp;
    std::unique_ptr<ComponentTree> owned_tree;

    explicit AvaComponent(IComponent* c, std::unique_ptr<ComponentTree> t = nullptr)
        : comp(c), owned_tree(std::move(t)) {}
};

struct AvaComponentTree {
    std::unique_ptr<ComponentTree> tree;
    AvaComponentTree() : tree(ComponentTree::Create()) {}
};

AVAUI_API AvaComponentTree* ava_ui_create_tree(void) {
    return new AvaComponentTree();
}

AVAUI_API void ava_ui_destroy_tree(AvaComponentTree* tree) {
    delete tree;
}

AVAUI_API AvaComponent* ava_ui_create_component(const char* type) {
    auto tree = ComponentTree::Create();
    auto* comp = tree->CreateComponent(type ? type : "");
    return new AvaComponent(comp, std::move(tree));
}

AVAUI_API void ava_ui_destroy_component(AvaComponent* component) {
    if (!component) return;
    auto compIt = EventFunctions().find(component->comp);
    if (compIt != EventFunctions().end()) {
        for (auto& [event, stored] : compIt->second) {
            ava_value_release(stored.vm, stored.value);
        }
        EventFunctions().erase(compIt);
    }
    delete component;
}

AVAUI_API void ava_ui_set_property(AvaVM* vm, AvaComponent* comp, const char* key, ava_value_t value) {
    if (!comp || !key) return;
    comp->comp->SetProperty(key, ToPropertyValue(vm, value));
}

AVAUI_API int ava_ui_has_property(AvaComponent* comp, const char* key) {
    if (!comp || !key) return 0;
    return comp->comp->HasProperty(key) ? 1 : 0;
}

AVAUI_API ava_value_t ava_ui_get_property(AvaVM* vm, AvaComponent* comp, const char* key) {
    if (!comp || !key) return ava_value_t{AVA_NIL, {0}};
    const auto* pv = comp->comp->GetProperty(key);
    if (!pv) return ava_value_t{AVA_NIL, {0}};
    return ToAvaValue(vm, *pv);
}

AVAUI_API void ava_ui_remove_property(AvaComponent* comp, const char* key) {
    if (!comp || !key) return;
    ReleaseStoredEvent(comp->comp, key);
    comp->comp->RemoveProperty(key);
}

AVAUI_API size_t ava_ui_property_count(AvaComponent* comp) {
    if (!comp) return 0;
    return comp->comp->PropertyNames().size();
}

AVAUI_API const char* ava_ui_property_key_at(AvaComponent* comp, size_t index) {
    if (!comp) return nullptr;
    auto names = comp->comp->PropertyNames();
    if (index >= names.size()) return nullptr;
    static thread_local std::string key_str;
    key_str = names[index];
    return key_str.c_str();
}

AVAUI_API void ava_ui_add_child(AvaComponent* parent, AvaComponent* child) {
    if (!parent || !child) return;
    parent->comp->AddChild(child->comp);
}

AVAUI_API void ava_ui_remove_child(AvaComponent* parent, AvaComponent* child) {
    if (!parent || !child) return;
    parent->comp->RemoveChild(child->comp);
}

AVAUI_API size_t ava_ui_child_count(AvaComponent* parent) {
    if (!parent) return 0;
    return parent->comp->Children().size();
}

AVAUI_API AvaComponent* ava_ui_get_child(AvaComponent* parent, size_t index) {
    if (!parent) return nullptr;
    auto children = parent->comp->Children();
    if (index >= children.size()) return nullptr;
    return new AvaComponent(children[index]);
}

AVAUI_API void ava_ui_set_event(AvaVM* vm, AvaComponent* comp, const char* event, ava_value_t callback) {
    if (!comp || !event) return;
    ReleaseStoredEvent(comp->comp, event);
    if (callback.type == AVA_FUNCTION) {
        ava_value_retain(vm, callback);
        EventFunctions()[comp->comp][event] = StoredCallback{vm, callback};
    }
    comp->comp->SetProperty(event, ToPropertyValue(vm, callback));
}

AVAUI_API int ava_ui_has_event(AvaComponent* comp, const char* event) {
    if (!comp || !event) return 0;
    if (!comp->comp->HasProperty(event)) return 0;
    return IsEventPropertyName(event) ? 1 : 0;
}

AVAUI_API ava_value_t ava_ui_get_event(AvaVM* vm, AvaComponent* comp, const char* event) {
    if (!comp || !event) return ava_value_t{AVA_NIL, {0}};
    auto compIt = EventFunctions().find(comp->comp);
    if (compIt != EventFunctions().end()) {
        auto evIt = compIt->second.find(event);
        if (evIt != compIt->second.end()) {
            return evIt->second.value;
        }
    }
    const auto* pv = comp->comp->GetProperty(event);
    if (!pv) return ava_value_t{AVA_NIL, {0}};
    return ToAvaValue(vm, *pv);
}

AVAUI_API size_t ava_ui_event_count(AvaComponent* comp) {
    if (!comp) return 0;
    size_t count = 0;
    for (const auto& name : comp->comp->PropertyNames()) {
        if (IsEventPropertyName(name)) ++count;
    }
    return count;
}

AVAUI_API const char* ava_ui_event_key_at(AvaComponent* comp, size_t index) {
    if (!comp) return nullptr;
    std::vector<std::string> event_names;
    for (const auto& name : comp->comp->PropertyNames()) {
        if (IsEventPropertyName(name)) {
            event_names.push_back(name);
        }
    }
    if (index >= event_names.size()) return nullptr;
    static thread_local std::string key_str;
    key_str = event_names[index];
    return key_str.c_str();
}

AVAUI_API void ava_ui_set_id(AvaComponent* comp, const char* id) {
    if (!comp) return;
    comp->comp->SetProperty("id", PropertyValue(std::string(id ? id : "")));
}

AVAUI_API const char* ava_ui_get_id(AvaComponent* comp) {
    if (!comp) return nullptr;
    const auto* pv = comp->comp->GetProperty("id");
    if (!pv || pv->Type() != PropertyType::String) return "";
    static thread_local std::string id_str;
    id_str = pv->AsString();
    return id_str.c_str();
}

AVAUI_API void ava_ui_set_layout(AvaComponent* comp, int layout) {
    if (!comp) return;
    comp->comp->SetProperty("__layout", PropertyValue(static_cast<double>(layout)));
}

AVAUI_API int ava_ui_get_layout(AvaComponent* comp) {
    if (!comp) return 0;
    const auto* pv = comp->comp->GetProperty("__layout");
    if (!pv || pv->Type() != PropertyType::Number) return 0;
    return static_cast<int>(pv->AsNumber());
}

AVAUI_API void ava_ui_set_root(AvaComponentTree* tree, AvaComponent* root) {
    if (!tree || !root) return;
    tree->tree->SetRoot(root->comp);
}

AVAUI_API AvaComponent* ava_ui_get_root(AvaComponentTree* tree) {
    if (!tree) return nullptr;
    auto* root = tree->tree->Root();
    if (!root) return nullptr;
    return new AvaComponent(root);
}

AVAUI_API const char* ava_ui_get_component_type(AvaComponent* comp) {
    if (!comp) return nullptr;
    static thread_local std::string type_str;
    type_str = comp->comp->TypeName();
    return type_str.c_str();
}

AVAUI_API const char* ava_ui_tree_to_json(AvaComponentTree* tree) {
    if (!tree) return "";
    static thread_local std::ostringstream oss;
    oss.str(""); oss.clear();
    auto* root = tree->tree->Root();
    if (root) {
        ComponentToJson(oss, root, 0);
    }
    static thread_local std::string result;
    result = oss.str();
    return result.c_str();
}

AVAUI_API void ava_ui_json_free(char* json) {
    (void)json;
}

AVAUI_API AvaComponentTree* ava_ui_parse_avaui_text(
    const char* text,
    char** out_state_json,
    char** out_imports_json,
    char** out_methods_text,
    char** out_error,
    char** out_extends,
    char** out_routes_json
) {
    auto* result = new AvaComponentTree();

    try {
        auto parsed = parser::AvauiParser::Parse(text ? text : "");
        result->tree = std::move(parsed.tree);

        if (out_state_json) *out_state_json = DupString(StateMapToJson(parsed.state));
        if (out_imports_json) *out_imports_json = DupString(ImportsToJson(parsed.imports));
        if (out_methods_text) *out_methods_text = DupString(parsed.code);
        if (out_error) *out_error = DupString("");
        if (out_extends) *out_extends = DupString(parsed.extends);
        if (out_routes_json) *out_routes_json = DupString(RoutesToJson(parsed.routes));
    } catch (const std::exception& e) {
        if (out_error) *out_error = DupString(e.what());
    } catch (...) {
        if (out_error) *out_error = DupString("unknown parse error");
    }

    return result;
}

AVAUI_API char* ava_ui_write_avaui_text(
    AvaComponentTree* tree,
    const char* state_json,
    const char* imports_json,
    const char* methods_text,
    const char* extends,
    const char* routes_json
) {
    if (!tree) return DupString("");
    auto* root = tree->tree->Root();
    if (!root) return DupString("");

    parser::AvauiWriteOptions opts;
    opts.code_behind = methods_text ? methods_text : "";
    opts.extends = extends ? extends : "";

    if (state_json && *state_json) {
        std::istringstream ss(state_json);
        std::string line;
        while (std::getline(ss, line)) {
            auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string key = line.substr(0, eq);
            std::string val = line.substr(eq + 1);
            if (!key.empty() && !val.empty()) {
                opts.initial_state.push_back({key, val});
            }
        }
    }

    if (imports_json && *imports_json) {
        std::istringstream ss(imports_json);
        std::string line;
        while (std::getline(ss, line)) {
            std::string trimmed = line;
            trimmed.erase(0, trimmed.find_first_not_of(" \t[]\""));
            trimmed.erase(trimmed.find_last_not_of(" \t\"]") + 1);
            if (!trimmed.empty()) {
                opts.imports.push_back(trimmed);
            }
        }
    }

    if (routes_json && *routes_json) {
        std::istringstream ss(routes_json);
        std::string line;
        while (std::getline(ss, line)) {
            auto tplStart = line.find("\"template\"");
            if (tplStart == std::string::npos) continue;
            auto colon = line.find(":", tplStart);
            if (colon == std::string::npos) continue;
            auto q1 = line.find("\"", colon);
            if (q1 == std::string::npos) continue;
            auto q2 = line.find("\"", q1 + 1);
            if (q2 == std::string::npos) continue;
            std::string tpl = line.substr(q1 + 1, q2 - q1 - 1);
            if (!tpl.empty()) {
                opts.routes.push_back({tpl});
            }
        }
    }

    return DupString(parser::WriteAvaui(root, opts));
}

AVAUI_API void ava_ui_text_free(char* text) {
    std::free(text);
}

}
