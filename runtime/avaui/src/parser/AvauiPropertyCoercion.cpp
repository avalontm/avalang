#include "parser/AvauiPropertyCoercion.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <unordered_map>

#include "registry/ComponentTypeRegistry.h"

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



    return PropertyValue(raw);
}

namespace {




const std::unordered_map<std::string, std::string>& PropertyAliases() {
    static const std::unordered_map<std::string, std::string> kPropertyAliases = {
        {"gap", "spacing"},
        {"value", "text"},
    };
    return kPropertyAliases;
}

const std::unordered_map<std::string, std::string>& TypeNameAliases() {








    static const std::unordered_map<std::string, std::string> kTypeNameAliases = {
        {"input", "TextBox"},
        {"radio", "RadioButton"},
        {"scroll", "ScrollView"},
    };
    return kTypeNameAliases;
}

const std::unordered_map<std::string, std::string>& TypeNames() {









    static const std::unordered_map<std::string, std::string> kTypeNames = [] {
        std::unordered_map<std::string, std::string> types;
        for (const avalang::ui::registry::ComponentTypeDescriptor& descriptor :
             avalang::ui::registry::GetComponentTypeRegistry()) {
            std::string lower = descriptor.type;
            std::transform(lower.begin(), lower.end(), lower.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            types[lower] = descriptor.type;
        }
        for (const auto& [alias, canonical] : TypeNameAliases()) {
            types[alias] = canonical;
        }
        return types;
    }();
    return kTypeNames;
}

}

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

std::string NumberToDisplayString(double n) {
    std::ostringstream oss;
    oss << n;
    return oss.str();
}

bool LooksLikeCall(const std::string& s) {
    return !s.empty() && s.back() == ')' &&
           s.find('(') != std::string::npos;
}

}
}
}