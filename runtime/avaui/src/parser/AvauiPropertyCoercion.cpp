#include "parser/AvauiPropertyCoercion.h"

#include <cctype>
#include <cstdlib>
#include <unordered_map>

namespace avalang {
namespace ui {
namespace parser {

std::string Unquote(const std::string& s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

bool LooksLikeNumber(const std::string& s, double* out) {
    if (s.empty()) return false;
    char* end = nullptr;
    double v = std::strtod(s.c_str(), &end);
    if (end != s.c_str() + s.size()) return false;
    *out = v;
    return true;
}

PropertyValue InferValue(const std::string& raw) {
    if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"') {
        return PropertyValue(Unquote(raw));
    }
    if (raw == "true") return PropertyValue(true);
    if (raw == "false") return PropertyValue(false);
    double num;
    if (LooksLikeNumber(raw, &num)) return PropertyValue(num);
    // Cualquier otra cosa (identificador suelto, concatenacion,
    // expresion sin resolver) se deja como texto opaco -- mismo "soft
    // fallback" que ya documentaba AvauiParser.h ("Semantic gaps").
    return PropertyValue(raw);
}

namespace {

// Ver AvauiPropertyCoercion.h -- estos dos mapas eran privados a
// AvauiParser.cpp; ahora viven aca porque SetPropertyWithAlias y
// CanonicalTypeName son de uso publico.
const std::unordered_map<std::string, std::string>& PropertyAliases() {
    static const std::unordered_map<std::string, std::string> kPropertyAliases = {
        {"gap", "spacing"},
        {"value", "text"},
    };
    return kPropertyAliases;
}

const std::unordered_map<std::string, std::string>& TypeNames() {
    static const std::unordered_map<std::string, std::string> kTypeNames = {
        {"page", "Page"},         {"container", "Container"},
        {"row", "Row"},           {"column", "Column"},
        {"stack", "Stack"},       {"text", "Text"},
        {"label", "Label"},       {"button", "Button"},
        {"image", "Image"},       {"input", "TextBox"},
        {"checkbox", "CheckBox"}, {"icon", "Icon"},
        {"link", "Link"},         {"textbox", "TextBox"},
        {"combobox", "ComboBox"}, {"radiobutton", "RadioButton"},
        {"radio", "RadioButton"}, {"dialog", "Dialog"},
        {"scrollview", "ScrollView"}, {"scroll", "ScrollView"},
    };
    return kTypeNames;
}

} // namespace

std::string CanonicalTypeName(const std::string& asWritten) {
    const auto& types = TypeNames();
    auto it = types.find(asWritten);
    if (it != types.end()) return it->second;
    if (asWritten.empty()) return asWritten;
    std::string result = asWritten;
    result[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(result[0])));
    return result;
}

void SetPropertyWithAlias(IComponent* component, const std::string& name,
                           const PropertyValue& value) {
    component->SetProperty(name, value);
    const auto& aliases = PropertyAliases();
    auto alias = aliases.find(name);
    if (alias != aliases.end() && alias->second != name) {
        component->SetProperty(alias->second, value);
    }
}

} // namespace parser
} // namespace ui
} // namespace avalang
