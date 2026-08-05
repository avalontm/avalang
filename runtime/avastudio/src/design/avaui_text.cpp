#include "design/avaui_text.h"

#include <cctype>
#include <sstream>

#include "avalang.h"
#include "events/AutoBind.h"
#include "components/IComponent.h"
#include "components/PropertyValue.h"

namespace studio::design {

namespace {

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

ava_value_t MakeStringValue(const std::string& s) {
    return ava_string_create(nullptr, s.c_str(), s.size());
}

std::string PropValueToString(const avalang::ui::PropertyValue& pv) {
    switch (pv.Type()) {
        case avalang::ui::PropertyType::Bool: return pv.AsBool() ? "true" : "false";
        case avalang::ui::PropertyType::Number: {
            double n = pv.AsNumber();
            if (n == static_cast<long long>(n)) return std::to_string(static_cast<long long>(n));
            return std::to_string(n);
        }
        case avalang::ui::PropertyType::String: return pv.AsString();
        default: return "";
    }
}

void ComponentToAvaComponent(const avalang::ui::IComponent* src, AvaComponent* dst) {
    if (!src || !dst) return;
    if (const auto* idProp = src->GetProperty("id")) {
        if (idProp->Type() == avalang::ui::PropertyType::String) {
            ava_ui_set_id(dst, idProp->AsString().c_str());
        }
    }
    for (const auto& key : src->PropertyNames()) {
        if (key == "id") continue;
        if (const auto* pv = src->GetProperty(key)) {
            std::string value = PropValueToString(*pv);
            if (avalang::ui::IsEventPropertyName(key)) {
                ava_ui_set_event(dst, key.c_str(), MakeStringValue(value));
            } else {
                ava_ui_set_property(dst, key.c_str(), MakeStringValue(value));
            }
        }
    }
    for (const auto* child : src->Children()) {
        AvaComponent* child_comp = ava_ui_create_component(child->TypeName().c_str());
        ComponentToAvaComponent(child, child_comp);
        ava_ui_add_child(dst, child_comp);
        ava_ui_destroy_component(child_comp);
    }
}

} // namespace

bool IsEventPropertyName(const std::string& name) {
    return avalang::ui::IsEventPropertyName(name);
}

std::string WriteAvauiText(const avalang::ui::IComponent* root, const std::string& code_behind,
                            const std::vector<PropertyRow>& initial_state,
                            const std::vector<std::string>& imports) {
    if (!root) return "";

    AvaComponentTree* tree = ava_ui_create_tree();
    AvaComponent* root_comp = ava_ui_create_component(root->TypeName().c_str());
    ComponentToAvaComponent(root, root_comp);
    ava_ui_set_root(tree, root_comp);
    ava_ui_destroy_component(root_comp);

    std::string state_json = StateToJson(initial_state);
    std::string imports_json = ImportsToJson(imports);

    char* text = ava_ui_write_avaui_text(tree, state_json.c_str(), imports_json.c_str(), code_behind.c_str(),
                                          /*extends=*/nullptr, /*routes_json=*/nullptr);
    std::string result = text ? text : "";
    ava_ui_text_free(text);
    ava_ui_destroy_tree(tree);
    return result;
}

} // namespace studio::design
