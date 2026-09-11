#include "languages/block_scanner.h"

#include <algorithm>
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

void SkipInlineWhitespace(const std::string& text, size_t& i) {
    while (i < text.size() && (text[i] == ' ' || text[i] == '\t')) ++i;
}

std::string PeekNextWord(const std::string& text, size_t i) {
    for (;;) {
        while (i < text.size() && (text[i] == ' ' || text[i] == '\t' || text[i] == '\r' || text[i] == '\n')) ++i;
        if (i < text.size() && text[i] == '#') {
            while (i < text.size() && text[i] != '\n') ++i;
            continue;
        }
        break;
    }
    if (i >= text.size() || !IsIdentStart(text[i])) return "";
    size_t j = i;
    return ReadIdent(text, j);
}

bool IsBodylessFuncSignature(const std::string& text, size_t i) {
    SkipInlineWhitespace(text, i);
    if (i >= text.size() || !IsIdentStart(text[i])) return false;
    ReadIdent(text, i);
    SkipInlineWhitespace(text, i);
    if (i >= text.size() || text[i] != '(') return false;

    int depth = 0;
    for (; i < text.size(); ++i) {
        if (text[i] == '(') ++depth;
        else if (text[i] == ')') {
            --depth;
            if (depth == 0) { ++i; break; }
        }
    }
    if (depth != 0) return false;

    SkipInlineWhitespace(text, i);
    size_t as_save = i;
    if (i < text.size() && IsIdentStart(text[i])) {
        std::string maybe_as = ReadIdent(text, i);
        if (maybe_as == "as") {
            SkipInlineWhitespace(text, i);
            if (i < text.size() && IsIdentStart(text[i])) ReadIdent(text, i);
            else i = as_save;
        } else {
            i = as_save;
        }
    }

    std::string next_word = PeekNextWord(text, i);
    return next_word == "end" || next_word == "func" || next_word == "static" || next_word == "private";
}

}

int LineAt(const std::string& text, size_t offset) {
    const size_t clamped = offset > text.size() ? text.size() : offset;
    return static_cast<int>(std::count(text.begin(), text.begin() + static_cast<long>(clamped), '\n'));
}

bool IsBlockKeyword(const std::string& word) {
    return word == "try" || word == "if" || word == "while" || word == "for" ||
           word == "func" || word == "class" || word == "interface";
}

bool FindMatchingEnd(const std::string& text, size_t& i, size_t& body_end, bool is_interface_body) {
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
            if (word == "func" && is_interface_body && IsBodylessFuncSignature(text, i)) continue;
            if (IsBlockKeyword(word)) ++depth;
            continue;
        }
        ++i;
    }
    i = start;
    return false;
}

}
