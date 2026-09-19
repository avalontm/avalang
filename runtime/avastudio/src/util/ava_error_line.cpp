#include "util/ava_error_line.h"

#include <cctype>
#include <cstdlib>

namespace studio {

namespace {

const char kPrefix[] = "@@AVA_ERROR@@ ";

bool ExtractJsonString(const std::string& json, const std::string& key, std::string& out) {
    const std::string needle = "\"" + key + "\":\"";
    const size_t start = json.find(needle);
    if (start == std::string::npos) return false;

    size_t pos = start + needle.size();
    std::string result;
    while (pos < json.size() && json[pos] != '"') {
        const char c = json[pos];
        if (c == '\\' && pos + 1 < json.size()) {
            const char next = json[pos + 1];
            if (next == '"') { result += '"'; pos += 2; continue; }
            if (next == '\\') { result += '\\'; pos += 2; continue; }
            if (next == 'n') { result += '\n'; pos += 2; continue; }
            if (next == 'r') { result += '\r'; pos += 2; continue; }
            if (next == 't') { result += '\t'; pos += 2; continue; }
            if (next == 'u' && pos + 5 < json.size()) {
                const std::string hex = json.substr(pos + 2, 4);
                result += static_cast<char>(std::strtol(hex.c_str(), nullptr, 16));
                pos += 6;
                continue;
            }
        }
        result += c;
        ++pos;
    }
    out = result;
    return true;
}

bool ExtractJsonInt(const std::string& json, const std::string& key, int& out) {
    const std::string needle = "\"" + key + "\":";
    const size_t start = json.find(needle);
    if (start == std::string::npos) return false;

    size_t pos = start + needle.size();
    size_t end = pos;
    while (end < json.size() && (std::isdigit(static_cast<unsigned char>(json[end])) || json[end] == '-')) ++end;
    if (end == pos) return false;

    out = std::atoi(json.substr(pos, end - pos).c_str());
    return true;
}

}

bool ParseAvaErrorLine(const std::string& line, ParsedAvaError& out) {
    if (line.rfind(kPrefix, 0) != 0) return false;

    const std::string json = line.substr(sizeof(kPrefix) - 1);
    std::string kind;
    std::string message;
    if (!ExtractJsonString(json, "kind", kind) || !ExtractJsonString(json, "message", message)) return false;

    out.kind = kind;
    out.message = message;
    out.file.clear();
    out.line = 0;
    out.col = 0;

    std::string file;
    if (ExtractJsonString(json, "file", file)) out.file = file;

    int line_no = 0;
    if (ExtractJsonInt(json, "line", line_no)) out.line = line_no;

    int col_no = 0;
    if (ExtractJsonInt(json, "col", col_no)) out.col = col_no;

    return true;
}

}
