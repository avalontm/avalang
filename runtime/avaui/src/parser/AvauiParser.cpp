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

// Fase 3: line-lookup half of the ported formatError -- same approach as
// frontend_antlr.cpp's getLine (1-based line numbers, strip a trailing
// \r so Windows-authored .avaui files don't leave a stray caret column).
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

}  // namespace

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

bool IsPropertyLine(const std::string& text) {
    size_t eq = text.find('=');
    if (eq == std::string::npos) return false;
    std::string key = Trim(text.substr(0, eq));
    // A real property's left-hand side is exactly one identifier, e.g.
    // `gap`, `source`, `click` -- nothing else, checked directly rather
    // than by ruling out individual disqualifying characters (space,
    // '(', etc. -- new syntax that puts something else before '=' would
    // otherwise need its own carve-out here every time). Anything that
    // isn't a bare identifier -- `ListView source` (component header
    // with inline property), `CartItem(name` (component call with inline
    // args) -- falls through to ParseComponent instead of being consumed
    // as a property here.
    return IsIdentifier(key);
}

IComponent* ParseComponent(const std::vector<Line>& lines, size_t& idx, ComponentTree* tree,
                           std::vector<AnimationSpec>* animations);

bool IsTemplateHeader(const std::string& text) {
    // `template ... end` inside a container (ListView, For, etc.) is a real
    // sub-block, like `state`/`params`/`view`/`properties` elsewhere in the
    // language: everything above it are the container's own properties,
    // everything inside it are the item's template children. This is
    // purely readability sugar -- children added this way end up as direct
    // children of the enclosing component, same as if `template` wasn't
    // there at all.
    return text == "template";
}

void ParseTemplateBlock(IComponent* comp, const Line& header, const std::vector<Line>& lines,
                        size_t& idx, ComponentTree* tree, std::vector<AnimationSpec>* animations) {
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
        IComponent* child = ParseComponent(lines, idx, tree, animations);
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
        // Fase 5: point at the '=' itself -- that's where the reader's eye
        // lands when the key to its left is blank, same convention
        // formatError's caret uses for a "nothing here" error.
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

    *nameOut = name;
    if (argsOut) {
        *argsOut = text.substr(open + 1, text.size() - open - 2);
    }
    return true;
}

// Fase 5: `argsBaseColumn` is the 1-based column where `argsText` starts
// in the original header line (right after the opening '('), so the two
// throws below can point at the actual offending argument instead of
// defaulting to column 0. `start` (byte offset of each part within the
// untrimmed `args`) plus that base gets us close -- same "good enough,
// cheap" tier as the rest of Fase 5, not a full token scan.
void ParseComponentCallArgs(const std::string& argsText, IComponent* comp, int lineNo,
                             int argsBaseColumn) {
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
        SetPropertyWithAlias(comp, key, InferValue(value));
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
    // Fase 5: previously threw `headerIndent` as the *line* argument here
    // (a pre-existing bug -- it reported the block's indent column as if
    // it were a line number). headerLine is the animate header's real
    // line, threaded in from the call site below.
    throw ParseError("unterminated 'animate' block (missing 'end')", headerLine,
                      headerIndent + 1);
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
            ParseTemplateBlock(comp, templateHeader, lines, idx, tree, animations);
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
    throw ParseError("unterminated component block (missing 'end')", header.lineNo,
                      header.indent + 1);
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

        // Fase 5: 1-based column right after the '(' -- header.indent is
        // the column the header line starts at, '(' is somewhere in
        // header.text, args start one char past it.
        size_t openParen = header.text.find('(');
        int argsBaseColumn = header.indent + static_cast<int>(openParen) + 2;
        ParseComponentCallArgs(callArgs, comp, header.lineNo, argsBaseColumn);
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
            // Inline property right on the header line, e.g.
            // `ListView source = cart` -- lets ListView (and any other
            // component) bind a property without a separate body line.
            std::string key = Trim(rest.substr(0, eq));
            std::string value = Trim(rest.substr(eq + 1));
            if (!key.empty() && !value.empty()) {
                SetPropertyWithAlias(comp, key, InferValue(value));
            }
        } else {
            // `Type id` form, e.g. `Button submitBtn`.
            comp->SetProperty("id", PropertyValue(rest));
        }
    }

    ++idx;
    ParseComponentBody(comp, header, lines, idx, tree, animations);
    return comp;
}

std::vector<IComponent*> ParseViewBody(const std::vector<Line>& lines, size_t& idx,
                                        int headerIndent, int headerLine, ComponentTree* tree,
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
                              line.lineNo, line.indent + 1);
        }
        if (IsPropertyLine(line.text) && !IsIfHeader(line.text, nullptr) &&
            !IsForHeader(line.text, nullptr, nullptr)) {
            throw ParseError("unexpected property directly inside 'view' "
                              "(properties belong to a component)",
                              line.lineNo, line.indent + 1);
        }
        created.push_back(ParseComponent(lines, idx, tree, animations));
    }
    // Fase 5: same pre-existing headerIndent-as-line bug as
    // ParseAnimateBlock, fixed the same way via a threaded headerLine.
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

// Fase 2: the actual line-by-line parse, unaware of sourcePath -- every
// `throw ParseError(msg, line.lineNo)` call site inside this function
// (and the helpers it calls) keeps constructing ParseError the same way
// it always has, with source left as "". AvauiParser::Parse below is the
// only place that knows sourcePath, so it's the only place that needs to
// touch it: one catch/relabel/rethrow at the boundary instead of editing
// every one of the ~20 existing throw sites.
ParsedAvaui ParseImpl(const std::string& source) {
    std::vector<Line> lines = Tokenize(source);

    ParsedAvaui result;
    result.tree = ComponentTree::Create();
    bool sawExtends = false;

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
            ++idx;
            std::vector<IComponent*> topLevel =
                ParseViewBody(lines, idx, 0, headerLine, result.tree.get(), &result.animations);

            IComponent* root = result.tree->CreateComponent("Page");
            for (IComponent* child : topLevel) {
                root->AddChild(child);
            }
            result.tree->SetRoot(root);
        } else {
            throw ParseError("unknown top-level block: " + keyword, line.lineNo,
                              line.indent + 1);
        }
    }

    if (!result.tree->Root()) {
        IComponent* root = result.tree->CreateComponent("Page");
        result.tree->SetRoot(root);
    }

    AutoBindEvents(result.tree->Root(), result.code);

    return result;
}

}  // namespace

ParsedAvaui AvauiParser::Parse(const std::string& source, const std::string& sourcePath) {
    try {
        return ParseImpl(source);
    } catch (const ParseError& e) {
        // Relabel with sourcePath, preserving the original message/line/
        // column. Only relabel if we actually have a path to attach and
        // the error doesn't already carry one -- a nested Parse() call
        // (e.g. Fase 6's imported-component parse) will already have
        // stamped its own file by the time it gets here, and that's the
        // one we want to keep, not overwrite with the outer caller's path.
        if (sourcePath.empty() || !e.Source().empty()) {
            throw;
        }
        throw ParseError(e.RawMessage(), e.Line(), e.Column(), sourcePath);
    }
}

}
}
}