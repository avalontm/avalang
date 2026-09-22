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
        std::string inner = s.substr(1, s.size() - 2);
        std::string out;
        out.reserve(inner.size());
        for (size_t i = 0; i < inner.size(); ++i) {
            char c = inner[i];
            if (c == '\\' && i + 1 < inner.size()) {
                char next = inner[i + 1];
                if (next == '"' || next == '\\' || next == '{') {
                    out.push_back(next);
                    ++i;
                    continue;
                }
                if (next == 'n') {
                    out.push_back('\n');
                    ++i;
                    continue;
                }
            }
            out.push_back(c);
        }
        return out;
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

bool IsBalancedBraceExpression(const std::string& raw, std::string* inner) {
    if (raw.size() < 2 || raw.front() != '{' || raw.back() != '}') return false;
    int depth = 0;
    bool inDouble = false;
    for (size_t i = 0; i < raw.size(); ++i) {
        char c = raw[i];
        if (inDouble) {
            if (c == '\\' && i + 1 < raw.size()) { ++i; continue; }
            if (c == '"') inDouble = false;
            continue;
        }
        if (c == '"') { inDouble = true; continue; }
        if (c == '{') {
            ++depth;
        } else if (c == '}') {
            --depth;
            if (depth == 0 && i != raw.size() - 1) return false;
        }
    }
    if (depth != 0) return false;
    if (inner) *inner = TrimWs(raw.substr(1, raw.size() - 2));
    return true;
}

bool StringLiteralHasInterpolation(const std::string& quoted) {
    if (quoted.size() < 2 || quoted.front() != '"' || quoted.back() != '"') return false;
    std::string inner = quoted.substr(1, quoted.size() - 2);
    for (size_t i = 0; i < inner.size(); ++i) {
        char c = inner[i];
        if (c == '\\' && i + 1 < inner.size()) { ++i; continue; }
        if (c == '{') return true;
    }
    return false;
}

std::string BuildInterpolationTemplate(const std::string& inner) {
    std::string out;
    out.reserve(inner.size());
    size_t i = 0;
    while (i < inner.size()) {
        char c = inner[i];
        if (c == '\\' && i + 1 < inner.size()) {
            char next = inner[i + 1];
            if (next == '"' || next == '\\' || next == '{') {
                out.push_back(next);
                i += 2;
                continue;
            }
            if (next == 'n') {
                out.push_back('\n');
                i += 2;
                continue;
            }
            out.push_back(c);
            ++i;
            continue;
        }
        if (c == '{') {
            size_t start = i;
            int depth = 0;
            bool inDouble = false;
            size_t j = i;
            for (; j < inner.size(); ++j) {
                char cj = inner[j];
                if (inDouble) {
                    if (cj == '\\' && j + 1 < inner.size()) { ++j; continue; }
                    if (cj == '"') inDouble = false;
                    continue;
                }
                if (cj == '"') { inDouble = true; continue; }
                if (cj == '{') ++depth;
                else if (cj == '}') {
                    --depth;
                    if (depth == 0) { ++j; break; }
                }
            }
            out += inner.substr(start, j - start);
            i = j;
            continue;
        }
        out.push_back(c);
        ++i;
    }
    return out;
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
    std::string trimmed = TrimWs(raw);

    std::string exprInner;
    if (IsBalancedBraceExpression(trimmed, &exprInner)) {
        return PropertyValue::MakeExpression(exprInner, false);
    }

    if (trimmed.size() >= 2 && trimmed.front() == '$') {
        std::string quoted = TrimWs(trimmed.substr(1));
        if (quoted.size() >= 2 && quoted.front() == '"' && quoted.back() == '"') {
            std::string inner = quoted.substr(1, quoted.size() - 2);
            return PropertyValue::MakeExpression(BuildInterpolationTemplate(inner), true);
        }
    }

    if (trimmed.size() >= 2 && trimmed.front() == '"' && trimmed.back() == '"') {
        if (StringLiteralHasInterpolation(trimmed)) {
            std::string inner = trimmed.substr(1, trimmed.size() - 2);
            return PropertyValue::MakeExpression(BuildInterpolationTemplate(inner), true);
        }
        return PropertyValue(Unquote(trimmed));
    }

    if (trimmed == "true") return PropertyValue(true);
    if (trimmed == "false") return PropertyValue(false);
    double num;
    if (LooksLikeNumber(trimmed, &num)) return PropertyValue(num);

    if (LooksLikeListLiteral(trimmed)) {
        return PropertyValue(ParseListLiteral(trimmed));
    }

    return PropertyValue(trimmed);
}

}

PropertyValue InferValue(const std::string& raw) {
    return InferValueScalarOrList(raw);
}

namespace {

const std::unordered_map<std::string, std::string>& PropertyAliases() {
    static const std::unordered_map<std::string, std::string> kPropertyAliases = {
        {"gap", "spacing"},
        {"value", "text"},
        {"checked", "isChecked"},
        {"selected", "isSelected"},
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