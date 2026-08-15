#include "parser/AvauiParser.h"
#include "parser/AvauiPropertyCoercion.h"
#include "events/AutoBind.h"

#include <cctype>
#include <cstdlib>
#include <regex>
#include <sstream>

namespace avalang {
namespace ui {
namespace parser {

ParseError::ParseError(const std::string& message, int line)
    : std::runtime_error(message + " (line " + std::to_string(line) + ")"),
      line_(line) {}

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

bool IsPropertyLine(const std::string& text) {

    return text.find('=') != std::string::npos;
}

std::pair<std::string, std::string> SplitProperty(const Line& line) {
    size_t eq = line.text.find('=');
    std::string key = Trim(line.text.substr(0, eq));
    std::string value = Trim(line.text.substr(eq + 1));
    if (key.empty()) {
        throw ParseError("empty property name", line.lineNo);
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

    *nameOut = name;
    if (argsOut) {
        *argsOut = text.substr(open + 1, text.size() - open - 2);
    }
    return true;
}

void ParseComponentCallArgs(const std::string& argsText, IComponent* comp, int lineNo) {
    std::string args = Trim(argsText);
    if (args.empty()) return;

    std::vector<std::string> parts;
    bool inString = false;
    size_t start = 0;
    for (size_t i = 0; i < args.size(); ++i) {
        char c = args[i];
        if (c == '"') inString = !inString;
        if (c == ',' && !inString) {
            parts.push_back(args.substr(start, i - start));
            start = i + 1;
        }
    }
    parts.push_back(args.substr(start));

    for (const std::string& rawPart : parts) {
        std::string part = Trim(rawPart);
        if (part.empty()) continue;
        size_t eq = part.find('=');
        if (eq == std::string::npos) {
            throw ParseError("expected 'key = value' in component call arguments, got: " + part, lineNo);
        }
        std::string key = Trim(part.substr(0, eq));
        std::string value = Trim(part.substr(eq + 1));
        if (key.empty()) {
            throw ParseError("empty property name in component call arguments", lineNo);
        }
        SetPropertyWithAlias(comp, key, InferValue(value));
    }
}

void ParseAnimateBlock(const std::vector<Line>& lines, size_t& idx, int headerIndent,
                       ComponentId target, std::vector<AnimationSpec>* animations) {
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
                              line.lineNo);
        }
        if (!IsPropertyLine(line.text)) {
            throw ParseError("expected 'key = value' inside 'animate', got: " + line.text,
                              line.lineNo);
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
    throw ParseError("unterminated 'animate' block (missing 'end')", headerIndent);
}

bool IsAnimateHeader(const std::string& text) {
    return text == "animate";
}

bool IsIfHeader(const std::string& text, std::string* condOut) {
    static const std::string kPrefix = "if ";
    static const std::string kSuffix = " then";
    if (text.size() <= kPrefix.size() + kSuffix.size()) return false;
    if (text.compare(0, kPrefix.size(), kPrefix) != 0) return false;
    if (text.compare(text.size() - kSuffix.size(), kSuffix.size(), kSuffix) != 0) return false;
    if (condOut) {
        *condOut = Trim(text.substr(kPrefix.size(),
                                     text.size() - kPrefix.size() - kSuffix.size()));
        if (condOut->empty()) return false;
    }
    return true;
}

bool IsForHeader(const std::string& text, std::string* varOut, std::string* iterOut) {
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
    if (iterOut) *iterOut = iter;
    return true;
}

IComponent* ParseComponent(const std::vector<Line>& lines, size_t& idx, ComponentTree* tree,
                           std::vector<AnimationSpec>* animations);

void ParseComponentBody(IComponent* comp, const Line& header, const std::vector<Line>& lines,
                        size_t& idx, ComponentTree* tree, std::vector<AnimationSpec>* animations) {
    while (idx < lines.size()) {
        const Line& line = lines[idx];
        if (line.indent <= header.indent) {
            if (line.indent == header.indent && line.text == "end") {
                ++idx;
                return;
            }
            throw ParseError("unterminated component block (expected 'end' at column " +
                                  std::to_string(header.indent) + ")",
                              line.lineNo);
        }
        if (IsAnimateHeader(line.text)) {
            int animateIndent = line.indent;
            ++idx;
            ParseAnimateBlock(lines, idx, animateIndent, comp->Id(), animations);
        } else if (std::string peekName; IsComponentCall(line.text, &peekName, nullptr)) {
            IComponent* child = ParseComponent(lines, idx, tree, animations);
            comp->AddChild(child);
        } else if (IsIfHeader(line.text, nullptr) || IsForHeader(line.text, nullptr, nullptr)) {
            IComponent* child = ParseComponent(lines, idx, tree, animations);
            comp->AddChild(child);
        } else if (IsPropertyLine(line.text)) {
            auto kv = SplitProperty(line);
            SetPropertyWithAlias(comp, kv.first, InferValue(kv.second));
            ++idx;
        } else {
            IComponent* child = ParseComponent(lines, idx, tree, animations);
            comp->AddChild(child);
        }
    }
    throw ParseError("unterminated component block (missing 'end')", header.lineNo);
}

IComponent* ParseComponent(const std::vector<Line>& lines, size_t& idx, ComponentTree* tree,
                           std::vector<AnimationSpec>* animations) {
    const Line& header = lines[idx];

    std::string condText;
    if (IsIfHeader(header.text, &condText)) {
        IComponent* comp = tree->CreateComponent("If");
        comp->SetProperty("condition", PropertyValue(condText));
        ++idx;
        ParseComponentBody(comp, header, lines, idx, tree, animations);
        return comp;
    }

    std::string loopVar, iterExpr;
    if (IsForHeader(header.text, &loopVar, &iterExpr)) {
        IComponent* comp = tree->CreateComponent("For");
        comp->SetProperty("loopVar", PropertyValue(loopVar));
        comp->SetProperty("iterable", PropertyValue(iterExpr));
        ++idx;
        ParseComponentBody(comp, header, lines, idx, tree, animations);
        return comp;
    }

    std::string callName;
    std::string callArgs;
    if (IsComponentCall(header.text, &callName, &callArgs)) {
        ++idx;
        IComponent* comp = tree->CreateComponent(CanonicalTypeName(callName));

        comp->SetProperty("__unresolvedImportCall", PropertyValue(true));

        ParseComponentCallArgs(callArgs, comp, header.lineNo);
        return comp;
    }

    std::istringstream headerStream(header.text);
    std::string typeWord, idWord;
    headerStream >> typeWord;
    headerStream >> idWord;

    IComponent* comp = tree->CreateComponent(CanonicalTypeName(typeWord));
    if (!idWord.empty()) {
        comp->SetProperty("id", PropertyValue(idWord));
    }

    ++idx;
    ParseComponentBody(comp, header, lines, idx, tree, animations);
    return comp;
}

std::vector<IComponent*> ParseViewBody(const std::vector<Line>& lines, size_t& idx,
                                        int headerIndent, ComponentTree* tree,
                                        std::vector<AnimationSpec>* animations) {
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
                              line.lineNo);
        }
        if (IsPropertyLine(line.text) && !IsIfHeader(line.text, nullptr) &&
            !IsForHeader(line.text, nullptr, nullptr)) {
            throw ParseError("unexpected property directly inside 'view' "
                              "(properties belong to a component)",
                              line.lineNo);
        }
        created.push_back(ParseComponent(lines, idx, tree, animations));
    }
    throw ParseError("unterminated 'view' block (missing 'end')", headerIndent);
}


void ParseFlatBlock(const std::vector<Line>& lines, size_t& idx, int headerIndent,
                     std::unordered_map<std::string, std::string>* out) {
    while (idx < lines.size()) {
        const Line& line = lines[idx];
        if (line.indent <= headerIndent) {
            if (line.indent == headerIndent && line.text == "end") {
                ++idx;
                return;
            }
            throw ParseError("unterminated block (expected 'end' at column " +
                                  std::to_string(headerIndent) + ")",
                              line.lineNo);
        }
        if (!IsPropertyLine(line.text)) {
            throw ParseError("expected 'key = value', got: " + line.text, line.lineNo);
        }
        auto kv = SplitProperty(line);
        (*out)[kv.first] = Unquote(kv.second);
        ++idx;
    }
    throw ParseError("unterminated block (missing 'end')", headerIndent);
}

void ParseParamsBlock(const std::vector<Line>& lines, size_t& idx, int headerIndent,
                       std::vector<ParamDeclaration>* out) {
    while (idx < lines.size()) {
        const Line& line = lines[idx];
        if (line.indent <= headerIndent) {
            if (line.indent == headerIndent && line.text == "end") {
                ++idx;
                return;
            }
            throw ParseError("unterminated 'params' block (expected 'end' at column " +
                                  std::to_string(headerIndent) + ")",
                              line.lineNo);
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
                throw ParseError("expected a parameter name inside 'params'", line.lineNo);
            }
            decl.hasDefault = false;
        }
        out->push_back(std::move(decl));
        ++idx;
    }
    throw ParseError("unterminated 'params' block (missing 'end')", headerIndent);
}

std::string ParseRawBlock(const std::vector<Line>& lines, size_t& idx, int headerIndent) {
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
                              line.lineNo);
        }
        raw += std::string(line.indent - headerIndent, ' ') + line.rawText + "\n";
        ++idx;
    }
    throw ParseError("unterminated block (missing 'end')", headerIndent);
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

ParsedAvaui AvauiParser::Parse(const std::string& source) {
    std::vector<Line> lines = Tokenize(source);

    ParsedAvaui result;
    result.tree = ComponentTree::Create();
    bool sawExtends = false;

    size_t idx = 0;
    while (idx < lines.size()) {
        const Line& line = lines[idx];
        if (line.indent != 0) {
            throw ParseError("unexpected indentation at document top level", line.lineNo);
        }

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
            ParseFlatBlock(lines, idx, 0, &result.properties);
        } else if (keyword == "state") {
            ++idx;
            ParseFlatBlock(lines, idx, 0, &result.state);
        } else if (keyword == "params") {
            ++idx;
            ParseParamsBlock(lines, idx, 0, &result.params);
        } else if (keyword == "style") {
            ++idx;
            ParseFlatBlock(lines, idx, 0, &result.style);
        } else if (keyword == "code" || keyword == "methods") {
            ++idx;
            result.code = ParseRawBlock(lines, idx, 0);
        } else if (keyword == "view") {
            ++idx;
            std::vector<IComponent*> topLevel =
                ParseViewBody(lines, idx, 0, result.tree.get(), &result.animations);

            IComponent* root = result.tree->CreateComponent("Page");
            for (IComponent* child : topLevel) {
                root->AddChild(child);
            }
            result.tree->SetRoot(root);
        } else {
            throw ParseError("unknown top-level block: " + keyword, line.lineNo);
        }
    }

    if (!result.tree->Root()) {
        IComponent* root = result.tree->CreateComponent("Page");
        result.tree->SetRoot(root);
    }

    AutoBindEvents(result.tree->Root(), result.code);

    return result;
}

}
}
}