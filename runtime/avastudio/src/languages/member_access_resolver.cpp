#include "languages/member_access_resolver.h"

#include <cctype>
#include <vector>

#include "languages/block_scanner.h"
#include "languages/lexer_utils.h"

namespace studio {

using namespace lexer;

namespace {

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

std::string BuildLinePrefix(const std::string& full_text, int cursor_line,
                             const std::string& text_before_cursor_on_line) {
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
    return prefix;
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
                                   const ClassIndex* workspace_classes, const FunctionIndex* workspace_functions,
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

        const size_t word_start = i;
        std::string word = ReadIdent(text, i);
        const int word_line = LineAt(text, word_start);

        if (word == "class") {
            size_t save = i;
            SkipInlineWhitespace(text, i);
            if (i >= end || !IsIdentStart(text[i])) { i = save; continue; }
            ReadIdent(text, i);
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
            ScanRange(text, body_start, body_end, class_index, function_index, workspace_classes,
                      workspace_functions, class_body_scope);
            i = scan_pos;
            continue;
        }

        if (word == "func") {
            size_t save = i;
            SkipInlineWhitespace(text, i);
            if (i >= end || !IsIdentStart(text[i])) { i = save; continue; }
            ReadIdent(text, i);
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
            TryParseAsType(text, body_scan_start, end);

            size_t scan_pos = body_scan_start;
            size_t body_end = 0;
            if (!FindMatchingEnd(text, scan_pos, body_end)) { i = body_scan_start; continue; }

            scopes_.push_back(Scope{});
            Scope& new_scope = scopes_.back();
            new_scope.start = body_scan_start;
            new_scope.end = body_end;
            for (const auto& raw : raw_params) {
                std::string pname = ParamBaseName(raw);
                if (pname.empty()) continue;
                std::string ptype = ParamBaseType(raw);
                if (!ptype.empty()) new_scope.var_types[pname] = ptype;
                new_scope.var_decl_lines[pname] = word_line;
            }

            ScanRange(text, body_scan_start, body_end, class_index, function_index, workspace_classes,
                      workspace_functions, new_scope);
            i = scan_pos;
            continue;
        }

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

        size_t decl_pos = i;
        std::string declared_type = TryParseAsType(text, decl_pos, end);

        size_t eq_check = decl_pos;
        SkipInlineWhitespace(text, eq_check);
        bool is_plain_assign = eq_check < end && text[eq_check] == '=' &&
                                (eq_check + 1 >= end || text[eq_check + 1] != '=');

        if (!is_plain_assign) {
            if (!declared_type.empty()) {
                current.var_types[word] = declared_type;
                if (current.var_decl_lines.find(word) == current.var_decl_lines.end())
                    current.var_decl_lines[word] = word_line;
                i = decl_pos;
            }
            continue;
        }

        size_t rhs = eq_check + 1;
        SkipInlineWhitespace(text, rhs);

        std::string resolved_type = declared_type;
        if (resolved_type.empty() && rhs < end && IsIdentStart(text[rhs])) {
            size_t ident_pos = rhs;
            std::string rhs_name = ReadIdent(text, ident_pos);

            if (rhs_name == "new") {
                size_t after_new = ident_pos;
                SkipInlineWhitespace(text, after_new);
                if (after_new < end && IsIdentStart(text[after_new])) {
                    ident_pos = after_new;
                    rhs_name = ReadIdent(text, ident_pos);
                }
            }

            size_t after_ident = ident_pos;
            SkipInlineWhitespace(text, ident_pos);

            if (ident_pos < end && text[ident_pos] == '(') {
                if (class_index.Find(rhs_name) != nullptr ||
                    (workspace_classes && workspace_classes->Find(rhs_name) != nullptr)) {
                    resolved_type = rhs_name;
                } else if (const FunctionSignature* fn = function_index.Find(rhs_name)) {
                    resolved_type = fn->EffectiveReturnType();
                } else if (workspace_functions) {
                    if (const FunctionSignature* fn = workspace_functions->Find(rhs_name)) {
                        resolved_type = fn->EffectiveReturnType();
                    }
                }
            } else if (ident_pos < end && text[ident_pos] == '.') {
                size_t k = ident_pos + 1;
                SkipInlineWhitespace(text, k);
                if (k < end && IsIdentStart(text[k])) {
                    std::string method_name = ReadIdent(text, k);
                    SkipInlineWhitespace(text, k);
                    if (k < end && text[k] == '(') {
                        std::string obj_type = LookupInScope(current, rhs_name);
                        if (!obj_type.empty()) {
                            for (const auto& member : class_index.FlattenedMembers(obj_type, workspace_classes)) {
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
        if (current.var_decl_lines.find(word) == current.var_decl_lines.end())
            current.var_decl_lines[word] = word_line;
        continue;
    }
}

void VariableTypeIndex::Rebuild(const std::string& text, const ClassIndex& class_index,
                                 const FunctionIndex& function_index,
                                 const ClassIndex* workspace_classes,
                                 const FunctionIndex* workspace_functions) {
    scopes_.clear();
    module_scope_ = Scope{};
    module_scope_.start = 0;
    module_scope_.end = text.size();
    ScanRange(text, 0, text.size(), class_index, function_index, workspace_classes, workspace_functions,
              module_scope_);
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

int VariableTypeIndex::DeclarationLine(const std::string& variable, size_t cursor_offset) const {
    const Scope* best = nullptr;
    for (const auto& s : scopes_) {
        if (cursor_offset >= s.start && cursor_offset <= s.end) {
            if (!best || (s.end - s.start) < (best->end - best->start)) best = &s;
        }
    }
    if (best) {
        auto it = best->var_decl_lines.find(variable);
        if (it != best->var_decl_lines.end()) return it->second;
    }
    auto it = module_scope_.var_decl_lines.find(variable);
    return it == module_scope_.var_decl_lines.end() ? -1 : it->second;
}

std::vector<std::string> VariableTypeIndex::VisibleVariables(size_t cursor_offset) const {
    const Scope* best = nullptr;
    for (const auto& s : scopes_) {
        if (cursor_offset >= s.start && cursor_offset <= s.end) {
            if (!best || (s.end - s.start) < (best->end - best->start)) best = &s;
        }
    }

    std::vector<std::string> names;
    if (best) {
        for (const auto& [name, type] : best->var_types) names.push_back(name);
    }
    for (const auto& [name, type] : module_scope_.var_types) {
        if (!best || best->var_types.find(name) == best->var_types.end()) names.push_back(name);
    }
    return names;
}

bool ResolveMemberAccess(const std::string& full_text, int cursor_line,
                          const std::string& text_before_cursor_on_line,
                          const ClassIndex& class_index, const VariableTypeIndex& var_types,
                          MemberAccessContext& out, const ClassIndex* workspace_classes) {
    std::string identifier;
    if (!ExtractDotAccess(text_before_cursor_on_line, identifier)) return false;

    std::string prefix = BuildLinePrefix(full_text, cursor_line, text_before_cursor_on_line);
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

    if (class_index.Find(identifier) != nullptr ||
        (workspace_classes && workspace_classes->Find(identifier) != nullptr)) {
        out.kind = MemberAccessKind::kClassName;
        out.class_name = identifier;
        out.viewer_class = viewer_class;
        return true;
    }

    return false;
}

bool ResolveOwnScopeSuggestions(const std::string& full_text, int cursor_line,
                                 const std::string& text_before_cursor_on_line,
                                 const ClassIndex& class_index, const VariableTypeIndex& var_types,
                                 std::vector<std::string>& out_variable_names,
                                 std::vector<ClassMember>& out_own_members) {
    std::string prefix = BuildLinePrefix(full_text, cursor_line, text_before_cursor_on_line);
    out_variable_names = var_types.VisibleVariables(prefix.size());

    std::string viewer_class = FindEnclosingClass(prefix);
    if (!viewer_class.empty()) {
        std::vector<ClassMember> members = class_index.FlattenedMembers(viewer_class);
        members = ClassIndex::FilterForAccess(members, MemberAccessKind::kThis, viewer_class);
        for (auto& member : members) {
            if (member.is_method && member.name == member.declared_in) continue;
            out_own_members.push_back(std::move(member));
        }
    }

    return !out_variable_names.empty() || !out_own_members.empty();
}

}
