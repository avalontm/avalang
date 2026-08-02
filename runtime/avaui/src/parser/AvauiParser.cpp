#include "parser/AvauiParser.h"

#include <cctype>
#include <cstdlib>
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
    std::string text;    // trimmed, comment-stripped, never empty -- use
                          // for markup-level parsing (component headers,
                          // `key = value` properties, block keywords),
                          // where a bare `#` means "rest of line is a
                          // comment" (matches AvaLang's own COMMENT
                          // token, see StripComment below).
    std::string rawText; // trimmed, but WITHOUT comment-stripping -- use
                          // for `code`/`methods` block bodies. Now that
                          // the markup comment marker is `#` (same as
                          // AvaLang's), this split is no longer strictly
                          // required for `--` (AvaLang's decrement
                          // operator) to survive -- kept anyway as a
                          // defense-in-depth belt-and-suspenders: raw
                          // AvaLang source should never be re-lexed by
                          // this tokenizer's markup rules at all, in
                          // case a future markup feature introduces
                          // another special character.
    int lineNo;
};

// Strips a `#` line comment, respecting simple double-quoted strings
// so a "#" inside a literal (e.g. a hex color like "#f0f0f0") doesn't
// truncate the value. `#` matches AvaLang's own COMMENT token (see
// AvaLang.g4: `COMMENT: '#' ~[\r\n]* -> skip`), so markup and
// code-behind now share one comment convention -- `--` is free to mean
// only "decrement" everywhere, including inside `.avaui` property
// expressions outside of `code`/`methods` blocks, not just inside them
// (the `rawText` split below was the first, narrower fix for that
// ambiguity; switching the marker itself removes it at the root).
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
        size_t firstNonSpace = stripped.find_first_not_of(" \t");
        if (firstNonSpace == std::string::npos) continue; // blank/comment-only
        std::string text = Trim(stripped);
        if (text.empty()) continue;
        // Recover the same line's un-stripped content from the same
        // starting column, for rawText. A line that's blank/comment-only
        // once stripped (firstNonSpace/text above) is still dropped
        // exactly as before -- this only adds a second view of lines
        // that already survive comment-stripping, it doesn't change
        // which lines make it into `lines`.
        std::string rawText = Trim(raw.substr(firstNonSpace));
        lines.push_back({static_cast<int>(firstNonSpace), text, rawText, lineNo});
    }
    return lines;
}

bool IsPropertyLine(const std::string& text) {
    // A component header line is `type` or `type id` (no '='); a
    // property line is always `key = value`. Sufficient to
    // disambiguate at any block level this grammar allows.
    return text.find('=') != std::string::npos;
}

// key = value  ->  {"key", "value"} (both trimmed). Throws ParseError
// if there is no '=' -- callers only invoke this after IsPropertyLine.
std::pair<std::string, std::string> SplitProperty(const Line& line) {
    size_t eq = line.text.find('=');
    std::string key = Trim(line.text.substr(0, eq));
    std::string value = Trim(line.text.substr(eq + 1));
    if (key.empty()) {
        throw ParseError("empty property name", line.lineNo);
    }
    return {key, value};
}

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
    // Anything else (bareword identifier, "a" + b concatenation,
    // unresolved expression) -- Phase 14 does not evaluate
    // expressions/state bindings, so it is kept as opaque text. See
    // AvauiParser.h class comment ("Semantic gaps" / soft fallback).
    return PropertyValue(raw);
}

// A handful of property names the spec/real .avaui files use that
// don't match the name the current (frozen, Phase 13) engine reads --
// see docs/architecture/17_AVAUI_FILE_FORMAT.md's own example
// (`gap`, `value`) vs. ui/src/layout/LayoutProperties.h (`spacing`)
// and ui/src/render_tree/RenderTree.cpp (`text`). Discovered gap,
// called out in docs/AVAUI_FASE14_PARSER.md. Rather than silently
// produce a tree that lays out/renders wrong, both the name
// as-authored and the name the engine actually reads get set.
const std::unordered_map<std::string, std::string> kPropertyAliases = {
    {"gap", "spacing"},
    {"value", "text"},
};

// Spec keywords (lowercase, as written in .avaui) -> the PascalCase
// TypeName LayoutEngine/RenderTree recognize. Anything not in this map
// passes through with only its first letter capitalized -- a
// deliberate soft fallback, not an error: LayoutEngine already treats
// any unrecognized TypeName as a generic Stack-like container (see
// LayoutEngine.h) and RenderTree already treats one as a generic
// Container (see RenderTree.cpp). A component call like `Navbar()`
// keeps its own PascalCase name unchanged.
const std::unordered_map<std::string, std::string> kTypeNames = {
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

std::string CanonicalTypeName(const std::string& asWritten) {
    auto it = kTypeNames.find(asWritten);
    if (it != kTypeNames.end()) return it->second;
    if (asWritten.empty()) return asWritten;
    std::string result = asWritten;
    result[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(result[0])));
    return result;
}

void SetPropertyWithAlias(IComponent* component, const std::string& name,
                           const PropertyValue& value) {
    component->SetProperty(name, value);
    auto alias = kPropertyAliases.find(name);
    if (alias != kPropertyAliases.end() && alias->second != name) {
        component->SetProperty(alias->second, value);
    }
}

// True if `text` is a no-body component reference call, e.g.
// "Navbar()" -- a name immediately followed by "(" ... ")" (any
// argument text between the parens is ignored: passing arguments to
// an imported component is a future-phase concern, see AvauiParser.h
// "Semantic gaps").
bool IsComponentCall(const std::string& text, std::string* nameOut) {
    if (text.empty() || text.back() != ')') return false;
    size_t open = text.find('(');
    if (open == std::string::npos) return false;
    std::string name = Trim(text.substr(0, open));
    if (name.empty()) return false;
    *nameOut = name;
    return true;
}

// Parses an `animate` block's flat `key = value` body (same grammar
// as ParseFlatBlock, but the result is a typed AnimationSpec appended
// to `animations` rather than a generic string map -- see
// AnimationSpec's comment in AvauiParser.h for the recognized keys).
// Unrecognized keys are a soft gap: kept off the struct, simply
// ignored, same "semantic gap" policy as an unresolved `Name()` import
// call elsewhere in this file.
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
        // else: unrecognized key inside `animate` -- soft gap, ignored.
        ++idx;
    }
    throw ParseError("unterminated 'animate' block (missing 'end')", headerIndent);
}

// True if `text` is exactly the `animate` block header (bareword, no
// id, no '=' -- distinguishes it from a property line and from a
// child component header in the same position ParseComponent's body
// loop already disambiguates those two).
bool IsAnimateHeader(const std::string& text) {
    return text == "animate";
}

// Parses one component (and its whole subtree) starting at `lines[idx]`,
// which must be a component header line (not a property, not `end`).
// Advances `idx` past the component's closing `end` (or, for a
// no-body call, past the header line itself).
IComponent* ParseComponent(const std::vector<Line>& lines, size_t& idx, ComponentTree* tree,
                           std::vector<AnimationSpec>* animations) {
    const Line& header = lines[idx];

    std::string callName;
    if (IsComponentCall(header.text, &callName)) {
        ++idx;
        IComponent* comp = tree->CreateComponent(CanonicalTypeName(callName));
        // Unresolved import reference (docs/architecture/17_AVAUI_FILE_FORMAT.md,
        // "Llamadas a Componentes Importados") -- component-call
        // resolution lives in a future phase, out of scope for Phase
        // 14's Layout/Render/Scene/Commands pipeline. Flagged on the
        // node itself so a later phase can find these without a
        // second pass over the source text.
        comp->SetProperty("__unresolvedImportCall", PropertyValue(true));
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
    while (idx < lines.size()) {
        const Line& line = lines[idx];
        if (line.indent <= header.indent) {
            if (line.indent == header.indent && line.text == "end") {
                ++idx;
                return comp;
            }
            throw ParseError("unterminated component block (expected 'end' at column " +
                                  std::to_string(header.indent) + ")",
                              line.lineNo);
        }
        if (IsAnimateHeader(line.text)) {
            int animateIndent = line.indent;
            ++idx;
            ParseAnimateBlock(lines, idx, animateIndent, comp->Id(), animations);
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

// Parses the `view` block body (a sequence of top-level components,
// no properties directly at this level) starting right after the
// `view` header at `headerIndent`. Consumes through the matching
// `end`.
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
        if (IsPropertyLine(line.text)) {
            throw ParseError("unexpected property directly inside 'view' "
                              "(properties belong to a component)",
                              line.lineNo);
        }
        created.push_back(ParseComponent(lines, idx, tree, animations));
    }
    throw ParseError("unterminated 'view' block (missing 'end')", headerIndent);
}

// Parses a flat `key = value` block (properties/state/style) into
// `out`, starting right after the header at `headerIndent`. Consumes
// through the matching `end`.
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

// Parses a raw, un-parsed block (code/methods): everything between the
// header and its matching `end`, reassembled with newlines, indentation
// preserved relative to the block body. This phase does not lex/parse
// AvaLang code-behind -- see class comment in AvauiParser.h.
//
// Uses `line.rawText`, not `line.text`: `text` has already had this
// tokenizer's `--` markup-comment stripped off, which would silently
// truncate an AvaLang statement like `counter--` down to `counter`
// (see the `Line::rawText` comment above for the full story). The
// `line.indent`/`line.text == "end"` structural checks below are
// unaffected either way -- "end" never contains "--".
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

} // namespace

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
            result.routes.push_back(Unquote(rest));
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
        // No `view` block at all -- still return a usable (empty) tree
        // rather than a null root, per the class-comment guarantee.
        IComponent* root = result.tree->CreateComponent("Page");
        result.tree->SetRoot(root);
    }

    return result;
}

} // namespace parser
} // namespace ui
} // namespace avalang