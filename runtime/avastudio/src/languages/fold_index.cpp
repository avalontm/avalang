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

    // Tracks the same "what block am I directly inside" context the
    // formatter and DetectUnreachableCode use, so a bodyless `func Name(...)`
    // signature inside `extern ... as Alias`/`interface` (no `end` of its
    // own -- the block's single `end` closes the whole thing) isn't treated
    // as its own foldable region. Without this, IsBlockKeyword("func")
    // matches every signature line too, and FindMatchingEnd (called with
    // is_interface_body=false, since this loop doesn't know it's inside
    // extern/interface at that point) walks forward counting each following
    // signature as a further +1 nesting level with no matching `end`,
    // producing wrong/spurious fold ranges instead of a clean one for the
    // whole extern/interface block.
    std::vector<std::string> block_stack;

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

            if (word == "end") {
                if (!block_stack.empty()) block_stack.pop_back();
                continue;
            }

            const bool headerless_func = word == "func" && !block_stack.empty() &&
                                          (block_stack.back() == "extern" || block_stack.back() == "interface");
            if (headerless_func) continue;

            // `extern` itself isn't in IsBlockKeyword (never was, even before
            // this fix), but it still opens a block worth folding and worth
            // tracking on block_stack for the headerless-func check above.
            if (word == "extern" || IsBlockKeyword(word)) {
                const int start_line = line;
                size_t scan_from = i;
                size_t body_end = 0;
                const bool is_interface_body = (word == "interface" || word == "extern");
                if (FindMatchingEnd(text, scan_from, body_end, is_interface_body)) {
                    const int end_line = start_line + CountNewlines(text, i, body_end);
                    if (end_line > start_line) ranges_.push_back({start_line, end_line});
                }
                block_stack.push_back(word);
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
