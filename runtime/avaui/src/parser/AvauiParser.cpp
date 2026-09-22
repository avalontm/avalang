#include "parser/AvauiParser.h"
#include "parser/AvauiPropertyCoercion.h"
#include "events/AutoBind.h"
#include "registry/ComponentTypeRegistry.h"
#include "resolver/KnownComponentProperties.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <regex>
#include <set>
#include <sstream>
#include <unordered_set>
#include <vector>

namespace avalang {
namespace ui {
namespace parser {

ParseError::ParseError(const std::string& message, int line, int column,
                        std::string source)
    : std::runtime_error(
          message + " (" +
          (source.empty() ? "line " + std::to_string(line)
                           : source + ":" + std::to_string(line) +
                                 (column > 0 ? ":" + std::to_string(column) : "")) +
          ")"),
      line_(line), column_(column), source_(std::move(source)),
      rawMessage_(message) {}

namespace {

std::string GetSourceLine(const std::string& text, int lineNum) {
    if (lineNum <= 0) return "";
    std::istringstream iss(text);
    std::string line;
    int current = 1;
    while (std::getline(iss, line)) {
        if (current == lineNum) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            return line;
        }
        ++current;
    }
    return "";
}

}

std::string FormatParseError(const ParseErrorInfo& info, const std::string& sourceText) {
    std::ostringstream out;

    out << (info.source.empty() ? "error" : "error at " + info.source);
    if (info.line > 0) {
        out << ":" << info.line;
        if (info.column > 0) out << ":" << info.column;
    }
    out << ": " << info.message << "\n";

    std::string lineContent = GetSourceLine(sourceText, info.line);
    if (!lineContent.empty()) {
        out << "    " << info.line << " | " << lineContent << "\n";

        size_t column = info.column > 0 ? static_cast<size_t>(info.column) : 1;
        size_t displayCol = std::min(column, lineContent.size() + 1);
        size_t indent = 5 + std::to_string(info.line).size();
        out << std::string(indent, ' ');

        for (size_t i = 1; i < displayCol; ++i) {
            char c = lineContent[i - 1];
            out << ((i <= lineContent.size() &&
                     (c == '\t' || (c >= 0 && c < 32))) ? c : ' ');
        }

        out << "^";

        if (column <= lineContent.size()) {
            std::string tokenText = lineContent.substr(column - 1);
            size_t tokenEnd = tokenText.find_first_of(" \t\n\r.,;:!?()[]{}");
            if (tokenEnd == std::string::npos) tokenEnd = tokenText.size();
            for (size_t i = 1; i < tokenEnd && i < 20; ++i) out << "~";
        }
        out << "\n";
    }

    return out.str();
}

namespace {

struct Line {
    int indent;
    std::string text;
    std::string rawText;

    int lineNo;
};

std::string StripComment(const std::string& raw) {
    bool inString = false;
    for (size_t i = 0; i < raw.size(); ++i) {
        char c = raw[i];
        if (c == '"') inString = !inString;
        if (!inString && c == '#') {
            return raw.substr(0, i);
        }
    }
    return raw;
}

std::string Trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::vector<Line> Tokenize(const std::string& source) {
    std::vector<Line> lines;
    std::istringstream stream(source);
    std::string raw;
    int lineNo = 0;
    while (std::getline(stream, raw)) {
        ++lineNo;
        std::string stripped = StripComment(raw);

        size_t firstNonSpace = 0;
        int indentColumn = 0;
        for (; firstNonSpace < stripped.size(); ++firstNonSpace) {
            char c = stripped[firstNonSpace];
            if (c == ' ') {
                indentColumn += 1;
            } else if (c == '\t') {
                indentColumn += 4;
            } else {
                break;
            }
        }
        if (firstNonSpace == stripped.size()) continue;

        std::string text = Trim(stripped);
        if (text.empty()) continue;

        std::string rawText = Trim(raw.substr(firstNonSpace));
        lines.push_back({indentColumn, text, rawText, lineNo});
    }
    return lines;
}

bool IsIdentifier(const std::string& text) {
    if (text.empty()) return false;
    for (char c : text) {
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) {
            return false;
        }
    }
    return !std::isdigit(static_cast<unsigned char>(text[0]));
}

bool IsBindableProperty(const std::string& typeName, const std::string& propName) {
    static const std::unordered_set<std::string> kAlwaysBindable = {
        "checked", "selected", "value", "isOpen",
    };
    if (kAlwaysBindable.count(propName)) return true;
    return propName == "text" && typeName == "TextBox";
}

struct ReferenceScope {
    const std::set<std::string>* knownNames = nullptr;
    std::vector<std::string> loopScope;

    bool Knows(const std::string& name) const {
        if (knownNames && knownNames->count(name)) return true;
        return std::find(loopScope.begin(), loopScope.end(), name) != loopScope.end();
    }
};

std::vector<std::string> ExtractCodeIdentifiers(const std::string& code) {
    static const std::unordered_set<std::string> kKeywords = {
        "true", "false", "null", "and", "or", "not",
    };
    std::vector<std::string> names;
    size_t i = 0;
    bool inDouble = false;
    while (i < code.size()) {
        char c = code[i];
        if (inDouble) {
            if (c == '\\' && i + 1 < code.size()) { i += 2; continue; }
            if (c == '"') inDouble = false;
            ++i;
            continue;
        }
        if (c == '"') { inDouble = true; ++i; continue; }
        if (!(std::isalpha(static_cast<unsigned char>(c)) || c == '_')) {
            ++i;
            continue;
        }
        size_t start = i;
        while (i < code.size() &&
               (std::isalnum(static_cast<unsigned char>(code[i])) || code[i] == '_')) {
            ++i;
        }
        std::string word = code.substr(start, i - start);
        if (!kKeywords.count(word)) names.push_back(word);
    }
    return names;
}

std::vector<std::string> ExtractReferencedIdentifiers(const PropertyValue& value) {
    if (value.Type() != PropertyType::Expression) return {};
    const std::string& source = value.AsExpressionSource();
    if (!value.IsInterpolation()) {
        return ExtractCodeIdentifiers(source);
    }

    std::vector<std::string> names;
    size_t i = 0;
    while (i < source.size()) {
        char c = source[i];
        if (c == '\\' && i + 1 < source.size()) {
            i += 2;
            continue;
        }
        if (c != '{') {
            ++i;
            continue;
        }
        size_t start = i + 1;
        int depth = 1;
        bool inDouble = false;
        size_t j = start;
        for (; j < source.size(); ++j) {
            char cj = source[j];
            if (inDouble) {
                if (cj == '\\' && j + 1 < source.size()) { ++j; continue; }
                if (cj == '"') inDouble = false;
                continue;
            }
            if (cj == '"') { inDouble = true; continue; }
            if (cj == '{') ++depth;
            else if (cj == '}') {
                --depth;
                if (depth == 0) break;
            }
        }
        std::vector<std::string> inner = ExtractCodeIdentifiers(source.substr(start, j - start));
        names.insert(names.end(), inner.begin(), inner.end());
        i = (j < source.size()) ? j + 1 : source.size();
    }
    return names;
}

void ValidateReferencedNames(const PropertyValue& value, const ReferenceScope& scope, int lineNo,
                             int column) {
    if (!scope.knownNames) return;
    for (const std::string& name : ExtractReferencedIdentifiers(value)) {
        if (scope.Knows(name)) continue;
        throw ParseError("undeclared name '" + name + "' referenced inside '{ }' "
                              "(expected a 'var', 'const', 'func' or 'params' entry "
                              "declared at the top level, or an enclosing 'for' loop variable)",
                          lineNo, column);
    }
}

bool IsKnownProperty(const std::string& typeName, const std::string& propName) {
    static const std::unordered_set<std::string> kUniversal = [] {
        std::size_t count = 0;
        const char* const* names = avalang::ui::KnownComponentPropertyNames(count);
        std::unordered_set<std::string> set;
        for (std::size_t i = 0; i < count; ++i) set.insert(names[i]);
        return set;
    }();
    if (kUniversal.count(propName)) return true;

    const avalang::ui::registry::ComponentTypeDescriptor* descriptor =
        avalang::ui::registry::FindComponentType(typeName);
    if (!descriptor) return true;
    for (const auto& prop : descriptor->default_properties) {
        if (prop.name == propName) return true;
    }
    return false;
}

void ValidateKnownProperty(const std::string& typeName, const std::string& propName, int lineNo,
                          int column) {
    if (IsKnownProperty(typeName, propName)) return;
    throw ParseError("unknown property '" + propName + "' for element '" + typeName + "'",
                      lineNo, column);
}

void ValidateBindableProperty(const std::string& typeName, const std::string& propName,
                              const PropertyValue& value, int lineNo, int column) {
    if (!IsBindableProperty(typeName, propName)) return;
    if (value.Type() != PropertyType::Expression) return;
    if (value.IsInterpolation()) {
        throw ParseError("'" + propName + "' does not accept string interpolation "
                              "(expected a binding '{identifier}' or a literal value)",
                          lineNo, column);
    }
    if (!IsIdentifier(value.AsExpressionSource())) {
        throw ParseError("'" + propName + " = {" + value.AsExpressionSource() +
                              "}' is not a valid binding (expected a simple identifier, e.g. '" +
                              propName + " = {someVar}')",
                          lineNo, column);
    }
}

bool CoerceEventProperty(const std::string& propName, const PropertyValue& value, int lineNo,
                         int column, std::string* internalNameOut, PropertyValue* storedValueOut) {
    if (!avalang::ui::IsNewEventPropertyName(propName)) return false;
    if (value.Type() != PropertyType::Expression || value.IsInterpolation()) {
        throw ParseError("'" + propName + "' expects a handler in '{ }' "
                              "(a reference, a call, or a statement)",
                          lineNo, column);
    }
    std::string source = Trim(value.AsExpressionSource());
    if (source.empty()) {
        throw ParseError("'" + propName + "' handler cannot be empty", lineNo, column);
    }
    *internalNameOut = avalang::ui::ResolveEventPropertyName(propName);
    *storedValueOut = PropertyValue(source);
    return true;
}

void AssignProperty(IComponent* comp, const std::string& propName, const PropertyValue& value,
                    int lineNo, int column) {
    std::string internalName;
    PropertyValue storedValue;
    if (CoerceEventProperty(propName, value, lineNo, column, &internalName, &storedValue)) {
        comp->SetProperty(internalName, storedValue);
        return;
    }
    SetPropertyWithAlias(comp, propName, value);
}

bool IsPropertyLine(const std::string& text) {
    size_t eq = text.find('=');
    if (eq == std::string::npos) return false;
    std::string key = Trim(text.substr(0, eq));
    return IsIdentifier(key);
}

IComponent* ParseComponent(const std::vector<Line>& lines, size_t& idx, ComponentTree* tree,
                           std::vector<AnimationSpec>* animations, const ReferenceScope& scope);

bool IsTemplateHeader(const std::string& text) {
    return text == "template";
}

void ParseTemplateBlock(IComponent* comp, const Line& header, const std::vector<Line>& lines,
                        size_t& idx, ComponentTree* tree, std::vector<AnimationSpec>* animations,
                        const ReferenceScope& scope) {
    while (idx < lines.size()) {
        const Line& line = lines[idx];
        if (line.indent <= header.indent) {
            if (line.indent == header.indent && line.text == "end") {
                ++idx;
                return;
            }
            throw ParseError("unterminated 'template' block (expected 'end' at column " +
                                  std::to_string(header.indent) + ")",
                              line.lineNo, line.indent + 1);
        }
        if (IsPropertyLine(line.text)) {
            throw ParseError("unexpected property directly inside 'template' "
                              "(properties belong to the container, above 'template')",
                              line.lineNo, line.indent + 1);
        }
        IComponent* child = ParseComponent(lines, idx, tree, animations, scope);
        comp->AddChild(child);
    }
    throw ParseError("unterminated 'template' block (missing 'end')", header.lineNo,
                      header.indent + 1);
}

std::pair<std::string, std::string> SplitProperty(const Line& line) {
    size_t eq = line.text.find('=');
    std::string key = Trim(line.text.substr(0, eq));
    std::string value = Trim(line.text.substr(eq + 1));
    if (key.empty()) {
        throw ParseError("empty property name", line.lineNo,
                          line.indent + static_cast<int>(eq) + 1);
    }
    return {key, value};
}

bool IsComponentCall(const std::string& text, std::string* nameOut, std::string* argsOut = nullptr) {
    if (text.empty() || text.back() != ')') return false;
    size_t open = text.find('(');
    if (open == std::string::npos) return false;
    std::string name = Trim(text.substr(0, open));
    if (name.empty()) return false;

    for (char c : name) {
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) {
            return false;
        }
    }
    if (std::isdigit(static_cast<unsigned char>(name[0]))) return false;

    if (nameOut) *nameOut = name;
    if (argsOut) {
        *argsOut = text.substr(open + 1, text.size() - open - 2);
    }
    return true;
}

void ParseComponentCallArgs(const std::string& argsText, IComponent* comp, int lineNo,
                             int argsBaseColumn, const ReferenceScope& scope) {
    std::string args = Trim(argsText);
    if (args.empty()) return;

    std::vector<std::string> parts;
    std::vector<size_t> partStarts;
    bool inString = false;
    size_t start = 0;
    for (size_t i = 0; i < args.size(); ++i) {
        char c = args[i];
        if (c == '"') inString = !inString;
        if (c == ',' && !inString) {
            parts.push_back(args.substr(start, i - start));
            partStarts.push_back(start);
            start = i + 1;
        }
    }
    parts.push_back(args.substr(start));
    partStarts.push_back(start);

    for (size_t p = 0; p < parts.size(); ++p) {
        std::string part = Trim(parts[p]);
        if (part.empty()) continue;
        int partColumn = argsBaseColumn + static_cast<int>(partStarts[p]);
        size_t eq = part.find('=');
        if (eq == std::string::npos) {
            throw ParseError("expected 'key = value' in component call arguments, got: " + part,
                              lineNo, partColumn);
        }
        std::string key = Trim(part.substr(0, eq));
        std::string value = Trim(part.substr(eq + 1));
        if (key.empty()) {
            throw ParseError("empty property name in component call arguments", lineNo,
                              partColumn + static_cast<int>(eq));
        }
        PropertyValue inferred = InferValue(value);
        ValidateKnownProperty(comp->TypeName(), key, lineNo, partColumn);
        ValidateReferencedNames(inferred, scope, lineNo, partColumn);
        ValidateBindableProperty(comp->TypeName(), key, inferred, lineNo, partColumn);
        AssignProperty(comp, key, inferred, lineNo, partColumn);
    }
}

void ParseAnimateBlock(const std::vector<Line>& lines, size_t& idx, int headerIndent,
                       int headerLine, ComponentId target,
                       std::vector<AnimationSpec>* animations) {
    AnimationSpec spec;
    spec.target = target;

    while (idx < lines.size()) {
        const Line& line = lines[idx];
        if (line.indent <= headerIndent) {
            if (line.indent == headerIndent && line.text == "end") {
                ++idx;
                animations->push_back(std::move(spec));
                return;
            }
            throw ParseError("unterminated 'animate' block (expected 'end' at column " +
                                  std::to_string(headerIndent) + ")",
                              line.lineNo, line.indent + 1);
        }
        if (!IsPropertyLine(line.text)) {
            throw ParseError("expected 'key = value' inside 'animate', got: " + line.text,
                              line.lineNo, line.indent + 1);
        }
        auto kv = SplitProperty(line);
        const std::string& key = kv.first;
        std::string value = Unquote(Trim(kv.second));
        if (key == "property") spec.property = value;
        else if (key == "from") spec.fromRaw = value;
        else if (key == "to") spec.toRaw = value;
        else if (key == "duration") spec.duration = value;
        else if (key == "easing") spec.easing = value;
        else if (key == "trigger") spec.trigger = value;
        else if (key == "mode") spec.mode = value;

        ++idx;
    }
    throw ParseError("unterminated 'animate' block (missing 'end')", headerLine,
                      headerIndent + 1);
}

bool IsAnimateHeader(const std::string& text) {
    return text == "animate";
}

// Fase 8 -- helper local (misma lógica que IsBalancedBraceExpression en
// AvauiPropertyCoercion.cpp, pero esa es estática a ese .cpp): confirma que
// `raw` es exactamente una expresión "{ ... }" balanceada (consciente de
// comillas), sin nada antes ni después de las llaves.
bool TryExtractBracedExpr(const std::string& raw, std::string* inner) {
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
    if (inner) *inner = Trim(raw.substr(1, raw.size() - 2));
    return true;
}

// Sintaxis nueva (Fase 8, Opción A): "if {expr}". Se sigue aceptando la
// sintaxis vieja "if <cond> then" (usada por proyectos anteriores a la
// Opción A, p. ej. samples/web/testproj). isBracedOut, si se pasa, indica
// cuál de las dos formas produjo el match.
bool IsIfHeader(const std::string& text, std::string* condOut, bool* isBracedOut = nullptr) {
    static const std::string kPrefix = "if ";
    if (text.size() <= kPrefix.size()) return false;
    if (text.compare(0, kPrefix.size(), kPrefix) != 0) return false;
    std::string rest = Trim(text.substr(kPrefix.size()));

    std::string braced;
    if (TryExtractBracedExpr(rest, &braced)) {
        if (braced.empty()) return false;
        if (condOut) *condOut = braced;
        if (isBracedOut) *isBracedOut = true;
        return true;
    }

    static const std::string kSuffix = " then";
    if (rest.size() <= kSuffix.size()) return false;
    if (rest.compare(rest.size() - kSuffix.size(), kSuffix.size(), kSuffix) != 0) return false;
    std::string cond = Trim(rest.substr(0, rest.size() - kSuffix.size()));
    if (cond.empty()) return false;
    if (condOut) *condOut = cond;
    if (isBracedOut) *isBracedOut = false;
    return true;
}

// Sintaxis nueva (Fase 8, Opción A): "for x in {expr}". Se sigue aceptando
// la sintaxis vieja "for x in expr" (sin llaves) por la misma razón que
// arriba.
bool IsForHeader(const std::string& text, std::string* varOut, std::string* iterOut,
                  bool* isBracedOut = nullptr) {
    static const std::string kPrefix = "for ";
    if (text.size() <= kPrefix.size()) return false;
    if (text.compare(0, kPrefix.size(), kPrefix) != 0) return false;
    std::string rest = text.substr(kPrefix.size());
    size_t inPos = rest.find(" in ");
    if (inPos == std::string::npos) return false;
    std::string var = Trim(rest.substr(0, inPos));
    std::string iter = Trim(rest.substr(inPos + 4));
    if (var.empty() || iter.empty()) return false;
    for (char c : var) {
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) return false;
    }
    if (std::isdigit(static_cast<unsigned char>(var[0]))) return false;
    if (varOut) *varOut = var;

    std::string braced;
    if (TryExtractBracedExpr(iter, &braced)) {
        if (braced.empty()) return false;
        if (iterOut) *iterOut = braced;
        if (isBracedOut) *isBracedOut = true;
        return true;
    }

    if (iterOut) *iterOut = iter;
    if (isBracedOut) *isBracedOut = false;
    return true;
}

IComponent* ParseComponent(const std::vector<Line>& lines, size_t& idx, ComponentTree* tree,
                           std::vector<AnimationSpec>* animations, const ReferenceScope& scope);

bool IsReservedDeclarationKeyword(const std::string& word) {
    static const std::unordered_map<std::string, bool> kReserved = {
        {"extends", true}, {"route", true}, {"import", true},
        {"properties", true}, {"metadata", true}, {"state", true},
        {"params", true}, {"param", true}, {"style", true}, {"code", true},
        {"methods", true}, {"view", true},
        {"const", true}, {"func", true},
    };
    return kReserved.count(word) != 0;
}

void RejectDeclarationInsideView(const Line& line) {
    std::istringstream headerStream(line.text);
    std::string keyword;
    headerStream >> keyword;
    if (!IsReservedDeclarationKeyword(keyword)) return;
    throw ParseError("invalid declaration '" + keyword +
                          "' inside 'view' (declarations belong to the document top "
                          "level, outside 'view')",
                      line.lineNo, line.indent + 1);
}

void ParseComponentBody(IComponent* comp, const Line& header, const std::vector<Line>& lines,
                        size_t& idx, ComponentTree* tree, std::vector<AnimationSpec>* animations,
                        const ReferenceScope& scope) {
    while (idx < lines.size()) {
        const Line& line = lines[idx];
        if (line.indent <= header.indent) {
            if (line.indent == header.indent && line.text == "end") {
                ++idx;
                return;
            }
            throw ParseError("unterminated component block (expected 'end' at column " +
                                  std::to_string(header.indent) + ")",
                              line.lineNo, line.indent + 1);
        }
        if (IsAnimateHeader(line.text)) {
            int animateIndent = line.indent;
            int animateLine = line.lineNo;
            ++idx;
            ParseAnimateBlock(lines, idx, animateIndent, animateLine, comp->Id(), animations);
        } else if (IsTemplateHeader(line.text)) {
            Line templateHeader = line;
            ++idx;
            ParseTemplateBlock(comp, templateHeader, lines, idx, tree, animations, scope);
        } else if (std::string peekName; IsComponentCall(line.text, &peekName, nullptr)) {
            IComponent* child = ParseComponent(lines, idx, tree, animations, scope);
            comp->AddChild(child);
        } else if (IsIfHeader(line.text, nullptr) || IsForHeader(line.text, nullptr, nullptr)) {
            IComponent* child = ParseComponent(lines, idx, tree, animations, scope);
            comp->AddChild(child);
        } else if (IsPropertyLine(line.text)) {
            auto kv = SplitProperty(line);
            PropertyValue inferred = InferValue(kv.second);
            ValidateKnownProperty(comp->TypeName(), kv.first, line.lineNo, line.indent + 1);
            ValidateReferencedNames(inferred, scope, line.lineNo, line.indent + 1);
            ValidateBindableProperty(comp->TypeName(), kv.first, inferred, line.lineNo, line.indent + 1);
            AssignProperty(comp, kv.first, inferred, line.lineNo, line.indent + 1);
            ++idx;
        } else {
            RejectDeclarationInsideView(line);
            IComponent* child = ParseComponent(lines, idx, tree, animations, scope);
            comp->AddChild(child);
        }
    }
    throw ParseError("unterminated component block (missing 'end')", header.lineNo,
                      header.indent + 1);
}

IComponent* ParseComponent(const std::vector<Line>& lines, size_t& idx, ComponentTree* tree,
                           std::vector<AnimationSpec>* animations, const ReferenceScope& scope) {
    const Line& header = lines[idx];

    std::string condText;
    bool condBraced = false;
    if (IsIfHeader(header.text, &condText, &condBraced)) {
        IComponent* comp = tree->CreateComponent("If");
        comp->SetProperty("condition", PropertyValue(condText));
        if (condBraced) {
            // Solo la forma nueva "if {expr}" valida referencias (5.3):
            // la vieja "if cond then" convive con código de proyectos
            // anteriores a la Opción A que no declara todo por 'var'/'const'.
            ValidateReferencedNames(PropertyValue::MakeExpression(condText, false), scope,
                                    header.lineNo, header.indent + 1);
        }
        ++idx;
        ParseComponentBody(comp, header, lines, idx, tree, animations, scope);
        return comp;
    }

    std::string loopVar, iterExpr;
    bool iterBraced = false;
    if (IsForHeader(header.text, &loopVar, &iterExpr, &iterBraced)) {
        IComponent* comp = tree->CreateComponent("For");
        comp->SetProperty("loopVar", PropertyValue(loopVar));
        comp->SetProperty("iterable", PropertyValue(iterExpr));
        if (iterBraced) {
            ValidateReferencedNames(PropertyValue::MakeExpression(iterExpr, false), scope,
                                    header.lineNo, header.indent + 1);
        }
        ++idx;
        ReferenceScope loopScope = scope;
        loopScope.loopScope.push_back(loopVar);
        ParseComponentBody(comp, header, lines, idx, tree, animations, loopScope);
        return comp;
    }

    std::string callName;
    std::string callArgs;
    if (IsComponentCall(header.text, &callName, &callArgs)) {
        ++idx;
        IComponent* comp = tree->CreateComponent(CanonicalTypeName(callName));

        comp->SetProperty("__unresolvedImportCall", PropertyValue(true));

        size_t openParen = header.text.find('(');
        int argsBaseColumn = header.indent + static_cast<int>(openParen) + 2;
        ParseComponentCallArgs(callArgs, comp, header.lineNo, argsBaseColumn, scope);
        return comp;
    }

    std::istringstream headerStream(header.text);
    std::string typeWord;
    headerStream >> typeWord;

    IComponent* comp = tree->CreateComponent(CanonicalTypeName(typeWord));

    std::string rest = Trim(header.text.substr(typeWord.size()));
    if (!rest.empty()) {
        size_t eq = rest.find('=');
        if (eq != std::string::npos) {
            std::string key = Trim(rest.substr(0, eq));
            std::string value = Trim(rest.substr(eq + 1));
            if (!key.empty() && !value.empty()) {
                PropertyValue inferred = InferValue(value);
                ValidateKnownProperty(comp->TypeName(), key, header.lineNo, header.indent + 1);
                ValidateReferencedNames(inferred, scope, header.lineNo, header.indent + 1);
                ValidateBindableProperty(comp->TypeName(), key, inferred, header.lineNo,
                                         header.indent + 1);
                AssignProperty(comp, key, inferred, header.lineNo, header.indent + 1);
            }
        } else {
            comp->SetProperty("id", PropertyValue(rest));
        }
    }

    ++idx;
    ParseComponentBody(comp, header, lines, idx, tree, animations, scope);
    return comp;
}

std::vector<IComponent*> ParseViewBody(const std::vector<Line>& lines, size_t& idx,
                                        int headerIndent, int headerLine, ComponentTree* tree,
                                        std::vector<AnimationSpec>* animations,
                                        const ReferenceScope& scope) {
    std::vector<IComponent*> created;
    while (idx < lines.size()) {
        const Line& line = lines[idx];
        if (line.indent <= headerIndent) {
            if (line.indent == headerIndent && line.text == "end") {
                ++idx;
                return created;
            }
            throw ParseError("unterminated 'view' block (expected 'end' at column " +
                                  std::to_string(headerIndent) + ")",
                              line.lineNo, line.indent + 1);
        }
        if (IsPropertyLine(line.text) && !IsIfHeader(line.text, nullptr) &&
            !IsForHeader(line.text, nullptr, nullptr)) {
            throw ParseError("unexpected property directly inside 'view' "
                              "(properties belong to a component)",
                              line.lineNo, line.indent + 1);
        }
        RejectDeclarationInsideView(line);
        created.push_back(ParseComponent(lines, idx, tree, animations, scope));
    }
    throw ParseError("unterminated 'view' block (missing 'end')", headerLine, headerIndent + 1);
}

void ParseFlatBlock(const std::vector<Line>& lines, size_t& idx, int headerIndent,
                     int headerLine, std::unordered_map<std::string, std::string>* out) {
    while (idx < lines.size()) {
        const Line& line = lines[idx];
        if (line.indent <= headerIndent) {
            if (line.indent == headerIndent && line.text == "end") {
                ++idx;
                return;
            }
            throw ParseError("unterminated block (expected 'end' at column " +
                                  std::to_string(headerIndent) + ")",
                              line.lineNo, line.indent + 1);
        }
        if (!IsPropertyLine(line.text)) {
            throw ParseError("expected 'key = value', got: " + line.text, line.lineNo,
                              line.indent + 1);
        }
        auto kv = SplitProperty(line);
        (*out)[kv.first] = Unquote(kv.second);
        ++idx;
    }
    throw ParseError("unterminated block (missing 'end')", headerLine, headerIndent + 1);
}

void ParseParamsBlock(const std::vector<Line>& lines, size_t& idx, int headerIndent,
                       int headerLine, std::vector<ParamDeclaration>* out) {
    while (idx < lines.size()) {
        const Line& line = lines[idx];
        if (line.indent <= headerIndent) {
            if (line.indent == headerIndent && line.text == "end") {
                ++idx;
                return;
            }
            throw ParseError("unterminated 'params' block (expected 'end' at column " +
                                  std::to_string(headerIndent) + ")",
                              line.lineNo, line.indent + 1);
        }

        ParamDeclaration decl;
        if (IsPropertyLine(line.text)) {
            auto kv = SplitProperty(line);
            decl.name = kv.first;
            decl.hasDefault = true;
            decl.defaultValue = InferValue(Trim(kv.second));
        } else {
            decl.name = Trim(line.text);
            if (decl.name.empty()) {
                throw ParseError("expected a parameter name inside 'params'", line.lineNo,
                                  line.indent + 1);
            }
            decl.hasDefault = false;
        }
        out->push_back(std::move(decl));
        ++idx;
    }
    throw ParseError("unterminated 'params' block (missing 'end')", headerLine,
                      headerIndent + 1);
}

void ParseTopLevelDecl(const Line& line, const std::string& keyword, bool isConst,
                        std::set<std::string>* declared,
                        std::unordered_map<std::string, std::string>* state,
                        std::unordered_set<std::string>* constNames) {
    std::string rest = Trim(line.text.substr(keyword.size()));
    size_t eq = rest.find('=');
    if (eq == std::string::npos) {
        throw ParseError("expected '" + keyword + " name = expr'", line.lineNo, line.indent + 1);
    }
    std::string namePart = Trim(rest.substr(0, eq));
    std::string valuePart = Trim(rest.substr(eq + 1));
    size_t colon = namePart.find(':');
    std::string name = colon == std::string::npos ? namePart : Trim(namePart.substr(0, colon));
    if (!IsIdentifier(name)) {
        throw ParseError("invalid " + keyword + " name: " + name, line.lineNo, line.indent + 1);
    }
    if (valuePart.empty()) {
        throw ParseError("expected a value after '=' in '" + keyword + " " + name + "'",
                          line.lineNo, line.indent + 1);
    }
    if (!declared->insert(name).second) {
        throw ParseError("duplicate declaration: " + name, line.lineNo, line.indent + 1);
    }
    (*state)[name] = Unquote(valuePart);
    if (isConst) constNames->insert(name);
}

void ParseTopLevelBareDecl(const Line& line, std::set<std::string>* declared,
                           std::unordered_map<std::string, std::string>* state) {
    size_t eq = line.text.find('=');
    std::string namePart = Trim(line.text.substr(0, eq));
    std::string valuePart = Trim(line.text.substr(eq + 1));
    if (!IsIdentifier(namePart)) {
        throw ParseError("invalid state variable name: " + namePart, line.lineNo, line.indent + 1);
    }
    if (valuePart.empty()) {
        throw ParseError("expected a value after '=' in '" + namePart + "'", line.lineNo,
                          line.indent + 1);
    }
    if (!declared->insert(namePart).second) {
        throw ParseError("duplicate declaration: " + namePart, line.lineNo, line.indent + 1);
    }
    (*state)[namePart] = Unquote(valuePart);
}

// Fase 8 -- 'param' suelto a nivel de archivo (Opción B original para
// entradas de componente), reemplazando para la sintaxis Opción A al
// bloque envuelto 'params ... end' (ese sigue existiendo tal cual para
// proyectos anteriores, ver ParseParamsBlock). Misma forma que
// 'var'/'const': 'param nombre' o 'param nombre = valorPorDefecto'.
ParamDeclaration ParseTopLevelParamDecl(const Line& line, std::set<std::string>* declared) {
    static const std::string kKeyword = "param";
    std::string rest = Trim(line.text.substr(kKeyword.size()));
    size_t eq = rest.find('=');
    std::string namePart = Trim(eq == std::string::npos ? rest : rest.substr(0, eq));
    if (!IsIdentifier(namePart)) {
        throw ParseError("invalid param name: " + namePart, line.lineNo, line.indent + 1);
    }
    if (!declared->insert(namePart).second) {
        throw ParseError("duplicate declaration: " + namePart, line.lineNo, line.indent + 1);
    }

    ParamDeclaration decl;
    decl.name = namePart;
    if (eq == std::string::npos) {
        decl.hasDefault = false;
    } else {
        std::string valuePart = Trim(rest.substr(eq + 1));
        if (valuePart.empty()) {
            throw ParseError("expected a value after '=' in 'param " + namePart + "'",
                              line.lineNo, line.indent + 1);
        }
        decl.hasDefault = true;
        decl.defaultValue = InferValue(valuePart);
    }
    return decl;
}

bool LineAssignsToConst(const std::string& text, const std::unordered_set<std::string>& constNames,
                        std::string* nameOut) {
    size_t i = 0;
    while (i < text.size() &&
           (std::isalnum(static_cast<unsigned char>(text[i])) || text[i] == '_')) {
        ++i;
    }
    if (i == 0) return false;
    std::string name = text.substr(0, i);
    if (!constNames.count(name)) return false;

    std::string rest = Trim(text.substr(i));
    if (rest.rfind("++", 0) == 0 || rest.rfind("--", 0) == 0) {
        *nameOut = name;
        return true;
    }
    if (rest.rfind("+=", 0) == 0 || rest.rfind("-=", 0) == 0 || rest.rfind("*=", 0) == 0 ||
        rest.rfind("/=", 0) == 0) {
        *nameOut = name;
        return true;
    }
    if (!rest.empty() && rest[0] == '=' && (rest.size() < 2 || rest[1] != '=')) {
        *nameOut = name;
        return true;
    }
    return false;
}

std::string ParseTopLevelFunc(const Line& header, const std::vector<Line>& lines, size_t& idx,
                               std::set<std::string>* declared,
                               const std::unordered_set<std::string>& constNames) {
    size_t paren = header.text.find('(');
    if (paren == std::string::npos) {
        throw ParseError("expected 'func Name(...)'", header.lineNo, header.indent + 1);
    }
    std::string name = Trim(header.text.substr(4, paren - 4));
    if (!IsIdentifier(name)) {
        throw ParseError("invalid function name: " + name, header.lineNo, header.indent + 1);
    }
    if (!declared->insert(name).second) {
        throw ParseError("duplicate declaration: " + name, header.lineNo, header.indent + 1);
    }

    int headerIndent = header.indent;
    int headerLine = header.lineNo;
    std::string body = header.rawText + "\n";
    ++idx;
    while (idx < lines.size()) {
        const Line& line = lines[idx];
        if (line.indent <= headerIndent) {
            if (line.indent == headerIndent && line.text == "end") {
                body += "end\n";
                ++idx;
                return body;
            }
            throw ParseError("unterminated 'func' block (expected 'end' at column " +
                                  std::to_string(headerIndent) + ")",
                              line.lineNo, line.indent + 1);
        }
        std::string constName;
        if (LineAssignsToConst(line.text, constNames, &constName)) {
            throw ParseError("cannot assign to '" + constName +
                                  "' (declared with 'const' and cannot be reassigned)",
                              line.lineNo, line.indent + 1);
        }
        body += std::string(line.indent, ' ') + line.rawText + "\n";
        ++idx;
    }
    throw ParseError("unterminated 'func' block (missing 'end')", headerLine, headerIndent + 1);
}

std::string ParseRawBlock(const std::vector<Line>& lines, size_t& idx, int headerIndent,
                           int headerLine) {
    std::string raw;
    while (idx < lines.size()) {
        const Line& line = lines[idx];
        if (line.indent <= headerIndent) {
            if (line.indent == headerIndent && line.text == "end") {
                ++idx;
                return raw;
            }
            throw ParseError("unterminated block (expected 'end' at column " +
                                  std::to_string(headerIndent) + ")",
                              line.lineNo, line.indent + 1);
        }
        raw += std::string(line.indent - headerIndent, ' ') + line.rawText + "\n";
        ++idx;
    }
    throw ParseError("unterminated block (missing 'end')", headerLine, headerIndent + 1);
}

struct TopLevelNames {
    std::set<std::string> known;
    std::unordered_set<std::string> constNames;
};

TopLevelNames CollectTopLevelNames(const std::vector<Line>& lines) {
    TopLevelNames result;
    for (size_t i = 0; i < lines.size(); ++i) {
        const Line& line = lines[i];
        if (line.indent != 0) continue;

        std::istringstream headerStream(line.text);
        std::string keyword;
        headerStream >> keyword;
        std::string rest = Trim(line.text.substr(keyword.size()));

        if (keyword == "const") {
            size_t eq = rest.find('=');
            std::string namePart = eq == std::string::npos ? rest : Trim(rest.substr(0, eq));
            size_t colon = namePart.find(':');
            std::string name = Trim(colon == std::string::npos ? namePart
                                                                 : namePart.substr(0, colon));
            if (IsIdentifier(name)) {
                result.known.insert(name);
                result.constNames.insert(name);
            }
        } else if (!rest.empty() && rest[0] == '=' && IsIdentifier(keyword)) {
            result.known.insert(keyword);
        } else if (keyword == "func") {
            size_t paren = rest.find('(');
            std::string name = Trim(paren == std::string::npos ? rest : rest.substr(0, paren));
            if (IsIdentifier(name)) result.known.insert(name);
        } else if (keyword == "param") {
            size_t eq = rest.find('=');
            std::string name = Trim(eq == std::string::npos ? rest : rest.substr(0, eq));
            if (IsIdentifier(name)) result.known.insert(name);
        } else if (keyword == "params") {
            size_t j = i + 1;
            while (j < lines.size() && lines[j].indent > line.indent) {
                const std::string& paramText = lines[j].text;
                size_t eq = paramText.find('=');
                std::string name =
                    Trim(eq == std::string::npos ? paramText : paramText.substr(0, eq));
                if (IsIdentifier(name)) result.known.insert(name);
                ++j;
            }
        }
    }
    return result;
}

}

RouteDeclaration ParseRoute(const std::string& template_str) {
    RouteDeclaration route;
    route.route_template = template_str;

    static const std::regex kParamRe(R"(\{(\w+)(\?)?(?::(\w+))?\})");
    auto begin = std::sregex_iterator(template_str.begin(), template_str.end(), kParamRe);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        const std::smatch& pm = *it;
        RouteParameter param;
        param.name = pm[1].str();
        param.kind = pm[2].matched ? RouteParameterKind::Optional : RouteParameterKind::Required;
        param.constraint = pm[3].matched ? pm[3].str() : "";
        route.parameters.push_back(std::move(param));
    }

    return route;
}

namespace {

ParsedAvaui ParseImpl(const std::string& source) {
    std::vector<Line> lines = Tokenize(source);

    ParsedAvaui result;
    result.tree = ComponentTree::Create();
    bool sawExtends = false;
    bool sawView = false;
    int viewLine = 0;
    std::set<std::string> declaredNames;
    TopLevelNames topNames = CollectTopLevelNames(lines);

    size_t idx = 0;
    while (idx < lines.size()) {
        const Line& line = lines[idx];
        if (line.indent != 0) {
            throw ParseError("unexpected indentation at document top level", line.lineNo,
                              line.indent + 1);
        }
        int headerLine = line.lineNo;

        std::istringstream headerStream(line.text);
        std::string keyword;
        headerStream >> keyword;
        std::string rest = Trim(line.text.substr(keyword.size()));

        if (keyword == "extends") {
            if (!sawExtends) {
                result.extends = Unquote(rest);
                sawExtends = true;
            }
            ++idx;
        } else if (keyword == "route") {
            result.routes.push_back(ParseRoute(Unquote(rest)));
            ++idx;
        } else if (keyword == "import") {
            result.imports.push_back(Unquote(rest));
            ++idx;
        } else if (keyword == "properties" || keyword == "metadata") {
            ++idx;
            ParseFlatBlock(lines, idx, 0, headerLine, &result.properties);
        } else if (keyword == "state") {
            ++idx;
            ParseFlatBlock(lines, idx, 0, headerLine, &result.state);
        } else if (keyword == "const") {
            ParseTopLevelDecl(line, "const", true, &declaredNames, &result.state,
                               &result.constNames);
            ++idx;
        } else if (keyword == "func") {
            result.code += ParseTopLevelFunc(line, lines, idx, &declaredNames, topNames.constNames);
        } else if (keyword == "param") {
            result.params.push_back(ParseTopLevelParamDecl(line, &declaredNames));
            ++idx;
        } else if (keyword == "params") {
            ++idx;
            ParseParamsBlock(lines, idx, 0, headerLine, &result.params);
        } else if (keyword == "style") {
            ++idx;
            ParseFlatBlock(lines, idx, 0, headerLine, &result.style);
        } else if (keyword == "code" || keyword == "methods") {
            ++idx;
            result.code = ParseRawBlock(lines, idx, 0, headerLine);
        } else if (keyword == "view") {
            if (sawView) {
                throw ParseError("multiple 'view' blocks are not allowed (a document "
                                  "must define exactly one 'view'; first one at line " +
                                      std::to_string(viewLine) + ")",
                                  line.lineNo, line.indent + 1);
            }
            sawView = true;
            viewLine = headerLine;
            ++idx;
            std::vector<IComponent*> topLevel =
                ParseViewBody(lines, idx, 0, headerLine, result.tree.get(), &result.animations,
                             ReferenceScope{&topNames.known, {}});

            IComponent* root = result.tree->CreateComponent("Page");
            for (IComponent* child : topLevel) {
                root->AddChild(child);
            }
            result.tree->SetRoot(root);
        } else if (IsComponentCall(line.text, nullptr, nullptr) ||
                   IsIfHeader(line.text, nullptr) ||
                   IsForHeader(line.text, nullptr, nullptr)) {
            throw ParseError("component '" + keyword +
                                  "' found outside 'view' (components must be declared "
                                  "inside the 'view' block)",
                              line.lineNo, line.indent + 1);
        } else if (!rest.empty() && rest[0] == '=' && IsIdentifier(keyword)) {
            ParseTopLevelBareDecl(line, &declaredNames, &result.state);
            ++idx;
        } else {
            throw ParseError("unknown top-level block: " + keyword, line.lineNo,
                              line.indent + 1);
        }
    }

    if (!sawView) {
        throw ParseError("missing 'view' block (every .avaui document must define "
                          "exactly one 'view')",
                          lines.empty() ? 1 : lines.back().lineNo, 1);
    }

    AutoBindEvents(result.tree->Root(), result.code);

    return result;
}

}

ParsedAvaui AvauiParser::Parse(const std::string& source, const std::string& sourcePath) {
    try {
        return ParseImpl(source);
    } catch (const ParseError& e) {
        if (sourcePath.empty() || !e.Source().empty()) {
            throw;
        }
        throw ParseError(e.RawMessage(), e.Line(), e.Column(), sourcePath);
    }
}

}
}
}