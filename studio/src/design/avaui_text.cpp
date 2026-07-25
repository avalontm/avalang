#include "design/avaui_text.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <regex>
#include <sstream>
#include <unordered_set>

namespace studio::design {

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
        "properties", "state", "import", "imports", "services", "methods", "lifecycle", "view",
    };
    return kKeywords.count(lower) != 0;
}

bool IsControlKeyword(const std::string& lower) {
    static const std::unordered_set<std::string> kKeywords = {
        "if", "while", "for", "func", "return", "var", "print", "end",
    };
    return kKeywords.count(lower) != 0;
}

const std::unordered_set<std::string>& EventPropNames() {
    static const std::unordered_set<std::string> kEventProps = {
        "click", "onclick", "onchange", "oninput", "onfocus", "onblur",
        "onkeydown", "onkeyup", "onmouseenter", "onmouseleave",
        "onsubmit", "onload", "onerror",
    };
    return kEventProps;
}

// --- Property value quoting (see avaui_text.h header comment) -------------

std::string UnquoteIfString(const std::string& raw) {
    std::string v = Trim(raw);
    if (v.size() >= 2 && v.front() == '"' && v.back() == '"') {
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

// --- properties/state block parsing (flat "key = value" lines) ------------

std::vector<PropertyRow> ParsePropertyLines(const std::vector<std::string>& lines) {
    static const std::regex kPropRe(R"(^(\w+)\s*=\s*(.+)$)");
    std::vector<PropertyRow> rows;
    for (const auto& raw_line : lines) {
        std::string line = Trim(raw_line);
        if (line.empty()) continue;
        std::smatch m;
        if (std::regex_match(line, m, kPropRe)) {
            rows.push_back({m[1].str(), UnquoteIfString(m[2].str())});
        }
    }
    return rows;
}

// --- `view` block parsing: indentation -> DesignNode tree ------------------
// Direct port of AvaComponentParser.cs::TryParseComponent /
// ParseView -- see avaui_text.h for the grammar this recognizes.

// Forwards to design::GenerateNodeUid (defined in design_document.cpp,
// declared in design_document.h which avaui_text.h already includes) --
// aliased locally so the parser code below (ported near-verbatim from
// AvaComponentParser.cs) reads closer to the reference it came from.
std::string GenerateNodeUidLocal() { return GenerateNodeUid(); }


std::optional<DesignNode> TryParseComponent(const std::vector<std::string>& lines, size_t& index) {
    static const std::regex kCallRe(R"(^(\w+)\s*\(\s*\)\s*$)");
    static const std::regex kBareWordRe(R"(^(\w+)$)");
    static const std::regex kPropRe(R"(^(\w+)\s*=\s*(.+)$)");

    const std::string line = Trim(lines[index]);

    std::smatch call_match;
    if (std::regex_match(line, call_match, kCallRe)) {
        const std::string type_original = call_match[1].str();
        if (IsControlKeyword(LowerCopy(type_original))) return std::nullopt;
        ++index;
        DesignNode node;
        node.node_uid = GenerateNodeUidLocal();
        node.type = type_original;
        return node;
    }

    std::smatch bare_match;
    if (!std::regex_match(line, bare_match, kBareWordRe)) return std::nullopt;
    const std::string type_original = bare_match[1].str();
    if (IsControlKeyword(LowerCopy(type_original))) return std::nullopt;

    DesignNode node;
    node.node_uid = GenerateNodeUidLocal();
    node.type = type_original;

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
                node.id = UnquoteIfString(raw_value);
            } else if (IsEventPropertyName(key_lower)) {
                node.events.push_back({key, Trim(raw_value)});
            } else {
                node.properties.push_back({key, UnquoteIfString(raw_value)});
            }
            ++j;
            continue;
        }

        size_t child_index = j;
        std::optional<DesignNode> child = TryParseComponent(lines, child_index);
        if (child) {
            node.children.push_back(std::move(*child));
            j = child_index + 1;
        } else {
            ++j;
        }
    }

    index = (j > 0) ? j - 1 : 0;
    return node;
}

std::vector<DesignNode> ParseViewLines(const std::vector<std::string>& lines) {
    std::vector<DesignNode> top_level;
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
        std::optional<DesignNode> node = TryParseComponent(lines, idx);
        if (node) {
            top_level.push_back(std::move(*node));
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

std::vector<std::string> ParseImportLines(const std::vector<std::string>& lines) {
    static const std::regex kImportRe(R"re(^import\s+"([^"]*)"\s*$)re");
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

std::string JoinTrimmedBlock(const std::vector<std::string>& lines) {
    // Drop leading/trailing blank lines but keep original indentation on
    // everything in between -- this is shown verbatim in the Code view
    // TextEditor (see avaui_text.h), so preserving whatever indentation
    // the user (or a previous WriteAvauiText pass) put there matters
    // more than normalizing it.
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

void WriteNode(const DesignNode& node, int indent, std::ostringstream& out) {
    const std::string pad(static_cast<size_t>(indent) * 4, ' ');

    // A node is written as a bare `Type()` call (no properties/events/
    // children) only when it's an external-component reference by
    // convention (PascalCase, e.g. `Navbar()`) -- see avaui_text.h.
    // Lowercase built-in types (`button`, `column`, ...) always use the
    // block form, even with zero properties (`spacer\nend`).
    const bool is_call_form = !node.type.empty() &&
                               std::isupper(static_cast<unsigned char>(node.type[0])) &&
                               node.id.empty() && node.properties.empty() && node.events.empty() &&
                               node.children.empty();

    if (is_call_form) {
        out << pad << node.type << "()\n";
        return;
    }

    out << pad << node.type << "\n";
    const std::string inner_pad(static_cast<size_t>(indent + 1) * 4, ' ');

    if (!node.id.empty()) {
        out << inner_pad << "id = " << WritePropertyValue(node.id) << "\n";
    }
    for (const auto& prop : node.properties) {
        out << inner_pad << prop.key << " = " << WritePropertyValue(prop.value) << "\n";
    }
    for (const auto& evt : node.events) {
        out << inner_pad << evt.key << " = " << evt.value << "\n";
    }
    if (!node.children.empty() && (!node.id.empty() || !node.properties.empty() || !node.events.empty())) {
        out << "\n"; // cosmetic spacing between props and children, matches the plan's example
    }
    for (const auto& child : node.children) {
        WriteNode(child, indent + 1, out);
    }
    out << pad << "end\n";
}

} // namespace

bool IsEventPropertyName(const std::string& name) { return EventPropNames().count(LowerCopy(name)) != 0; }

std::string WriteAvauiText(const DesignNode& root, const std::string& code_behind,
                            const std::vector<PropertyRow>& initial_state,
                            const std::vector<std::string>& imports) {
    std::ostringstream out;

    for (const auto& imp : imports) {
        out << "import \"" << imp << "\"\n";
    }
    if (!imports.empty()) out << "\n";

    const bool has_page_properties = !root.id.empty() || !root.properties.empty();
    if (has_page_properties) {
        out << "properties\n";
        if (!root.id.empty()) out << "    id = " << WritePropertyValue(root.id) << "\n";
        for (const auto& prop : root.properties) {
            out << "    " << prop.key << " = " << WritePropertyValue(prop.value) << "\n";
        }
        out << "end\n\n";
    }

    if (!initial_state.empty()) {
        out << "state\n";
        for (const auto& row : initial_state) {
            out << "    " << row.key << " = " << WritePropertyValue(row.value) << "\n";
        }
        out << "end\n\n";
    }

    out << "view\n";
    for (const auto& child : root.children) {
        WriteNode(child, /*indent=*/1, out);
    }
    out << "end\n";

    if (!code_behind.empty()) {
        out << "\nmethods\n" << code_behind << "\nend\n";
    }

    return out.str();
}

bool ParseAvauiText(const std::string& text, DesignNode& out_root, std::string& out_code_behind,
                     std::vector<PropertyRow>& out_initial_state, std::vector<std::string>& out_imports,
                     std::string& out_error) {
    const std::vector<std::string> lines = SplitLines(text);

    DesignNode root;
    root.node_uid = GenerateNodeUidLocal();
    root.type = "page";

    std::vector<PropertyRow> initial_state;
    std::string code_behind;

    for (const RawSection& section : SplitTopLevelSections(lines)) {
        if (section.keyword == "properties") {
            for (PropertyRow& row : ParsePropertyLines(section.content_lines)) {
                if (LowerCopy(row.key) == "id") {
                    root.id = row.value;
                } else {
                    root.properties.push_back(std::move(row));
                }
            }
        } else if (section.keyword == "state") {
            initial_state = ParsePropertyLines(section.content_lines);
        } else if (section.keyword == "view") {
            root.children = ParseViewLines(section.content_lines);
        } else if (section.keyword == "methods") {
            code_behind = JoinTrimmedBlock(section.content_lines);
        }
        // "imports"/"services"/"lifecycle" are reserved but not acted on
        // yet -- see avaui_text.h and 08_DESIGNER_VIEW_PLAN.md section 2
        // point 3/section 3's note on component-import resolution.
    }

    out_root = std::move(root);
    out_code_behind = std::move(code_behind);
    out_initial_state = std::move(initial_state);
    out_imports = ParseImportLines(lines);
    out_error.clear();
    return true; // forgiving by design -- see avaui_text.h header comment
}

} // namespace studio::design
