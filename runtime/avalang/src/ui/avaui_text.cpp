#include "ui/avaui_text.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <regex>
#include <sstream>
#include <unordered_set>

namespace ava {
namespace ui {

namespace {

// --- Small string helpers -------------------------------------------------

std::vector<std::string> SplitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::string current;
    for (char c : text) {
        if (c == '\n') {
            if (!current.empty() && current.back() == '\r') current.pop_back();
            lines.push_back(current);
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty() && current.back() == '\r') current.pop_back();
    lines.push_back(current); // last line, even if empty (no trailing \n case is fine too)
    return lines;
}

std::string Trim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
    return s.substr(start, end - start);
}

std::string LowerCopy(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

// Leading-whitespace count, same convention as
// AvaComponentParser.cs::GetLineIndent (TakeWhile(IsWhiteSpace).Count()).
size_t IndentOf(const std::string& line) {
    size_t i = 0;
    while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) ++i;
    return i;
}

bool IsBlank(const std::string& line) { return Trim(line).empty(); }

// --- Reserved words, shared with the .NET reference parser ----------------
// (AvaComponentParser.cs::SectionKeywords / BuiltinTypes / EventProps).

bool IsSectionKeyword(const std::string& lower) {
    static const std::unordered_set<std::string> kKeywords = {
        // Canonical (2026 AvaUI architecture revision -- see
        // avaui_text.h header comment and
        // docs/architecture/17_AVAUI_FILE_FORMAT.md):
        "properties", "state", "view", "code", "style",
        // Legacy aliases, still parsed for backward compatibility with
        // existing .avaui files -- see the "metadata"/"code" handling
        // in ParseAvauiText below. WriteAvauiText never emits these.
        "metadata", "methods",
        // Reserved, not acted on yet.
        "import", "imports", "services", "lifecycle",
    };
    return kKeywords.count(lower) != 0;
}

bool IsControlKeyword(const std::string& lower) {
    static const std::unordered_set<std::string> kKeywords = {
        "if", "while", "for", "func", "return", "var", "print", "end", "extends",
    };
    return kKeywords.count(lower) != 0;
}

// Canonical list of recognized event-prop names -- must stay in sync
// with studio/src/design/avaui_text.cpp's own EventPropertyNames()
// (that file keeps its own copy since IsEventPropertyName is part of
// its public API too -- see that file's comment).
const std::unordered_set<std::string>& EventPropNames() {
    static const std::unordered_set<std::string> kEventProps = {
        "click", "onclick", "onchange", "oninput", "onfocus", "onblur",
        "onkeydown", "onkeyup", "onmouseenter", "onmouseleave",
        "onsubmit", "onload", "onerror",
    };
    return kEventProps;
}

// --- Automatic event binding (naming convention) ---------------------
// See avaui_text.h header comment, "Automatic event binding". No
// config needed from the user: if a component has an `id` and `code`
// defines a function named `On` + PascalCase(id) + PascalCase(event),
// it's wired up as that event's handler, unless an explicit event prop
// already claimed the same event name in `view`.

std::string PascalCase(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    bool start_of_word = true;
    for (char c : s) {
        if (c == '_' || c == '-' || std::isspace(static_cast<unsigned char>(c))) {
            start_of_word = true;
            continue;
        }
        if (start_of_word) {
            out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
            start_of_word = false;
        } else {
            out.push_back(c);
        }
    }
    return out;
}

// The one or two events worth guessing a handler name for, per
// built-in component type -- deliberately a short list of the
// interaction every one of these types is actually used for, not the
// full EventPropNames() set (guessing a handler for every possible
// event on every component would mean scanning `code` for a lot of
// names that will almost never exist).
const std::vector<std::string>& DefaultEventsForType(const std::string& lower_type) {
    static const std::vector<std::string> kClick = {"click"};
    static const std::vector<std::string> kChange = {"change"};
    static const std::vector<std::string> kSubmit = {"submit"};
    static const std::vector<std::string> kNone = {};
    if (lower_type == "button") return kClick;
    if (lower_type == "input" || lower_type == "textfield" || lower_type == "textbox" ||
        lower_type == "checkbox" || lower_type == "select" || lower_type == "dropdown" ||
        lower_type == "slider" || lower_type == "switch" || lower_type == "toggle") {
        return kChange;
    }
    if (lower_type == "form") return kSubmit;
    return kNone;
}

// `code`'s func names, harvested once per document (not per node) --
// AutoBindEvents just needs to know which names exist, not their
// bodies (those still run wherever the language's own compiler parses
// them; this file never executes AvaLang code).
std::unordered_set<std::string> CollectFunctionNames(const std::string& code_text) {
    static const std::regex kFuncRe(R"(^\s*func\s+(\w+)\s*\()");
    std::unordered_set<std::string> names;
    for (const auto& line : SplitLines(code_text)) {
        std::smatch m;
        if (std::regex_search(line, m, kFuncRe)) {
            names.insert(m[1].str());
        }
    }
    return names;
}

void AutoBindEvents(Component& node, const std::unordered_set<std::string>& function_names) {
    if (!node.GetId().empty()) {
        const std::string pascal_id = PascalCase(node.GetId());
        for (const std::string& event : DefaultEventsForType(LowerCopy(node.GetType()))) {
            if (node.HasEvent(event)) continue; // explicit binding in `view` wins
            const std::string candidate = "On" + pascal_id + PascalCase(event);
            if (function_names.count(candidate)) {
                node.SetEvent(event, Value::String(candidate));
            }
        }
    }
    for (const auto& child : node.GetChildren()) {
        AutoBindEvents(*child, function_names);
    }
}

// --- Property value quoting (see avaui_text.h header comment) -------------

std::string UnquoteIfString(const std::string& raw) {
    std::string v = Trim(raw);
    if (v.size() >= 2 && v.front() == '"' && v.back() == '"') {
        // Starting and ending with '"' isn't enough on its own -- an
        // expression like `"No encontramos '" + request.path + "'"`
        // (string concatenation bookended by two separate literals)
        // also starts and ends with '"', but is NOT one single string
        // literal. Naively stripping just the first/last char used to
        // corrupt it into `No encontramos '" + request.path + "'` --
        // quotes and all -- which then failed to compile as an
        // expression and got displayed raw instead of evaluated.
        // Only treat this as one literal when the opening and closing
        // '"' actually pair up: no unescaped '"' anywhere strictly
        // between them.
        bool isSingleLiteral = true;
        for (size_t i = 1; i + 1 < v.size(); ++i) {
            if (v[i] == '"' && v[i - 1] != '\\') {
                isSingleLiteral = false;
                break;
            }
        }
        if (isSingleLiteral) {
            std::string inner = v.substr(1, v.size() - 2);
            std::string out;
            out.reserve(inner.size());
            for (size_t i = 0; i < inner.size(); ++i) {
                if (inner[i] == '\\' && i + 1 < inner.size() && inner[i + 1] == '"') {
                    out.push_back('"');
                    ++i;
                } else {
                    out.push_back(inner[i]);
                }
            }
            return out;
        }
    }
    return v; // number, bool, bare identifier, or a fuller expression -- kept as-is
}

bool LooksNumeric(const std::string& v) {
    if (v.empty()) return false;
    size_t i = 0;
    if (v[i] == '+' || v[i] == '-') ++i;
    if (i >= v.size()) return false;
    bool has_digits = false;
    bool has_dot = false;
    for (; i < v.size(); ++i) {
        if (std::isdigit(static_cast<unsigned char>(v[i]))) {
            has_digits = true;
        } else if (v[i] == '.' && !has_dot) {
            has_dot = true;
        } else {
            return false;
        }
    }
    return has_digits;
}

std::string WritePropertyValue(const std::string& value) {
    if (value == "true" || value == "false") return value;
    if (LooksNumeric(value)) return value;
    std::string escaped;
    escaped.reserve(value.size() + 2);
    for (char c : value) {
        if (c == '"' || c == '\\') escaped.push_back('\\');
        escaped.push_back(c);
    }
    return "\"" + escaped + "\"";
}

// Extracts the raw display string back out of a property Value. Every
// value this parser ever stores is a Value::String holding that exact
// text (see avaui_text.h) -- Number/Bool branches exist defensively in
// case a caller sets a property directly through the C API with a
// non-string Value instead of going through this parser.
std::string ValueToDisplayString(const Value& v) {
    switch (v.type) {
        case ValueType::String:
            return v.obj ? static_cast<StringObj*>(v.obj)->data : "";
        case ValueType::Bool:
            return v.b ? "true" : "false";
        case ValueType::Number: {
            std::ostringstream oss;
            oss << v.n;
            return oss.str();
        }
        default:
            return "";
    }
}

// --- properties/state block parsing (flat "key = value" lines) ------------

std::vector<std::pair<std::string, std::string>> ParsePropertyLines(
    const std::vector<std::string>& lines) {
    static const std::regex kPropRe(R"(^(\w+)\s*=\s*(.+)$)");
    std::vector<std::pair<std::string, std::string>> rows;
    for (const auto& raw_line : lines) {
        std::string line = Trim(raw_line);
        if (line.empty()) continue;
        std::smatch m;
        if (std::regex_match(line, m, kPropRe)) {
            rows.emplace_back(m[1].str(), UnquoteIfString(m[2].str()));
        }
    }
    return rows;
}

// --- `view` block parsing: indentation -> Component tree -------------------
// Direct port of AvaComponentParser.cs::TryParseComponent / ParseView --
// see avaui_text.h for the grammar this recognizes.

std::vector<std::shared_ptr<Component>> ParseViewLines(const std::vector<std::string>& lines);

std::shared_ptr<Component> TryParseComponent(const std::vector<std::string>& lines, size_t& index) {
    static const std::regex kCallRe(R"(^(\w+)\s*\(\s*\)\s*$)");
    static const std::regex kBareWordRe(R"(^(\w+)$)");
    // `type Id` on one line -- e.g. `button Guardar` -- a shorthand for
    // `button` + `id = "Guardar"`, so a hand-written file can name a
    // component without a separate properties line for the common
    // case (see avaui_text.h header comment, "Automatic event
    // binding": this is also what makes `id` available for
    // AutoBindEvents to key off of without extra typing). Only two
    // bare words, no `=`, so this never shadows a `key = value`
    // property line or a `Type()` call.
    static const std::regex kTypeAndIdRe(R"(^(\w+)\s+(\w+)$)");
    static const std::regex kPropRe(R"(^(\w+)\s*=\s*(.+)$)");

    const std::string line = Trim(lines[index]);

    std::smatch call_match;
    if (std::regex_match(line, call_match, kCallRe)) {
        const std::string type_original = call_match[1].str();
        if (IsControlKeyword(LowerCopy(type_original))) return nullptr;
        // Deliberately NOT `++index` here: every caller (ParseViewLines at
        // top level, and the child-parsing loop below) already does
        // `j/i = returned_index + 1` after this returns, expecting
        // `index` to land on the *last line this call consumed* (the
        // call line itself, since a call-form has no body/`end`) --
        // same convention the block-form branch below follows (it sets
        // index to the `end` line, not past it). Incrementing here as
        // well used to double-advance and skip the next sibling line
        // whenever a call-form component had no blank line after it.
        return std::make_shared<Component>(type_original);
    }

    std::string type_original;
    std::string inline_id;
    std::smatch bare_match;
    std::smatch type_id_match;
    if (std::regex_match(line, bare_match, kBareWordRe)) {
        type_original = bare_match[1].str();
    } else if (std::regex_match(line, type_id_match, kTypeAndIdRe)) {
        type_original = type_id_match[1].str();
        inline_id = type_id_match[2].str();
    } else {
        return nullptr;
    }
    if (IsControlKeyword(LowerCopy(type_original))) return nullptr;

    auto node = std::make_shared<Component>(type_original);
    if (!inline_id.empty()) node->SetId(inline_id);

    const size_t start_indent = IndentOf(lines[index]);
    size_t j = index + 1;

    while (j < lines.size()) {
        const std::string& raw_line = lines[j];
        const std::string trimmed = Trim(raw_line);

        if (trimmed == "end") {
            if (IndentOf(raw_line) == start_indent) {
                index = j;
                return node;
            }
            ++j;
            continue;
        }

        if (trimmed.empty()) {
            ++j;
            continue;
        }

        if (IndentOf(raw_line) < start_indent) {
            break; // unterminated block -- stop here, same as the .NET reference
        }

        const std::string trimmed_lower = LowerCopy(trimmed);
        if (IsControlKeyword(trimmed_lower) || IsSectionKeyword(trimmed_lower)) {
            ++j;
            continue;
        }

        std::smatch prop_match;
        if (std::regex_match(trimmed, prop_match, kPropRe)) {
            const std::string key = prop_match[1].str();
            const std::string raw_value = prop_match[2].str();
            const std::string key_lower = LowerCopy(key);
            if (key_lower == "id") {
                node->SetId(UnquoteIfString(raw_value));
            } else if (IsEventPropertyName(key_lower)) {
                node->SetEvent(key, Value::String(Trim(raw_value)));
            } else {
                node->SetProperty(key, Value::String(UnquoteIfString(raw_value)));
            }
            ++j;
            continue;
        }

        size_t child_index = j;
        std::shared_ptr<Component> child = TryParseComponent(lines, child_index);
        if (child) {
            node->AddChild(child);
            j = child_index + 1;
        } else {
            ++j;
        }
    }

    index = (j > 0) ? j - 1 : 0;
    return node;
}

std::vector<std::shared_ptr<Component>> ParseViewLines(const std::vector<std::string>& lines) {
    std::vector<std::shared_ptr<Component>> top_level;
    size_t i = 0;
    while (i < lines.size()) {
        const std::string trimmed = Trim(lines[i]);
        if (trimmed.empty() || trimmed == "end") {
            ++i;
            continue;
        }
        const std::string lower = LowerCopy(trimmed);
        if (IsControlKeyword(lower) || IsSectionKeyword(lower)) {
            ++i;
            continue;
        }
        size_t idx = i;
        std::shared_ptr<Component> node = TryParseComponent(lines, idx);
        if (node) {
            top_level.push_back(std::move(node));
            i = idx + 1;
        } else {
            ++i;
        }
    }
    return top_level;
}

// --- Top-level section splitting -------------------------------------------

struct RawSection {
    std::string keyword; // lowercased
    std::vector<std::string> content_lines;
};

std::vector<RawSection> SplitTopLevelSections(const std::vector<std::string>& lines) {
    std::vector<RawSection> sections;
    size_t i = 0;
    while (i < lines.size()) {
        const std::string trimmed = Trim(lines[i]);
        const std::string lower = LowerCopy(trimmed);
        if (IndentOf(lines[i]) == 0 && IsSectionKeyword(lower) && lower != "import") {
            size_t j = i + 1;
            while (j < lines.size()) {
                const std::string t2 = Trim(lines[j]);
                const std::string lower2 = LowerCopy(t2);
                if (IndentOf(lines[j]) == 0 && (t2 == "end" || IsSectionKeyword(lower2))) break;
                ++j;
            }
            RawSection section;
            section.keyword = lower;
            section.content_lines.assign(lines.begin() + static_cast<long>(i) + 1,
                                          lines.begin() + static_cast<long>(j));
            sections.push_back(std::move(section));
            // Skip the closing `end` if there was one; otherwise `j` is
            // already sitting on the next section keyword (or EOF), so
            // the outer loop picks it up next iteration without
            // re-scanning what we already consumed.
            if (j < lines.size() && Trim(lines[j]) == "end" && IndentOf(lines[j]) == 0) {
                i = j + 1;
            } else {
                i = j;
            }
        } else {
            ++i; // blank line, comment, `import "..."`, or stray text -- not a section header
        }
    }
    return sections;
}

// `import components.navbar` -- a dotted path, unquoted, resolved
// from the project root by AvaHost/Studio (path segments joined with
// "/", ".avaui" appended -- e.g. "components.navbar" ->
// "<projectRoot>/components/navbar.avaui"). Distinct from AvaLang's
// real `import module.name` compiled statement (grammar
// `importStatement`, resolved through ava_import()) -- this is a
// .avaui-file-only convention for pulling in another .avaui as a
// component, parsed here as plain text just like `extends`.
std::vector<std::string> ParseImportLines(const std::vector<std::string>& lines) {
    static const std::regex kImportRe(R"re(^import\s+([A-Za-z_][A-Za-z0-9_]*(?:\.[A-Za-z_][A-Za-z0-9_]*)*)\s*$)re");
    std::vector<std::string> imports;
    for (const auto& raw_line : lines) {
        if (IndentOf(raw_line) != 0) continue;
        std::smatch m;
        const std::string trimmed = Trim(raw_line);
        if (std::regex_match(trimmed, m, kImportRe)) {
            imports.push_back(m[1].str());
        }
    }
    return imports;
}

// `extends layouts.main` -- a dotted path, unquoted, same convention
// and resolution rule as `import` above (see comment there). Always
// includes the folder segment (e.g. "layouts") -- there's no bare
// `extends main` shorthand. An .avaui file only ever extends one
// layout.
std::string ParseExtendsLine(const std::vector<std::string>& lines) {
    static const std::regex kExtendsRe(R"re(^extends\s+([A-Za-z_][A-Za-z0-9_]*(?:\.[A-Za-z_][A-Za-z0-9_]*)*)\s*$)re");
    for (const auto& raw_line : lines) {
        if (IndentOf(raw_line) != 0) continue;
        std::smatch m;
        const std::string trimmed = Trim(raw_line);
        if (std::regex_match(trimmed, m, kExtendsRe)) {
            return m[1].str();
        }
    }
    return "";
}

// `route "/path/{param}"` -- one or more top-level lines. Port of
// AvaComponentParser.cs's routesMatch/paramRegex: each `{name}` /
// `{name?}` / `{name:constraint}` segment in the template becomes a
// RouteParameter.
std::vector<RouteDeclaration> ParseRouteLines(const std::vector<std::string>& lines) {
    static const std::regex kRouteRe(R"re(^route\s+"([^"]*)"\s*$)re");
    static const std::regex kParamRe(R"(\{(\w+)(\?)?(?::(\w+))?\})");
    std::vector<RouteDeclaration> routes;
    for (const auto& raw_line : lines) {
        if (IndentOf(raw_line) != 0) continue;
        const std::string trimmed = Trim(raw_line);
        std::smatch m;
        if (!std::regex_match(trimmed, m, kRouteRe)) continue;

        RouteDeclaration route;
        route.route_template = m[1].str();

        auto begin = std::sregex_iterator(route.route_template.begin(), route.route_template.end(), kParamRe);
        auto end = std::sregex_iterator();
        for (auto it = begin; it != end; ++it) {
            const std::smatch& pm = *it;
            RouteParameter param;
            param.name = pm[1].str();
            param.kind = pm[2].matched ? RouteParameterKind::Optional : RouteParameterKind::Required;
            param.constraint = pm[3].matched ? pm[3].str() : "";
            route.parameters.push_back(std::move(param));
        }
        routes.push_back(std::move(route));
    }
    return routes;
}

std::string JoinTrimmedBlock(const std::vector<std::string>& lines) {
    // Drop leading/trailing blank lines but keep original indentation on
    // everything in between -- this is shown verbatim in a host's Code
    // view (see avaui_text.h), so preserving whatever indentation the
    // user (or a previous WriteAvauiText pass) put there matters more
    // than normalizing it.
    size_t begin = 0;
    while (begin < lines.size() && IsBlank(lines[begin])) ++begin;
    size_t end = lines.size();
    while (end > begin && IsBlank(lines[end - 1])) --end;
    if (begin >= end) return "";
    std::ostringstream out;
    for (size_t i = begin; i < end; ++i) {
        out << lines[i];
        if (i + 1 < end) out << "\n";
    }
    return out.str();
}

void WriteNode(const Component& node, int indent, std::ostringstream& out) {
    const std::string pad(static_cast<size_t>(indent) * 4, ' ');

    const auto& props = node.GetAllProperties();
    const auto& events = node.GetAllEvents();
    const auto& children = node.GetChildren();

    // A node is written as a bare `Type()` call (no properties/events/
    // children) only when it's an external-component reference by
    // convention (PascalCase, e.g. `Navbar()`) -- see avaui_text.h.
    // Lowercase built-in types (`button`, `column`, ...) always use the
    // block form, even with zero properties (`spacer\nend`).
    const bool is_call_form = !node.GetType().empty() &&
                               std::isupper(static_cast<unsigned char>(node.GetType()[0])) &&
                               node.GetId().empty() && props.empty() && events.empty() &&
                               children.empty();

    if (is_call_form) {
        out << pad << node.GetType() << "()\n";
        return;
    }

    out << pad << node.GetType() << "\n";
    const std::string inner_pad(static_cast<size_t>(indent + 1) * 4, ' ');

    if (!node.GetId().empty()) {
        out << inner_pad << "id = " << WritePropertyValue(node.GetId()) << "\n";
    }
    for (const auto& [key, value] : props) {
        out << inner_pad << key << " = " << WritePropertyValue(ValueToDisplayString(value)) << "\n";
    }
    for (const auto& [key, value] : events) {
        out << inner_pad << key << " = " << ValueToDisplayString(value) << "\n";
    }
    if (!children.empty() && (!node.GetId().empty() || !props.empty() || !events.empty())) {
        out << "\n"; // cosmetic spacing between props and children, matches the plan's example
    }
    for (const auto& child : children) {
        WriteNode(*child, indent + 1, out);
    }
    out << pad << "end\n";
}

// --- Minimal JSON string escaping/parsing for the C API boundary ----------

void AppendJsonString(std::ostringstream& out, const std::string& s) {
    out << '"';
    for (char c : s) {
        switch (c) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default: out << c; break;
        }
    }
    out << '"';
}

// Parses one JSON string literal starting at text[i] == '"'. Advances
// `i` to just past the closing quote. Returns the unescaped content.
// Deliberately minimal -- see avaui_text.h's note on why a full JSON
// library isn't warranted here.
std::string ParseJsonStringAt(const std::string& text, size_t& i) {
    std::string out;
    if (i >= text.size() || text[i] != '"') return out;
    ++i; // opening quote
    while (i < text.size() && text[i] != '"') {
        if (text[i] == '\\' && i + 1 < text.size()) {
            char next = text[i + 1];
            switch (next) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                default: out.push_back(next); break;
            }
            i += 2;
        } else {
            out.push_back(text[i]);
            ++i;
        }
    }
    if (i < text.size() && text[i] == '"') ++i; // closing quote
    return out;
}

} // namespace

bool IsEventPropertyName(const std::string& name) { return EventPropNames().count(LowerCopy(name)) != 0; }

ParsedAvaui ParseAvauiText(const std::string& text) {
    const std::vector<std::string> lines = SplitLines(text);

    ParsedAvaui result;
    result.root = std::make_shared<Component>("page");

    for (const RawSection& section : SplitTopLevelSections(lines)) {
        // "metadata" is the retired name for this same block --
        // accepted here so existing .avaui files keep loading; see
        // avaui_text.h header comment.
        if (section.keyword == "properties" || section.keyword == "metadata") {
            for (auto& [key, value] : ParsePropertyLines(section.content_lines)) {
                if (LowerCopy(key) == "id") {
                    result.root->SetId(value);
                } else {
                    result.root->SetProperty(key, Value::String(value));
                }
            }
        } else if (section.keyword == "state") {
            result.state = ParsePropertyLines(section.content_lines);
        } else if (section.keyword == "style") {
            result.style = ParsePropertyLines(section.content_lines);
        } else if (section.keyword == "view") {
            for (auto& child : ParseViewLines(section.content_lines)) {
                result.root->AddChild(child);
            }
        } else if (section.keyword == "code" || section.keyword == "methods") {
            // "methods" is the retired name for this same block, same
            // backward-compatibility treatment as "metadata" above.
            result.methods_text = JoinTrimmedBlock(section.content_lines);
        }
        // "imports"/"services"/"lifecycle" are reserved but not acted on
        // yet -- see avaui_text.h and 08_DESIGNER_VIEW_PLAN.md section 2
        // point 3/section 9.2's note on component-import resolution.
    }

    result.imports = ParseImportLines(lines);
    result.extends = ParseExtendsLine(lines);
    result.routes = ParseRouteLines(lines);

    // Fill in any `click = ...`-style handler that a hand-written file
    // left implicit, purely by matching `code`'s function names
    // against the naming convention -- see AutoBindEvents above. Runs
    // after the whole tree + `code` block are both known, and never
    // overrides an explicit event prop already parsed into the tree.
    AutoBindEvents(*result.root, CollectFunctionNames(result.methods_text));

    return result;
}

std::string WriteAvauiText(const Component& root,
                            const std::vector<std::pair<std::string, std::string>>& state,
                            const std::vector<std::string>& imports,
                            const std::string& methods_text,
                            const std::string& extends,
                            const std::vector<RouteDeclaration>& routes,
                            const std::vector<std::pair<std::string, std::string>>& style) {
    std::ostringstream out;

    if (!extends.empty()) {
        out << "extends " << extends << "\n\n";
    }

    for (const auto& route : routes) {
        out << "route " << WritePropertyValue(route.route_template) << "\n";
    }
    if (!routes.empty()) out << "\n";

    for (const auto& imp : imports) {
        out << "import " << imp << "\n";
    }
    if (!imports.empty()) out << "\n";

    const auto& root_props = root.GetAllProperties();
    const bool has_page_properties = !root.GetId().empty() || !root_props.empty();
    if (has_page_properties) {
        out << "properties\n";
        if (!root.GetId().empty()) out << "    id = " << WritePropertyValue(root.GetId()) << "\n";
        for (const auto& [key, value] : root_props) {
            out << "    " << key << " = " << WritePropertyValue(ValueToDisplayString(value)) << "\n";
        }
        out << "end\n\n";
    }

    if (!state.empty()) {
        out << "state\n";
        for (const auto& [key, value] : state) {
            out << "    " << key << " = " << WritePropertyValue(value) << "\n";
        }
        out << "end\n\n";
    }

    out << "view\n";
    for (const auto& child : root.GetChildren()) {
        WriteNode(*child, /*indent=*/1, out);
    }
    out << "end\n";

    if (!methods_text.empty()) {
        out << "\ncode\n" << methods_text << "\nend\n";
    }

    if (!style.empty()) {
        out << "\nstyle\n";
        for (const auto& [key, value] : style) {
            out << "    " << key << " = " << WritePropertyValue(value) << "\n";
        }
        out << "end\n";
    }

    return out.str();
}

std::string StateToJson(const std::vector<std::pair<std::string, std::string>>& state) {
    std::ostringstream out;
    out << "{";
    bool first = true;
    for (const auto& [key, value] : state) {
        if (!first) out << ",";
        first = false;
        AppendJsonString(out, key);
        out << ":";
        AppendJsonString(out, value);
    }
    out << "}";
    return out.str();
}

std::vector<std::pair<std::string, std::string>> StateFromJson(const std::string& json) {
    std::vector<std::pair<std::string, std::string>> result;
    size_t i = 0;
    while (i < json.size() && json[i] != '{') ++i;
    if (i >= json.size()) return result;
    ++i; // '{'
    while (i < json.size()) {
        while (i < json.size() && (json[i] == ',' || std::isspace(static_cast<unsigned char>(json[i])))) ++i;
        if (i >= json.size() || json[i] == '}') break;
        std::string key = ParseJsonStringAt(json, i);
        while (i < json.size() && (json[i] == ':' || std::isspace(static_cast<unsigned char>(json[i])))) ++i;
        std::string value = ParseJsonStringAt(json, i);
        result.emplace_back(std::move(key), std::move(value));
    }
    return result;
}

std::string ImportsToJson(const std::vector<std::string>& imports) {
    std::ostringstream out;
    out << "[";
    bool first = true;
    for (const auto& imp : imports) {
        if (!first) out << ",";
        first = false;
        AppendJsonString(out, imp);
    }
    out << "]";
    return out.str();
}

std::vector<std::string> ImportsFromJson(const std::string& json) {
    std::vector<std::string> result;
    size_t i = 0;
    while (i < json.size() && json[i] != '[') ++i;
    if (i >= json.size()) return result;
    ++i; // '['
    while (i < json.size()) {
        while (i < json.size() && (json[i] == ',' || std::isspace(static_cast<unsigned char>(json[i])))) ++i;
        if (i >= json.size() || json[i] == ']') break;
        result.push_back(ParseJsonStringAt(json, i));
    }
    return result;
}

std::string RoutesToJson(const std::vector<RouteDeclaration>& routes) {
    std::ostringstream out;
    out << "[";
    bool first_route = true;
    for (const auto& route : routes) {
        if (!first_route) out << ",";
        first_route = false;
        out << "{\"template\":";
        AppendJsonString(out, route.route_template);
        out << ",\"parameters\":[";
        bool first_param = true;
        for (const auto& param : route.parameters) {
            if (!first_param) out << ",";
            first_param = false;
            out << "{\"name\":";
            AppendJsonString(out, param.name);
            out << ",\"optional\":" << (param.kind == RouteParameterKind::Optional ? "true" : "false");
            if (!param.constraint.empty()) {
                out << ",\"constraint\":";
                AppendJsonString(out, param.constraint);
            }
            out << "}";
        }
        out << "]}";
    }
    out << "]";
    return out.str();
}

// Deliberately minimal, same spirit as StateFromJson/ImportsFromJson --
// walks the exact shape RoutesToJson produces rather than a general
// JSON object parser. Field order within each object doesn't matter;
// unrecognized keys are skipped.
std::vector<RouteDeclaration> RoutesFromJson(const std::string& json) {
    std::vector<RouteDeclaration> routes;
    size_t i = 0;
    while (i < json.size() && json[i] != '[') ++i;
    if (i >= json.size()) return routes;
    ++i; // outer '['

    auto SkipWs = [&](size_t& k) {
        while (k < json.size() && (json[k] == ',' || std::isspace(static_cast<unsigned char>(json[k])))) ++k;
    };

    while (true) {
        SkipWs(i);
        if (i >= json.size() || json[i] == ']') break;
        if (json[i] != '{') { ++i; continue; }
        ++i; // route object '{'

        RouteDeclaration route;
        while (i < json.size() && json[i] != '}') {
            SkipWs(i);
            if (i >= json.size() || json[i] == '}') break;
            std::string key = ParseJsonStringAt(json, i);
            while (i < json.size() && (json[i] == ':' || std::isspace(static_cast<unsigned char>(json[i])))) ++i;

            if (key == "template") {
                route.route_template = ParseJsonStringAt(json, i);
            } else if (key == "parameters") {
                while (i < json.size() && json[i] != '[') ++i;
                if (i < json.size()) ++i; // '['
                while (i < json.size() && json[i] != ']') {
                    SkipWs(i);
                    if (i >= json.size() || json[i] == ']') break;
                    if (json[i] != '{') { ++i; continue; }
                    ++i; // param object '{'
                    RouteParameter param;
                    while (i < json.size() && json[i] != '}') {
                        SkipWs(i);
                        if (i >= json.size() || json[i] == '}') break;
                        std::string pkey = ParseJsonStringAt(json, i);
                        while (i < json.size() && (json[i] == ':' || std::isspace(static_cast<unsigned char>(json[i])))) ++i;
                        if (pkey == "name") {
                            param.name = ParseJsonStringAt(json, i);
                        } else if (pkey == "optional") {
                            const bool is_true = json.compare(i, 4, "true") == 0;
                            param.kind = is_true ? RouteParameterKind::Optional : RouteParameterKind::Required;
                            i += is_true ? 4 : 5; // "true" or "false"
                        } else if (pkey == "constraint") {
                            param.constraint = ParseJsonStringAt(json, i);
                        }
                        SkipWs(i);
                    }
                    if (i < json.size() && json[i] == '}') ++i;
                    route.parameters.push_back(std::move(param));
                    SkipWs(i);
                }
                if (i < json.size() && json[i] == ']') ++i;
            }
            SkipWs(i);
        }
        if (i < json.size() && json[i] == '}') ++i;
        routes.push_back(std::move(route));
    }
    return routes;
}

} // namespace ui
} // namespace ava
