#include "parser/AvauiParser.h"
#include "parser/AvauiPropertyCoercion.h"
#include "events/AutoBind.h"

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

        // Indentation column, tab-aware: a raw find_first_not_of(" \t")
        // treats a tab and a space as the same one column, so a file
        // that mixes an 8-space-indented property with a 2-tab-indented
        // sibling (e.g. an editor auto-indenting one line differently)
        // silently computes a SMALLER column for the tabbed line than
        // its space-indented siblings, even though both look like "one
        // level deeper" visually. That misindented line then reads as
        // closing the enclosing block early -- ParseComponent's body
        // loop sees `line.indent <= header.indent` and throws
        // "unterminated component block (expected 'end')", which
        // UiComponentResolver::LoadComponent silently swallows (`catch
        // (const ParseError&) { return nullptr; }`), leaving the whole
        // imported component unresolved -- it just doesn't render, no
        // visible error anywhere. Expanding each tab to a fixed 4-column
        // stride (this codebase's own nesting width, see every `row`/
        // `column`/property indent throughout the samples) instead of
        // counting it as 1 column makes indentation consistent whether
        // an editor happens to save a given line with tabs or spaces.
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
        if (firstNonSpace == stripped.size()) continue; // blank/comment-only

        std::string text = Trim(stripped);
        if (text.empty()) continue;
        // Recover the same line's un-stripped content from the same
        // starting column, for rawText. A line that's blank/comment-only
        // once stripped (firstNonSpace/text above) is still dropped
        // exactly as before -- this only adds a second view of lines
        // that already survive comment-stripping, it doesn't change
        // which lines make it into `lines`.
        std::string rawText = Trim(raw.substr(firstNonSpace));
        lines.push_back({indentColumn, text, rawText, lineNo});
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

// Unquote/LooksLikeNumber/InferValue/CanonicalTypeName/
// SetPropertyWithAlias (y los mapas kPropertyAliases/kTypeNames que
// usaban) fueron extraídos a parser/AvauiPropertyCoercion.h/.cpp
// (Fase 1 de AVAUI_DESIGNER_REAL_RENDER_PLAN.md) para que Ava Studio's
// live_render_bridge use exactamente la misma lógica de inferencia de
// tipo/alias sin duplicarla. Siguen resolviendo aquí sin calificar
// porque ambos archivos comparten el namespace avalang::ui::parser.

// True if `text` is a no-body component reference call, e.g.
// "Navbar()" or "Navbar(height = 64, class = \"dark\")" -- a name
// immediately followed by "(" ... ")". `argsOut`, if non-null, receives
// the raw text between the parens (empty for a plain "Navbar()") so
// the caller can parse it as call-site property overrides -- see
// ParseComponentCallArgs below.
bool IsComponentCall(const std::string& text, std::string* nameOut, std::string* argsOut = nullptr) {
    if (text.empty() || text.back() != ')') return false;
    size_t open = text.find('(');
    if (open == std::string::npos) return false;
    std::string name = Trim(text.substr(0, open));
    if (name.empty()) return false;
    *nameOut = name;
    if (argsOut) {
        *argsOut = text.substr(open + 1, text.size() - open - 2);
    }
    return true;
}

// Splits a component call's argument text -- e.g. `height = 64, class
// = "dark"` -- on top-level commas (commas inside a double-quoted
// value don't split, so `label = "A, B"` survives intact) and sets
// each `key = value` pair on `comp` via the same
// InferValue/SetPropertyWithAlias path a normal property line inside
// a component body uses (see ParseComponent's IsPropertyLine branch
// below), so `Navbar(height = 64)` behaves exactly like a `height =
// 64` line would inside a real (non-call) component -- same type
// inference, same "gap"->"spacing"/"value"->"text" aliasing. A bare
// `Navbar()` has empty argText and this is a no-op, matching the old
// "no-body call" behavior exactly.
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
    std::string callArgs;
    if (IsComponentCall(header.text, &callName, &callArgs)) {
        ++idx;
        IComponent* comp = tree->CreateComponent(CanonicalTypeName(callName));
        // Unresolved import reference (docs/architecture/17_AVAUI_FILE_FORMAT.md,
        // "Llamadas a Componentes Importados") -- component-call
        // resolution lives in a future phase, out of scope for Phase
        // 14's Layout/Render/Scene/Commands pipeline. Flagged on the
        // node itself so a later phase can find these without a
        // second pass over the source text.
        comp->SetProperty("__unresolvedImportCall", PropertyValue(true));
        // Call-site overrides (e.g. `height = 64` in `Navbar(height =
        // 64)`) -- parsed onto this same node so a later resolution
        // phase (e.g. avahost's UiComponentResolver) can read them off
        // the call node itself, same place it already looks for
        // "__unresolvedImportCall".
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
        IComponent* root = result.tree->CreateComponent("Page");
        result.tree->SetRoot(root);
    }

    AutoBindEvents(result.tree->Root(), result.code);

    return result;
}

} // namespace parser
} // namespace ui
} // namespace avalang