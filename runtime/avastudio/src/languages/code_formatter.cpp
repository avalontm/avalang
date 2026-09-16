#include "languages/code_formatter.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <unordered_set>
#include <vector>

namespace studio::languages {

namespace {

bool IsIdentStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
bool IsIdentChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

const std::unordered_set<std::string>& OpenKeywords() {
    static const std::unordered_set<std::string> keywords = {
        "class", "interface", "func", "if", "while", "for", "try", "select", "extern",
    };
    return keywords;
}

const std::unordered_set<std::string>& MidKeywords() {
    static const std::unordered_set<std::string> keywords = {
        "end", "else", "elif", "catch", "finally", "case",
    };
    return keywords;
}

std::string RightTrim(const std::string& text) {
    const size_t end = text.find_last_not_of(" \t\r");
    return end == std::string::npos ? "" : text.substr(0, end + 1);
}

std::string LeftTrim(const std::string& text) {
    const size_t start = text.find_first_not_of(" \t");
    return start == std::string::npos ? "" : text.substr(start);
}

size_t ConsumeSimpleString(const std::string& line, size_t start) {
    const char quote = line[start];
    size_t i = start + 1;
    while (i < line.size()) {
        if (line[i] == '\\' && i + 1 < line.size()) {
            i += 2;
            continue;
        }
        if (line[i] == quote) return i + 1;
        ++i;
    }
    return line.size();
}

size_t ConsumeBalancedInterpolation(const std::string& line, size_t start) {
    size_t i = start + 1;
    int depth = 1;
    while (i < line.size() && depth > 0) {
        const char c = line[i];
        if (c == '"' || c == '\'') {
            i = ConsumeSimpleString(line, i);
            continue;
        }
        if (c == '{') {
            ++depth;
            ++i;
            continue;
        }
        if (c == '}') {
            --depth;
            ++i;
            continue;
        }
        ++i;
    }
    return i;
}

size_t ConsumeFString(const std::string& line, size_t start) {
    size_t i = start + 2;
    while (i < line.size()) {
        const char c = line[i];
        if (c == '\\' && i + 1 < line.size()) {
            i += 2;
            continue;
        }
        if (c == '"') return i + 1;
        if (c == '{') {
            if (i + 1 < line.size() && line[i + 1] == '{') {
                i += 2;
                continue;
            }
            i = ConsumeBalancedInterpolation(line, i);
            continue;
        }
        ++i;
    }
    return line.size();
}

std::string BuildCodeMask(const std::string& line) {
    std::string mask(line.size(), ' ');
    size_t i = 0;
    while (i < line.size()) {
        const char c = line[i];
        if (c == '#') break;
        if (c == '$' && i + 1 < line.size() && line[i + 1] == '"') {
            i = ConsumeFString(line, i);
            continue;
        }
        if (c == '"' || c == '\'') {
            i = ConsumeSimpleString(line, i);
            continue;
        }
        mask[i] = c;
        ++i;
    }
    return mask;
}

std::vector<std::string> ExtractWords(const std::string& mask) {
    std::vector<std::string> words;
    size_t i = 0;
    while (i < mask.size()) {
        if (IsIdentStart(mask[i])) {
            size_t j = i + 1;
            while (j < mask.size() && IsIdentChar(mask[j])) ++j;
            words.push_back(mask.substr(i, j - i));
            i = j;
        } else {
            ++i;
        }
    }
    return words;
}

std::string FirstWord(const std::string& mask) {
    size_t i = 0;
    while (i < mask.size() && std::isspace(static_cast<unsigned char>(mask[i]))) ++i;
    if (i >= mask.size() || !IsIdentStart(mask[i])) return "";
    size_t j = i + 1;
    while (j < mask.size() && IsIdentChar(mask[j])) ++j;
    return mask.substr(i, j - i);
}

char FirstNonSpace(const std::string& mask) {
    for (const char c : mask) {
        if (!std::isspace(static_cast<unsigned char>(c))) return c;
    }
    return '\0';
}

int BracketDelta(const std::string& mask) {
    int delta = 0;
    for (const char c : mask) {
        if (c == '(' || c == '[' || c == '{') {
            ++delta;
        } else if (c == ')' || c == ']' || c == '}') {
            --delta;
        }
    }
    return delta;
}

std::vector<std::string> SplitLines(const std::string& source) {
    std::string normalized;
    normalized.reserve(source.size());
    for (const char c : source) {
        if (c != '\r') normalized += c;
    }

    std::vector<std::string> lines;
    std::istringstream stream(normalized);
    std::string line;
    while (std::getline(stream, line)) lines.push_back(line);
    return lines;
}

}

std::string FormatAvalangSource(const std::string& source, int indent_width) {
    const std::vector<std::string> lines = SplitLines(source);

    std::vector<std::string> output;
    output.reserve(lines.size());

    std::vector<std::string> block_stack;
    int bracket_depth = 0;
    bool previous_was_blank = false;

    for (const std::string& raw_line : lines) {
        const std::string trimmed = RightTrim(LeftTrim(raw_line));

        if (trimmed.empty()) {
            if (!previous_was_blank && !output.empty()) output.push_back("");
            previous_was_blank = true;
            continue;
        }
        previous_was_blank = false;

        const std::string mask = BuildCodeMask(trimmed);
        const std::string first_word = FirstWord(mask);
        const char first_char = FirstNonSpace(mask);

        const int depth = static_cast<int>(block_stack.size());
        const int keyword_dedent = MidKeywords().count(first_word) ? 1 : 0;
        const int bracket_dedent = (first_char == ')' || first_char == ']' || first_char == '}') ? 1 : 0;

        const int line_level =
            std::max(0, depth - keyword_dedent) + std::max(0, bracket_depth - bracket_dedent);

        output.push_back(std::string(static_cast<size_t>(line_level) * indent_width, ' ') + trimmed);

        const bool headerless_func = first_word == "func" && !block_stack.empty() &&
                                      (block_stack.back() == "extern" || block_stack.back() == "interface");

        for (const std::string& word : ExtractWords(mask)) {
            if (word == "end") {
                if (!block_stack.empty()) block_stack.pop_back();
            } else if (OpenKeywords().count(word)) {
                if (word == "func" && headerless_func) continue;
                block_stack.push_back(word);
            }
        }

        bracket_depth = std::max(0, bracket_depth + BracketDelta(mask));
    }

    while (!output.empty() && output.back().empty()) output.pop_back();

    std::string result;
    for (const std::string& line : output) {
        result += line;
        result += '\n';
    }
    return result;
}

}
