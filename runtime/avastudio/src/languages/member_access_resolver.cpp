#include "languages/member_access_resolver.h"

#include <cctype>
#include <vector>

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

bool IsBlockKeyword(const std::string& word) {
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
            if (IsBlockKeyword(word)) {
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

void VariableTypeIndex::Rebuild(const std::string& text, const ClassIndex& class_index) {
    variable_types_.clear();

    size_t i = 0;
    while (i < text.size()) {
        char c = text[i];

        if (c == '#') { while (i < text.size() && text[i] != '\n') ++i; continue; }
        if (c == '\'' || c == '"') {
            char quote = c;
            ++i;
            while (i < text.size() && text[i] != quote) {
                if (text[i] == '\\' && i + 1 < text.size()) i += 2; else ++i;
            }
            if (i < text.size()) ++i;
            continue;
        }

        if (IsIdentStart(c)) {
            std::string var_name = ReadIdent(text, i);
            size_t save = i;
            SkipInlineWhitespace(text, i);

            bool is_plain_assign = i < text.size() && text[i] == '=' &&
                                    (i + 1 >= text.size() || text[i + 1] != '=');
            if (!is_plain_assign) { i = save; continue; }

            ++i;
            SkipInlineWhitespace(text, i);

            std::string resolved_class;
            if (i < text.size() && IsIdentStart(text[i])) {
                size_t ident_start = i;
                std::string rhs_name = ReadIdent(text, i);
                size_t after_ident = i;
                SkipInlineWhitespace(text, i);
                if (i < text.size() && text[i] == '(' && class_index.Find(rhs_name) != nullptr) {
                    resolved_class = rhs_name;
                } else {
                    i = after_ident;
                }
                (void)ident_start;
            }

            if (!resolved_class.empty()) {
                variable_types_[var_name] = resolved_class;
            } else {

                variable_types_.erase(var_name);
            }
            continue;
        }

        ++i;
    }
}

std::string VariableTypeIndex::TypeOf(const std::string& variable) const {
    auto it = variable_types_.find(variable);
    return it == variable_types_.end() ? "" : it->second;
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

    std::string var_class = var_types.TypeOf(identifier);
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
