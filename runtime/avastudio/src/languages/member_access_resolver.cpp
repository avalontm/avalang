#include "languages/member_access_resolver.h"

#include <cctype>
#include <vector>

#include "languages/block_scanner.h"

namespace studio {

namespace {

bool IsIdentStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
bool IsIdentChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

std::string ReadIdent(const std::string& text, size_t& i) {
    size_t start = i;
    while (i < text.size() && IsIdentChar(text[i])) ++i;
    return text.substr(start, i - start);
}

void SkipInlineWhitespace(const std::string& text, size_t& i) {
    while (i < text.size() && (text[i] == ' ' || text[i] == '\t')) ++i;
}

std::vector<std::string> SplitParams(const std::string& raw) {
    std::vector<std::string> params;
    int depth = 0;
    char in_string = '\0';
    std::string current;

    for (size_t i = 0; i < raw.size(); ++i) {
        char c = raw[i];
        if (in_string) {
            current += c;
            if (c == '\\' && i + 1 < raw.size()) { current += raw[++i]; continue; }
            if (c == in_string) in_string = '\0';
            continue;
        }
        if (c == '\'' || c == '"') { in_string = c; current += c; continue; }
        if (c == '(' || c == '[' || c == '{') { ++depth; current += c; continue; }
        if (c == ')' || c == ']' || c == '}') { --depth; current += c; continue; }
        if (c == ',' && depth == 0) { params.push_back(current); current.clear(); continue; }
        current += c;
    }
    if (!current.empty() || !params.empty()) params.push_back(current);

    std::vector<std::string> trimmed;
    for (auto& p : params) {
        size_t b = p.find_first_not_of(" \t\r\n");
        if (b == std::string::npos) continue;
        size_t e = p.find_last_not_of(" \t\r\n");
        trimmed.push_back(p.substr(b, e - b + 1));
    }
    return trimmed;
}

// Tries to parse "as Type" starting at `i` (which is left untouched if it
// doesn't match). On success advances `i` past the type name and returns it.
std::string TryParseAsType(const std::string& text, size_t& i, size_t end) {
    size_t save = i;
    SkipInlineWhitespace(text, i);
    if (i >= end || !IsIdentStart(text[i])) { i = save; return ""; }
    size_t as_save = i;
    std::string maybe_as = ReadIdent(text, i);
    if (maybe_as != "as") { i = as_save; return ""; }
    SkipInlineWhitespace(text, i);
    if (i >= end || !IsIdentStart(text[i])) { i = as_save; return ""; }
    return ReadIdent(text, i);
}

bool IsBlockKeywordLocal(const std::string& word) {
    return word == "try" || word == "if" || word == "while" || word == "for" ||
           word == "func" || word == "class";
}

struct BlockFrame {
    bool is_class = false;
    std::string class_name;
};

std::string FindEnclosingClass(const std::string& prefix) {
    std::vector<BlockFrame> stack;
    size_t i = 0;
    while (i < prefix.size()) {
        char c = prefix[i];

        if (c == '#') {
            while (i < prefix.size() && prefix[i] != '\n') ++i;
            continue;
        }
        if (c == '\'' || c == '"') {
            char quote = c;
            ++i;
            while (i < prefix.size() && prefix[i] != quote) {
                if (prefix[i] == '\\' && i + 1 < prefix.size()) i += 2; else ++i;
            }
            if (i < prefix.size()) ++i;
            continue;
        }

        if (IsIdentStart(c)) {
            std::string word = ReadIdent(prefix, i);

            if (word == "class") {
                size_t save = i;
                SkipInlineWhitespace(prefix, i);
                BlockFrame frame;
                frame.is_class = true;
                if (i < prefix.size() && IsIdentStart(prefix[i])) {
                    frame.class_name = ReadIdent(prefix, i);
                } else {
                    i = save;
                }
                stack.push_back(std::move(frame));
                continue;
            }
            if (word == "end") {
                if (!stack.empty()) stack.pop_back();
                continue;
            }
            if (IsBlockKeywordLocal(word)) {
                stack.push_back(BlockFrame{});
                continue;
            }
            continue;
        }

        ++i;
    }

    for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
        if (it->is_class) return it->class_name;
    }
    return "";
}

bool ExtractDotAccess(const std::string& before, std::string& identifier) {
    size_t i = before.size();
    while (i > 0 && IsIdentChar(before[i - 1])) --i;

    if (i == 0 || before[i - 1] != '.') return false;
    size_t dot = i - 1;

    size_t j = dot;
    while (j > 0 && IsIdentChar(before[j - 1])) --j;
    if (j == dot) return false;

    if (j > 0 && before[j - 1] == '.') return false;

    identifier = before.substr(j, dot - j);
    return true;
}

}

std::string VariableTypeIndex::LookupInScope(const Scope& current, const std::string& name) const {
    auto it = current.var_types.find(name);
    if (it != current.var_types.end()) return it->second;
    if (&current != &module_scope_) {
        auto mit = module_scope_.var_types.find(name);
        if (mit != module_scope_.var_types.end()) return mit->second;
    }
    return "";
}

void VariableTypeIndex::ScanRange(const std::string& text, size_t start, size_t end,
                                   const ClassIndex& class_index, const FunctionIndex& function_index,
                                   Scope& current) {
    size_t i = start;
    while (i < end) {
        char c = text[i];

        if (c == '#') { while (i < end && text[i] != '\n') ++i; continue; }
        if (c == '\'' || c == '"') {
            char quote = c;
            ++i;
            while (i < end && text[i] != quote) {
                if (text[i] == '\\' && i + 1 < end) i += 2; else ++i;
            }
            if (i < end) ++i;
            continue;
        }

        if (!IsIdentStart(c)) { ++i; continue; }

        std::string word = ReadIdent(text, i);

        // `class ... end`: recurse into its body (in a throwaway scope) so
        // methods inside get their own function scope pushed onto
        // `scopes_`, but stray class-body-level statements (attribute
        // declarations) don't leak into whichever scope is currently
        // active around the class definition.
        if (word == "class") {
            size_t save = i;
            SkipInlineWhitespace(text, i);
            if (i >= end || !IsIdentStart(text[i])) { i = save; continue; }
            ReadIdent(text, i);  // class name, not needed here
            SkipInlineWhitespace(text, i);
            if (i < end && text[i] == ':') {
                size_t colon = i;
                ++i;
                SkipInlineWhitespace(text, i);
                if (i < end && IsIdentStart(text[i])) {
                    ReadIdent(text, i);
                } else {
                    i = colon;
                }
            }

            size_t body_start = i;
            size_t scan_pos = i;
            size_t body_end = 0;
            if (!FindMatchingEnd(text, scan_pos, body_end)) continue;

            Scope class_body_scope;
            ScanRange(text, body_start, body_end, class_index, function_index, class_body_scope);
            i = scan_pos;
            continue;
        }

        // `func name(params) [as Type] ... end`: everything assigned inside
        // gets its own scope (seeded with the parameter types), pushed onto
        // `scopes_` so TypeOf() can find it later by cursor offset.
        if (word == "func") {
            size_t save = i;
            SkipInlineWhitespace(text, i);
            if (i >= end || !IsIdentStart(text[i])) { i = save; continue; }
            ReadIdent(text, i);  // function/method name, not needed here
            SkipInlineWhitespace(text, i);
            if (i >= end || text[i] != '(') { i = save; continue; }

            size_t open = i;
            int depth = 0;
            size_t j = open;
            for (; j < end; ++j) {
                if (text[j] == '(') ++depth;
                else if (text[j] == ')') { --depth; if (depth == 0) break; }
            }
            if (j >= end) { i = save; continue; }

            std::vector<std::string> raw_params = SplitParams(text.substr(open + 1, j - open - 1));

            size_t after_params = j + 1;
            size_t body_scan_start = after_params;
            TryParseAsType(text, body_scan_start, end);  // skip an optional return type

            size_t scan_pos = body_scan_start;
            size_t body_end = 0;
            if (!FindMatchingEnd(text, scan_pos, body_end)) { i = body_scan_start; continue; }

            scopes_.push_back(Scope{});
            Scope& new_scope = scopes_.back();
            new_scope.start = body_scan_start;
            new_scope.end = body_end;
            for (const auto& raw : raw_params) {
                std::string pname = ParamBaseName(raw);
                std::string ptype = ParamBaseType(raw);
                if (!pname.empty() && !ptype.empty()) new_scope.var_types[pname] = ptype;
            }

            ScanRange(text, body_scan_start, body_end, class_index, function_index, new_scope);
            i = scan_pos;
            continue;
        }

        // `word.member[.more]` (e.g. `this.attr = ...`, `perro.nombre = ...`,
        // `Clase.staticAttr = ...`) is a member access, not a bare local
        // variable -- ClassIndex already tracks attribute types. Consume
        // the whole dotted chain here so `member` doesn't fall through to
        // the generic identifier handling below and get misread as a bare
        // local assignment target (it would otherwise poison this scope
        // with a fake local named `member` for every `obj.member = Clase(...)`).
        {
            size_t dot_check = i;
            SkipInlineWhitespace(text, dot_check);
            if (dot_check < end && text[dot_check] == '.') {
                size_t k = dot_check;
                while (k < end && text[k] == '.') {
                    ++k;
                    SkipInlineWhitespace(text, k);
                    if (k >= end || !IsIdentStart(text[k])) break;
                    ReadIdent(text, k);
                    size_t peek = k;
                    SkipInlineWhitespace(text, peek);
                    if (peek < end && text[peek] == '.') { k = peek; continue; }
                    break;
                }
                i = k;
                continue;
            }
        }

        // `word as Type` (declared type, with or without an initializer)
        // and `word = expr` (plain assignment) are the two patterns that
        // can teach us `word`'s type.
        size_t decl_pos = i;
        std::string declared_type = TryParseAsType(text, decl_pos, end);

        size_t eq_check = decl_pos;
        SkipInlineWhitespace(text, eq_check);
        bool is_plain_assign = eq_check < end && text[eq_check] == '=' &&
                                (eq_check + 1 >= end || text[eq_check + 1] != '=');

        if (!is_plain_assign) {
            if (!declared_type.empty()) {
                current.var_types[word] = declared_type;
                i = decl_pos;
            }
            continue;
        }

        size_t rhs = eq_check + 1;
        SkipInlineWhitespace(text, rhs);

        std::string resolved_type = declared_type;  // "x as Tipo = expr" -> declared type wins
        if (resolved_type.empty() && rhs < end && IsIdentStart(text[rhs])) {
            size_t ident_pos = rhs;
            std::string rhs_name = ReadIdent(text, ident_pos);
            size_t after_ident = ident_pos;
            SkipInlineWhitespace(text, ident_pos);

            if (ident_pos < end && text[ident_pos] == '(') {
                // `rhs_name(...)`: either a class constructor or a function call.
                if (class_index.Find(rhs_name) != nullptr) {
                    resolved_type = rhs_name;
                } else if (const FunctionSignature* fn = function_index.Find(rhs_name)) {
                    resolved_type = fn->EffectiveReturnType();
                }
            } else if (ident_pos < end && text[ident_pos] == '.') {
                // `obj.metodo(...)`: one dot level, no chained access.
                size_t k = ident_pos + 1;
                SkipInlineWhitespace(text, k);
                if (k < end && IsIdentStart(text[k])) {
                    std::string method_name = ReadIdent(text, k);
                    SkipInlineWhitespace(text, k);
                    if (k < end && text[k] == '(') {
                        std::string obj_type = LookupInScope(current, rhs_name);
                        if (!obj_type.empty()) {
                            for (const auto& member : class_index.FlattenedMembers(obj_type)) {
                                if (member.is_method && member.name == method_name && member.signature) {
                                    resolved_type = member.signature->EffectiveReturnType();
                                    break;
                                }
                            }
                        }
                    }
                }
            } else {
                i = after_ident;
            }
        }

        if (!resolved_type.empty()) {
            current.var_types[word] = resolved_type;
        } else {
            current.var_types.erase(word);
        }
        continue;
    }
}

void VariableTypeIndex::Rebuild(const std::string& text, const ClassIndex& class_index,
                                 const FunctionIndex& function_index) {
    scopes_.clear();
    module_scope_ = Scope{};
    module_scope_.start = 0;
    module_scope_.end = text.size();
    ScanRange(text, 0, text.size(), class_index, function_index, module_scope_);
}

std::string VariableTypeIndex::TypeOf(const std::string& variable, size_t cursor_offset) const {
    const Scope* best = nullptr;
    for (const auto& s : scopes_) {
        if (cursor_offset >= s.start && cursor_offset <= s.end) {
            if (!best || (s.end - s.start) < (best->end - best->start)) best = &s;
        }
    }
    if (best) {
        auto it = best->var_types.find(variable);
        if (it != best->var_types.end()) return it->second;
    }
    auto it = module_scope_.var_types.find(variable);
    return it == module_scope_.var_types.end() ? "" : it->second;
}

bool ResolveMemberAccess(const std::string& full_text, int cursor_line,
                          const std::string& text_before_cursor_on_line,
                          const ClassIndex& class_index, const VariableTypeIndex& var_types,
                          MemberAccessContext& out) {
    std::string identifier;
    if (!ExtractDotAccess(text_before_cursor_on_line, identifier)) return false;

    std::string prefix;
    if (cursor_line > 0) {
        size_t pos = 0;
        int line = 0;
        while (line < cursor_line && pos <= full_text.size()) {
            size_t nl = full_text.find('\n', pos);
            if (nl == std::string::npos) { pos = full_text.size(); break; }
            prefix.append(full_text, pos, nl - pos + 1);
            pos = nl + 1;
            ++line;
        }
    }
    prefix += text_before_cursor_on_line;

    std::string viewer_class = FindEnclosingClass(prefix);

    if (identifier == "this") {
        if (viewer_class.empty()) return false;
        out.kind = MemberAccessKind::kThis;
        out.class_name = viewer_class;
        out.viewer_class = viewer_class;
        return true;
    }

    std::string var_class = var_types.TypeOf(identifier, prefix.size());
    if (!var_class.empty()) {
        out.kind = MemberAccessKind::kInstance;
        out.class_name = var_class;
        out.viewer_class = viewer_class;
        return true;
    }

    if (class_index.Find(identifier) != nullptr) {
        out.kind = MemberAccessKind::kClassName;
        out.class_name = identifier;
        out.viewer_class = viewer_class;
        return true;
    }

    return false;
}

}
