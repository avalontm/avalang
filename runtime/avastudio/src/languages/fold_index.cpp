#include "languages/fold_index.h"

#include <algorithm>
#include <cctype>

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

int CountNewlines(const std::string& text, size_t from, size_t to) {
    int count = 0;
    const size_t end = std::min(to, text.size());
    for (size_t k = from; k < end; ++k) {
        if (text[k] == '\n') ++count;
    }
    return count;
}

}

void FoldIndex::Rebuild(const std::string& text) {
    ranges_.clear();

    size_t i = 0;
    int line = 0;
    while (i < text.size()) {
        char c = text[i];

        if (c == '\n') { ++line; ++i; continue; }

        if (c == '#') {
            while (i < text.size() && text[i] != '\n') ++i;
            continue;
        }
        if (c == '\'' || c == '"') {
            char quote = c;
            ++i;
            while (i < text.size() && text[i] != quote) {
                if (text[i] == '\n') ++line;
                if (text[i] == '\\' && i + 1 < text.size()) i += 2; else ++i;
            }
            if (i < text.size()) ++i;
            continue;
        }

        if (IsIdentStart(c)) {
            std::string word = ReadIdent(text, i);
            if (IsBlockKeyword(word)) {
                const int start_line = line;
                size_t scan_from = i;
                size_t body_end = 0;
                if (FindMatchingEnd(text, scan_from, body_end)) {
                    const int end_line = start_line + CountNewlines(text, i, body_end);
                    if (end_line > start_line) ranges_.push_back({start_line, end_line});
                }
            }
            continue;
        }

        ++i;
    }
}

const FoldRange* FoldIndex::RangeStartingAt(int line) const {
    for (const auto& range : ranges_) {
        if (range.start_line == line) return &range;
    }
    return nullptr;
}

}
