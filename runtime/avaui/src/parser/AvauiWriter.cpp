#include "parser/AvauiWriter.h"
#include "parser/AvauiPropertyCoercion.h"
#include "events/AutoBind.h"

#include <cctype>
#include <sstream>

namespace avalang {
namespace ui {
namespace parser {

namespace {

std::string WritePropertyValue(const std::string& value) {
    if (value == "true" || value == "false") return value;
    double unused = 0.0;
    if (LooksLikeNumber(value, &unused)) return value;
    std::string escaped;
    escaped.reserve(value.size() + 2);
    for (char c : value) {
        if (c == '"' || c == '\\') escaped.push_back('\\');
        escaped.push_back(c);
    }
    return "\"" + escaped + "\"";
}

std::string ValueToDisplayString(const PropertyValue& pv) {
    switch (pv.Type()) {
        case PropertyType::Bool: return pv.AsBool() ? "true" : "false";
        case PropertyType::Number: {
            double n = pv.AsNumber();
            if (n == static_cast<long long>(n)) return std::to_string(static_cast<long long>(n));
            return std::to_string(n);
        }
        case PropertyType::String: return pv.AsString();
        case PropertyType::List: {
            std::ostringstream out;
            out << "[";
            bool firstItem = true;
            for (const auto& record : pv.AsList()) {
                if (!firstItem) out << ", ";
                firstItem = false;
                out << "{";
                bool firstField = true;
                for (const auto& [key, value] : record) {
                    if (!firstField) out << ", ";
                    firstField = false;
                    out << key << ": " << WritePropertyValue(ValueToDisplayString(value));
                }
                out << "}";
            }
            out << "]";
            return out.str();
        }
        default: return "";
    }
}

std::vector<const AnimationSpec*> AnimationsFor(const std::vector<AnimationSpec>& animations,
                                                 const IComponent* node) {
    std::vector<const AnimationSpec*> result;
    for (const auto& spec : animations) {
        if (spec.target == node->Id()) result.push_back(&spec);
    }
    return result;
}

void WriteAnimationField(const std::string& key, const std::string& value, const std::string& pad,
                         std::ostringstream& out) {
    if (value.empty()) return;
    out << pad << key << " = " << WritePropertyValue(value) << "\n";
}

void WriteAnimateBlock(const AnimationSpec& spec, int indent, std::ostringstream& out) {
    const std::string pad(static_cast<size_t>(indent) * 4, ' ');
    const std::string inner_pad(static_cast<size_t>(indent + 1) * 4, ' ');

    out << pad << "animate\n";
    WriteAnimationField("property", spec.property, inner_pad, out);
    WriteAnimationField("from", spec.fromRaw, inner_pad, out);
    WriteAnimationField("to", spec.toRaw, inner_pad, out);
    WriteAnimationField("duration", spec.duration, inner_pad, out);
    WriteAnimationField("easing", spec.easing, inner_pad, out);
    WriteAnimationField("trigger", spec.trigger, inner_pad, out);
    WriteAnimationField("mode", spec.mode, inner_pad, out);
    out << pad << "end\n";
}

bool IsCallForm(const IComponent* node) {
    if (node->TypeName().empty()) return false;
    if (!std::isupper(static_cast<unsigned char>(node->TypeName()[0]))) return false;
    if (const auto* idProp = node->GetProperty("id")) {
        if (idProp->Type() == PropertyType::String && !idProp->AsString().empty()) return false;
    }
    for (const auto& key : node->PropertyNames()) {
        if (key == "id") continue;
        return false;
    }
    return node->Children().empty();
}

void WriteNode(const IComponent* node, int indent, std::ostringstream& out,
               const std::vector<AnimationSpec>& animations) {
    const std::string pad(static_cast<size_t>(indent) * 4, ' ');

    const std::vector<const AnimationSpec*> node_animations = AnimationsFor(animations, node);

    if (node_animations.empty() && IsCallForm(node)) {
        out << pad << node->TypeName() << "()\n";
        return;
    }

    out << pad << node->TypeName() << "\n";
    const std::string inner_pad(static_cast<size_t>(indent + 1) * 4, ' ');

    bool wrote_anything = false;
    if (const auto* idProp = node->GetProperty("id")) {
        if (idProp->Type() == PropertyType::String && !idProp->AsString().empty()) {
            out << inner_pad << "id = " << WritePropertyValue(idProp->AsString()) << "\n";
            wrote_anything = true;
        }
    }

    bool has_events = false;
    for (const auto& key : node->PropertyNames()) {
        if (key == "id" || key == "__layout") continue;
        if (avalang::ui::IsEventPropertyName(key)) {
            has_events = true;
            continue;
        }
        if (const auto* pv = node->GetProperty(key)) {
            out << inner_pad << key << " = " << WritePropertyValue(ValueToDisplayString(*pv)) << "\n";
            wrote_anything = true;
        }
    }

    if (has_events) {
        for (const auto& key : node->PropertyNames()) {
            if (key == "id" || key == "__layout") continue;
            if (!avalang::ui::IsEventPropertyName(key)) continue;
            if (const auto* pv = node->GetProperty(key)) {
                out << inner_pad << key << " = " << ValueToDisplayString(*pv) << "\n";
                wrote_anything = true;
            }
        }
    }

    for (const AnimationSpec* spec : node_animations) {
        WriteAnimateBlock(*spec, indent + 1, out);
        wrote_anything = true;
    }

    const auto children = node->Children();
    if (!children.empty() && wrote_anything) {
        out << "\n";
    }
    for (const auto* child : children) {
        WriteNode(child, indent + 1, out, animations);
    }
    out << pad << "end\n";
}

}

std::string WriteAvaui(const IComponent* root, const AvauiWriteOptions& options) {
    if (!root) return "";

    std::ostringstream out;

    if (!options.extends.empty()) {
        out << "extends " << options.extends << "\n\n";
    }

    for (const auto& route : options.routes) {
        out << "route " << WritePropertyValue(route.route_template) << "\n";
    }
    if (!options.routes.empty()) out << "\n";

    for (const auto& imp : options.imports) {
        out << "import " << imp << "\n";
    }
    if (!options.imports.empty()) out << "\n";

    bool has_page_properties = false;
    if (const auto* idProp = root->GetProperty("id")) {
        if (idProp->Type() == PropertyType::String && !idProp->AsString().empty()) {
            has_page_properties = true;
        }
    }
    if (!has_page_properties) {
        for (const auto& key : root->PropertyNames()) {
            if (key == "id" || key == "__layout") continue;
            has_page_properties = true;
            break;
        }
    }
    if (has_page_properties) {
        out << "properties\n";
        if (const auto* idProp = root->GetProperty("id")) {
            if (idProp->Type() == PropertyType::String && !idProp->AsString().empty()) {
                out << "    id = " << WritePropertyValue(idProp->AsString()) << "\n";
            }
        }
        for (const auto& key : root->PropertyNames()) {
            if (key == "id" || key == "__layout") continue;
            if (avalang::ui::IsEventPropertyName(key)) continue;
            if (const auto* pv = root->GetProperty(key)) {
                out << "    " << key << " = " << WritePropertyValue(ValueToDisplayString(*pv)) << "\n";
            }
        }
        out << "end\n\n";
    }

    if (!options.initial_state.empty()) {
        out << "state\n";
        for (const auto& entry : options.initial_state) {
            out << "    " << entry.key << " = " << WritePropertyValue(entry.value) << "\n";
        }
        out << "end\n\n";
    }

    out << "view\n";
    for (const auto* child : root->Children()) {
        WriteNode(child, 1, out, options.animations);
    }
    out << "end\n";

    if (!options.code_behind.empty()) {
        out << "\ncode\n" << options.code_behind;
        if (!options.code_behind.empty() && options.code_behind.back() != '\n') out << "\n";
        out << "end\n";
    }

    return out.str();
}

}
}
}