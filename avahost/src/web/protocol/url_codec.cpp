#include "web/protocol/url_codec.h"

#include <cctype>
#include <sstream>

namespace avahost {

namespace {

int HexDigit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

} // namespace

std::string UrlDecode(const std::string& encoded) {
    std::string out;
    out.reserve(encoded.size());

    for (size_t i = 0; i < encoded.size(); ++i) {
        char c = encoded[i];
        if (c == '+') {
            out += ' ';
        } else if (c == '%' && i + 2 < encoded.size()) {
            int hi = HexDigit(encoded[i + 1]);
            int lo = HexDigit(encoded[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out += static_cast<char>((hi << 4) | lo);
                i += 2;
            } else {
                out += c; // malformed escape -- pass through as-is
            }
        } else {
            out += c;
        }
    }
    return out;
}

std::vector<std::pair<std::string, std::string>> ParseQueryString(const std::string& raw) {
    std::vector<std::pair<std::string, std::string>> result;
    if (raw.empty()) return result;

    std::istringstream stream(raw);
    std::string pair;
    while (std::getline(stream, pair, '&')) {
        if (pair.empty()) continue;

        size_t eq = pair.find('=');
        std::string key = eq == std::string::npos ? pair : pair.substr(0, eq);
        std::string value = eq == std::string::npos ? "" : pair.substr(eq + 1);
        if (key.empty()) continue;

        result.emplace_back(UrlDecode(key), UrlDecode(value));
    }
    return result;
}

} // namespace avahost
