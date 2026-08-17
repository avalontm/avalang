#include "parser/AvauiPropertyCoercion.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <unordered_map>
#include <vector>

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

namespace {

std::string TrimWs(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::vector<std::string> SplitTopLevel(const std::string& text, char delim) {
    std::vector<std::string> parts;
    int depth = 0;
    bool inDouble = false;
    bool inSingle = false;
    size_t start = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        if (inDouble) {
            if (c == '\\' && i + 1 < text.size()) { ++i; continue; }
            if (c == '"') inDouble = false;
            continue;
        }
        if (inSingle) {
            if (c == '\\' && i + 1 < text.size()) { ++i; continue; }
            if (c == '\'') inSingle = false;
            continue;
        }
        if (c == '"') { inDouble = true; continue; }
        if (c == '\'') { inSingle = true; continue; }
        if (c == '(' || c == '[' || c == '{') { ++depth; continue; }
        if (c == ')' || c == ']' || c == '}') { --depth; continue; }
        if (c == delim && depth == 0) {
            parts.push_back(TrimWs(text.substr(start, i - start)));
            start = i + 1;
        }
    }
    std::string last = TrimWs(text.substr(start));
    if (!last.empty()) parts.push_back(last);
    return parts;
}

bool LooksLikeListLiteral(const std::string& s) {
    return s.size() >= 2 && s.front() == '[' && s.back() == ']';
}

bool LooksLikeRecordLiteral(const std::string& s) {
    return s.size() >= 2 && s.front() == '{' && s.back() == '}';
}

PropertyValue InferValueScalarOrList(const std::string& raw);

PropertyRecord ParseRecordLiteral(const std::string& raw) {
    PropertyRecord record;
    std::string inner = TrimWs(raw);
    if (inner.size() < 2 || inner.front() != '{' || inner.back() != '}') return record;
    inner = inner.substr(1, inner.size() - 2);

    for (const std::string& pair : SplitTopLevel(inner, ',')) {
        size_t colon = pair.find(':');
        if (colon == std::string::npos) continue;
        std::string key = TrimWs(pair.substr(0, colon));
        std::string value = TrimWs(pair.substr(colon + 1));
        if (key.empty() || value.empty()) continue;
        record[key] = InferValueScalarOrList(value);
    }
    return record;
}

PropertyList ParseListLiteral(const std::string& raw) {
    PropertyList list;
    std::string inner = TrimWs(raw);
    if (inner.size() < 2 || inner.front() != '[' || inner.back() != ']') return list;
    inner = inner.substr(1, inner.size() - 2);

    for (const std::string& item : SplitTopLevel(inner, ',')) {
        if (LooksLikeRecordLiteral(item)) {
            list.push_back(ParseRecordLiteral(item));
        }
    }
    return list;
}

PropertyValue InferValueScalarOrList(const std::string& raw) {
    if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"') {
        return PropertyValue(Unquote(raw));
    }
    if (raw == "true") return PropertyValue(true);
    if (raw == "false") return PropertyValue(false);
    double num;
    if (LooksLikeNumber(raw, &num)) return PropertyValue(num);

    std::string trimmed = TrimWs(raw);
    if (LooksLikeListLiteral(trimmed)) {
        return PropertyValue(ParseListLiteral(trimmed));
    }

    return PropertyValue(raw);
}

}  // namespace

PropertyValue InferValue(const std::string& raw) {
    return InferValueScalarOrList(raw);
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
        {"list", "ListView"},
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