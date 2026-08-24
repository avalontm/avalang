#include "vm/vm.h"
#include "vm/value.h"
#include "components/IComponent.h"
#include "components/ComponentTree.h"
#include "components/PropertyValue.h"
#include "parser/AvauiParser.h"
#include "parser/AvauiWriter.h"
#include "parser/AvauiPropertyCoercion.h"
#include "events/AutoBind.h"
#include "c_api_internal.h"

using namespace ava;

#include "avalang.h"

static int LayoutNameToId(const avastd::string& type) {
    if (type == "Column") return 1;
    if (type == "Row") return 2;
    if (type == "Stack") return 3;
    if (type == "Grid") return 4;
    if (type == "Flex") return 5;
    return 0;
}

static avalang::ui::PropertyValue ToPropertyValue(ava_value_t v) {
    switch (v.type) {
        case AVA_BOOL:   return avalang::ui::PropertyValue(v.as.b != 0);
        case AVA_NUMBER: return avalang::ui::PropertyValue(v.as.n);
        case AVA_STRING: {
            Value sv = FromC(v);
            if (sv.type == ValueType::String && sv.obj) {
                return avalang::ui::PropertyValue(static_cast<StringObj*>(sv.obj)->data);
            }
            return avalang::ui::PropertyValue(avastd::string());
        }
        default: {
            Value sv = FromC(v);
            if (sv.type == ValueType::String && sv.obj) {
                return avalang::ui::PropertyValue(static_cast<StringObj*>(sv.obj)->data);
            }
            return avalang::ui::PropertyValue(avastd::string());
        }
    }
}

static ava_value_t ToAvaValue(const avalang::ui::PropertyValue& pv) {
    switch (pv.Type()) {
        case avalang::ui::PropertyType::Bool:
            return ava_value_t{AVA_BOOL, {.b = pv.AsBool() ? 1 : 0}};
        case avalang::ui::PropertyType::Number:
            return ava_value_t{AVA_NUMBER, {.n = pv.AsNumber()}};
        case avalang::ui::PropertyType::String: {
            const avastd::string& s = pv.AsString();
            auto* so = new StringObj(s);
            Value v; v.type = ValueType::String; v.obj = so;
            // Same fix as ava_string_create/ava_list_create/ava_dict_create
            // in c_api.cpp (Fase 7 bugfix) and builtin_str/type/etc. in
            // builtins/: ToC() never retains, so a freshly `new`'d object
            // (refcount 1, owned only by this local `v`) gets deleted by
            // `v`'s destructor the instant this function returns, leaving
            // the handle already-returned dangling -- Retain() first keeps
            // the object alive past that point, for the caller's own copy.
            Retain(v);
            return ToC(v);
        }
        default:
            return ava_value_t{AVA_NIL, {0}};
    }
}

static avastd::string StateMapToJson(const avastd::unordered_map<avastd::string, avastd::string>& state) {
    avastd::ostringstream oss;
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

static avastd::string ImportsToJson(const avastd::vector<avastd::string>& imports) {
    avastd::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < imports.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << "\"" << imports[i] << "\"";
    }
    oss << "]";
    return oss.str();
}

static avastd::string RoutesToJson(const avastd::vector<avalang::ui::parser::RouteDeclaration>& routes) {
    avastd::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < routes.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << "{\"template\": \"" << routes[i].route_template << "\", \"parameters\": [";
        for (size_t j = 0; j < routes[i].parameters.size(); ++j) {
            if (j > 0) oss << ", ";
            oss << "{\"name\": \"" << routes[i].parameters[j].name << "\"";
            oss << ", \"optional\": " << (routes[i].parameters[j].kind == avalang::ui::parser::RouteParameterKind::Optional ? "true" : "false");
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

static void ComponentToJson(avastd::ostream& os, avalang::ui::IComponent* comp, int indent) {
    if (!comp) return;
    avastd::string pad(indent * 2, ' ');
    os << pad << "{\n";
    os << pad << "  \"type\": \"" << comp->TypeName() << "\"";
    const auto* idProp = comp->GetProperty("id");
    if (idProp && idProp->Type() == avalang::ui::PropertyType::String && !idProp->AsString().empty()) {
        os << ",\n" << pad << "  \"id\": \"" << idProp->AsString() << "\"";
    }
    int layout = 0;
    const auto* layoutProp = comp->GetProperty("__layout");
    if (layoutProp && layoutProp->Type() == avalang::ui::PropertyType::Number) {
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
                case avalang::ui::PropertyType::String: {
                    os << "\"";
                    for (char c : pv->AsString()) {
                        if (c == '"' || c == '\\') os << '\\';
                        os << c;
                    }
                    os << "\"";
                    break;
                }
                case avalang::ui::PropertyType::Number:
                    os << pv->AsNumber();
                    break;
                case avalang::ui::PropertyType::Bool:
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

extern "C" {

struct AvaComponent {
    avalang::ui::IComponent* comp;
    avastd::unique_ptr<avalang::ui::ComponentTree> owned_tree;

    explicit AvaComponent(avalang::ui::IComponent* c, avastd::unique_ptr<avalang::ui::ComponentTree> t = nullptr)
        : comp(c), owned_tree(avastd::move(t)) {}
};

struct AvaComponentTree {
    avastd::unique_ptr<avalang::ui::ComponentTree> tree;
    AvaComponentTree() : tree(avalang::ui::ComponentTree::Create()) {}
};

AVA_API AvaComponentTree* ava_ui_create_tree(void) {
    return new AvaComponentTree();
}

AVA_API void ava_ui_destroy_tree(AvaComponentTree* tree) {
    delete tree;
}

AVA_API AvaComponent* ava_ui_create_component(const char* type) {
    auto tree = avalang::ui::ComponentTree::Create();
    auto* comp = tree->CreateComponent(type ? type : "");
    return new AvaComponent(comp, avastd::move(tree));
}

AVA_API void ava_ui_destroy_component(AvaComponent* component) {
    delete component;
}

AVA_API void ava_ui_set_property(AvaComponent* comp, const char* key, ava_value_t value) {
    if (!comp || !key) return;
    comp->comp->SetProperty(key, ToPropertyValue(value));
}

AVA_API int ava_ui_has_property(AvaComponent* comp, const char* key) {
    if (!comp || !key) return 0;
    return comp->comp->HasProperty(key) ? 1 : 0;
}

AVA_API ava_value_t ava_ui_get_property(AvaComponent* comp, const char* key) {
    if (!comp || !key) return ava_value_t{AVA_NIL, {0}};
    const auto* pv = comp->comp->GetProperty(key);
    if (!pv) return ava_value_t{AVA_NIL, {0}};
    return ToAvaValue(*pv);
}

AVA_API void ava_ui_remove_property(AvaComponent* comp, const char* key) {
    if (!comp || !key) return;
    comp->comp->RemoveProperty(key);
}

AVA_API size_t ava_ui_property_count(AvaComponent* comp) {
    if (!comp) return 0;
    return comp->comp->PropertyNames().size();
}

AVA_API const char* ava_ui_property_key_at(AvaComponent* comp, size_t index) {
    if (!comp) return nullptr;
    auto names = comp->comp->PropertyNames();
    if (index >= names.size()) return nullptr;
    static thread_local avastd::string key_str;
    key_str = names[index];
    return key_str.c_str();
}

AVA_API void ava_ui_add_child(AvaComponent* parent, AvaComponent* child) {
    if (!parent || !child) return;
    parent->comp->AddChild(child->comp);
}

AVA_API void ava_ui_remove_child(AvaComponent* parent, AvaComponent* child) {
    if (!parent || !child) return;
    parent->comp->RemoveChild(child->comp);
}

AVA_API size_t ava_ui_child_count(AvaComponent* parent) {
    if (!parent) return 0;
    return parent->comp->Children().size();
}

AVA_API AvaComponent* ava_ui_get_child(AvaComponent* parent, size_t index) {
    if (!parent) return nullptr;
    auto children = parent->comp->Children();
    if (index >= children.size()) return nullptr;
    return new AvaComponent(children[index]);
}

AVA_API void ava_ui_set_event(AvaComponent* comp, const char* event, ava_value_t callback) {
    if (!comp || !event) return;
    comp->comp->SetProperty(event, ToPropertyValue(callback));
}

AVA_API int ava_ui_has_event(AvaComponent* comp, const char* event) {
    if (!comp || !event) return 0;
    if (!comp->comp->HasProperty(event)) return 0;
    return avalang::ui::IsEventPropertyName(event) ? 1 : 0;
}

AVA_API ava_value_t ava_ui_get_event(AvaComponent* comp, const char* event) {
    if (!comp || !event) return ava_value_t{AVA_NIL, {0}};
    const auto* pv = comp->comp->GetProperty(event);
    if (!pv) return ava_value_t{AVA_NIL, {0}};
    return ToAvaValue(*pv);
}

AVA_API size_t ava_ui_event_count(AvaComponent* comp) {
    if (!comp) return 0;
    size_t count = 0;
    for (const auto& name : comp->comp->PropertyNames()) {
        if (avalang::ui::IsEventPropertyName(name)) ++count;
    }
    return count;
}

AVA_API const char* ava_ui_event_key_at(AvaComponent* comp, size_t index) {
    if (!comp) return nullptr;
    avastd::vector<avastd::string> event_names;
    for (const auto& name : comp->comp->PropertyNames()) {
        if (avalang::ui::IsEventPropertyName(name)) {
            event_names.push_back(name);
        }
    }
    if (index >= event_names.size()) return nullptr;
    static thread_local avastd::string key_str;
    key_str = event_names[index];
    return key_str.c_str();
}

AVA_API void ava_ui_set_id(AvaComponent* comp, const char* id) {
    if (!comp) return;
    comp->comp->SetProperty("id", avalang::ui::PropertyValue(avastd::string(id ? id : "")));
}

AVA_API const char* ava_ui_get_id(AvaComponent* comp) {
    if (!comp) return nullptr;
    const auto* pv = comp->comp->GetProperty("id");
    if (!pv || pv->Type() != avalang::ui::PropertyType::String) return "";
    static thread_local avastd::string id_str;
    id_str = pv->AsString();
    return id_str.c_str();
}

AVA_API void ava_ui_set_layout(AvaComponent* comp, int layout) {
    if (!comp) return;
    comp->comp->SetProperty("__layout", avalang::ui::PropertyValue(static_cast<double>(layout)));
}

AVA_API int ava_ui_get_layout(AvaComponent* comp) {
    if (!comp) return 0;
    const auto* pv = comp->comp->GetProperty("__layout");
    if (!pv || pv->Type() != avalang::ui::PropertyType::Number) return 0;
    return static_cast<int>(pv->AsNumber());
}

AVA_API void ava_ui_set_root(AvaComponentTree* tree, AvaComponent* root) {
    if (!tree || !root) return;
    tree->tree->SetRoot(root->comp);
}

AVA_API AvaComponent* ava_ui_get_root(AvaComponentTree* tree) {
    if (!tree) return nullptr;
    auto* root = tree->tree->Root();
    if (!root) return nullptr;
    return new AvaComponent(root);
}

AVA_API const char* ava_ui_get_component_type(AvaComponent* comp) {
    if (!comp) return nullptr;
    static thread_local avastd::string type_str;
    type_str = comp->comp->TypeName();
    return type_str.c_str();
}

AVA_API const char* ava_ui_tree_to_json(AvaComponentTree* tree) {
    if (!tree) return "";
    static thread_local avastd::ostringstream oss;
    oss.str(""); oss.clear();
    auto* root = tree->tree->Root();
    if (root) {
        ComponentToJson(oss, root, 0);
    }
    static thread_local avastd::string result;
    result = oss.str();
    return result.c_str();
}

AVA_API void ava_ui_json_free(char* json) {
    (void)json;
}

AVA_API AvaComponentTree* ava_ui_parse_avaui_text(
    const char* text,
    char** out_state_json,
    char** out_imports_json,
    char** out_methods_text,
    char** out_error,
    char** out_extends,
    char** out_routes_json
) {
    auto* result = new AvaComponentTree();

#if AVA_HAVE_EXCEPTIONS
    try {
        auto parsed = avalang::ui::parser::AvauiParser::Parse(text ? text : "");
        result->tree = avastd::move(parsed.tree);

        if (out_state_json) *out_state_json = DupString(StateMapToJson(parsed.state));
        if (out_imports_json) *out_imports_json = DupString(ImportsToJson(parsed.imports));
        if (out_methods_text) *out_methods_text = DupString(parsed.code);
        if (out_error) *out_error = DupString("");
        if (out_extends) *out_extends = DupString(parsed.extends);
        if (out_routes_json) *out_routes_json = DupString(RoutesToJson(parsed.routes));
    } catch (const avastd::exception& e) {
        if (out_error) *out_error = DupString(e.what());
    } catch (...) {
        if (out_error) *out_error = DupString("unknown parse error");
    }
#else
    AVA_TRY {
        auto parsed = avalang::ui::parser::AvauiParser::Parse(text ? text : "");
        result->tree = avastd::move(parsed.tree);

        if (out_state_json) *out_state_json = DupString(StateMapToJson(parsed.state));
        if (out_imports_json) *out_imports_json = DupString(ImportsToJson(parsed.imports));
        if (out_methods_text) *out_methods_text = DupString(parsed.code);
        if (out_error) *out_error = DupString("");
        if (out_extends) *out_extends = DupString(parsed.extends);
        if (out_routes_json) *out_routes_json = DupString(RoutesToJson(parsed.routes));
    } AVA_CATCH(avastd::exception, e) {
        if (out_error) *out_error = DupString(e.what());
    }
#endif

    return result;
}

AVA_API char* ava_ui_write_avaui_text(
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

    avalang::ui::parser::AvauiWriteOptions opts;
    opts.code_behind = methods_text ? methods_text : "";
    opts.extends = extends ? extends : "";

    if (state_json && *state_json) {
        avastd::istringstream ss(state_json);
        avastd::string line;
        while (avastd::getline(ss, line)) {
            auto eq = line.find('=');
            if (eq == avastd::string::npos) continue;
            avastd::string key = line.substr(0, eq);
            avastd::string val = line.substr(eq + 1);
            if (!key.empty() && !val.empty()) {
                opts.initial_state.push_back({key, val});
            }
        }
    }

    if (imports_json && *imports_json) {
        avastd::istringstream ss(imports_json);
        avastd::string line;
        while (avastd::getline(ss, line)) {
            avastd::string trimmed = line;
            trimmed.erase(0, trimmed.find_first_not_of(" \t[]\""));
            trimmed.erase(trimmed.find_last_not_of(" \t\"]") + 1);
            if (!trimmed.empty()) {
                opts.imports.push_back(trimmed);
            }
        }
    }

    if (routes_json && *routes_json) {
        avastd::istringstream ss(routes_json);
        avastd::string line;
        while (avastd::getline(ss, line)) {
            auto tplStart = line.find("\"template\"");
            if (tplStart == avastd::string::npos) continue;
            auto colon = line.find(":", tplStart);
            if (colon == avastd::string::npos) continue;
            auto q1 = line.find("\"", colon);
            if (q1 == avastd::string::npos) continue;
            auto q2 = line.find("\"", q1 + 1);
            if (q2 == avastd::string::npos) continue;
            avastd::string tpl = line.substr(q1 + 1, q2 - q1 - 1);
            if (!tpl.empty()) {
                opts.routes.push_back({tpl});
            }
        }
    }

    return DupString(avalang::ui::parser::WriteAvaui(root, opts));
}

AVA_API void ava_ui_text_free(char* text) {
    std::free(text);
}

}
