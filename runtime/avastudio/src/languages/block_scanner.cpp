#include "languages/block_scanner.h"

#include <cctype>

namespace studio {

namespace {

bool IsIdentStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
bool IsIdentChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

std::string ReadIdent(const std::string& text, size_t& i) {
    size_t start = i;
    while (i < text.size() && IsIdentChar(text[i])) ++i;
    return text.substr(start, i - start);
}

}

bool IsBlockKeyword(const std::string& word) {
    return word == "try" || word == "if" || word == "while" || word == "for" ||
           word == "func" || word == "class";
}

bool FindMatchingEnd(const std::string& text, size_t& i, size_t& body_end) {
    size_t start = i;
    int depth = 1;
    while (i < text.size()) {
        char c = text[i];
        if (c == '#') {
            while (i < text.size() && text[i] != '\n') ++i;
            continue;
        }
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
            size_t word_start = i;
            std::string word = ReadIdent(text, i);
            if (word == "end") {
                --depth;
                if (depth == 0) { body_end = word_start; return true; }
                continue;
            }
            if (IsBlockKeyword(word)) ++depth;
            continue;
        }
        ++i;
    }
    i = start;
    return false;
}

}
