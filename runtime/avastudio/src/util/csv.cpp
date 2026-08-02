#include "util/csv.h"

namespace studio::util {

std::vector<std::vector<std::string>> ParseCsv(const std::string& text) {
    std::vector<std::vector<std::string>> rows;
    std::vector<std::string> row;
    std::string field;
    bool in_quotes = false;
    size_t i = 0;
    const size_t n = text.size();

    auto end_field = [&]() {
        row.push_back(field);
        field.clear();
    };
    auto end_row = [&]() {
        end_field();
        rows.push_back(row);
        row.clear();
    };

    while (i < n) {
        char c = text[i];
        if (in_quotes) {
            if (c == '"') {
                if (i + 1 < n && text[i + 1] == '"') { // "" -> " literal
                    field += '"';
                    i += 2;
                    continue;
                }
                in_quotes = false;
                ++i;
                continue;
            }
            field += c;
            ++i;
            continue;
        }
        if (c == '"') { in_quotes = true; ++i; continue; }
        if (c == ',') { end_field(); ++i; continue; }
        if (c == '\r') { ++i; continue; } // CRLF -> ignorar el \r, el \n cierra la fila
        if (c == '\n') { end_row(); ++i; continue; }
        field += c;
        ++i;
    }
    // Última fila sin salto de línea final.
    if (!field.empty() || !row.empty()) end_row();
    return rows;
}

std::string EscapeCell(const std::string& value) {
    bool needs_quotes = value.find_first_of(",\"\n\r") != std::string::npos;
    if (!needs_quotes) return value;
    std::string out = "\"";
    for (char c : value) {
        if (c == '"') out += "\"\"";
        else out += c;
    }
    out += "\"";
    return out;
}

std::string WriteCsvRow(const std::vector<std::string>& fields) {
    std::string out;
    for (size_t i = 0; i < fields.size(); ++i) {
        if (i > 0) out += ",";
        out += EscapeCell(fields[i]);
    }
    return out;
}

std::string UnescapeCell(const std::string& raw) {
    std::string out;
    out.reserve(raw.size());
    for (size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] == '\\' && i + 1 < raw.size()) {
            char next = raw[i + 1];
            if (next == 'n') { out += '\n'; ++i; continue; }
            if (next == 't') { out += '\t'; ++i; continue; }
            if (next == '\\') { out += '\\'; ++i; continue; }
        }
        out += raw[i];
    }
    return out;
}

std::vector<std::string> SplitOn(const std::string& text, const std::string& separator) {
    std::vector<std::string> parts;
    if (separator.empty() || text.empty()) {
        if (!text.empty()) parts.push_back(text);
        return parts;
    }
    size_t start = 0;
    while (true) {
        size_t pos = text.find(separator, start);
        if (pos == std::string::npos) {
            parts.push_back(text.substr(start));
            break;
        }
        parts.push_back(text.substr(start, pos - start));
        start = pos + separator.size();
    }
    return parts;
}

std::string JoinOn(const std::vector<std::string>& parts, const std::string& separator) {
    std::string out;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) out += separator;
        out += parts[i];
    }
    return out;
}

} // namespace studio::util
