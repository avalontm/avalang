#include "panels/syntax_highlight.h"

#include <cctype>
#include <unordered_set>

#include "palette.h"

namespace studio::syntax {

namespace {

bool IsNameStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
bool IsNameChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

const std::unordered_set<std::string>& KeywordSet() {
    static const std::unordered_set<std::string> set(kKeywords.begin(), kKeywords.end());
    return set;
}

const std::unordered_set<std::string>& TypeSet() {
    static const std::unordered_set<std::string> set(kTypes.begin(), kTypes.end());
    return set;
}

}

const std::vector<std::string> kKeywords = {
    "if", "then", "elif", "else", "end", "while", "for", "in",
    "func", "class", "try", "catch", "finally",
    "return", "break", "continue", "pass", "import", "local",
    "raise", "yield", "not", "and", "or", "true", "false", "nil",
    "base", "as",
};

const std::vector<std::string> kTypes = {
    "int", "float", "str", "string", "bool", "list", "dict",
    "number", "function", "instance", "exception", "native", "coroutine",
};

const std::vector<std::string> kBuiltinFunctions = {
    "print", "len", "range", "type", "abs", "min", "max", "pow", "sqrt",
    "round", "floor", "ceil", "sorted", "reversed", "sum", "all", "any", "resume",
    "list_append", "list_push", "list_pop", "list_insert", "list_remove",
    "list_contains", "list_length",
    "dict_keys", "dict_values", "dict_items", "dict_length", "dict_containsKey",
    "str_length", "str_upper", "str_lower", "str_trim", "str_split", "str_replace",
    "str_contains", "str_startsWith", "str_endsWith", "str_substring", "str_indexOf",
};

std::vector<Token> Tokenize(const std::string& text) {
    std::vector<Token> tokens;
    const int n = static_cast<int>(text.size());
    int i = 0;
    std::string prev_keyword;

    auto push = [&](int s, int e, TokenKind k) {
        if (e > s) tokens.push_back({s, e, k});
    };

    while (i < n) {
        const char c = text[i];

        if (c == '#') {
            const int s = i;
            while (i < n && text[i] != '\n') i++;
            push(s, i, TokenKind::Comment);
            continue;
        }

        if (c == '"' || c == '\'' || (c == '$' && i + 1 < n && text[i + 1] == '"')) {
            const int s = i;
            char quote;
            if (c == '$') {
                i += 2;
                quote = '"';
            } else {
                quote = c;
                i += 1;
            }
            while (i < n && text[i] != quote && text[i] != '\n') {
                if (text[i] == '\\' && i + 1 < n) i += 2;
                else i += 1;
            }
            if (i < n && text[i] == quote) i += 1;
            push(s, i, TokenKind::String);
            prev_keyword.clear();
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(c))) {
            const int s = i;
            while (i < n && std::isdigit(static_cast<unsigned char>(text[i]))) i++;
            if (i < n && text[i] == '.' && i + 1 < n && std::isdigit(static_cast<unsigned char>(text[i + 1]))) {
                i++;
                while (i < n && std::isdigit(static_cast<unsigned char>(text[i]))) i++;
            }
            push(s, i, TokenKind::Number);
            prev_keyword.clear();
            continue;
        }

        if (IsNameStart(c)) {
            const int s = i;
            while (i < n && IsNameChar(text[i])) i++;
            const std::string word = text.substr(s, i - s);

            TokenKind kind = TokenKind::Default;
            if (KeywordSet().count(word)) {
                kind = TokenKind::Keyword;
            } else if (prev_keyword == "func") {
                kind = TokenKind::Function;
            } else if (prev_keyword == "class") {
                kind = TokenKind::Type;
            } else if (TypeSet().count(word)) {
                kind = TokenKind::Type;
            } else {
                int j = i;
                while (j < n && (text[j] == ' ' || text[j] == '\t')) j++;
                kind = (j < n && text[j] == '(') ? TokenKind::Function : TokenKind::Default;
            }
            push(s, i, kind);

            prev_keyword = (word == "func" || word == "class") ? word : "";
            continue;
        }

        {
            const int s = i;
            i++;
            push(s, i, TokenKind::Default);
            if (!std::isspace(static_cast<unsigned char>(c))) prev_keyword.clear();
        }
    }

    return tokens;
}

ImU32 ColorForToken(TokenKind kind) {
    using namespace palette;
    switch (kind) {
        case TokenKind::Keyword:  return U32FromHex(kSynKeyword);
        case TokenKind::Function: return U32FromHex(kSynFunction);
        case TokenKind::Type:     return U32FromHex(kSynType);
        case TokenKind::String:   return U32FromHex(kSynString);
        case TokenKind::Number:   return U32FromHex(kSynNumber);
        case TokenKind::Comment:  return U32FromHex(kSynComment);
        case TokenKind::Default:
        default:                 return U32FromHex(kSynVariable);
    }
}

std::vector<std::string> CollectIdentifiers(const std::string& text) {
    std::vector<std::string> result;
    std::unordered_set<std::string> seen;
    const int n = static_cast<int>(text.size());
    int i = 0;
    while (i < n) {
        const char c = text[i];
        if (c == '"' || c == '\'') {
            const char quote = c;
            i++;
            while (i < n && text[i] != quote && text[i] != '\n') {
                if (text[i] == '\\' && i + 1 < n) i += 2; else i++;
            }
            if (i < n) i++;
            continue;
        }
        if (c == '#') { while (i < n && text[i] != '\n') i++; continue; }
        if (IsNameStart(c)) {
            const int s = i;
            while (i < n && IsNameChar(text[i])) i++;
            std::string word = text.substr(s, i - s);
            if (!KeywordSet().count(word) && seen.insert(word).second) {
                result.push_back(std::move(word));
            }
            continue;
        }
        i++;
    }
    return result;
}

}
