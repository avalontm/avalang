#pragma once
#include <cctype>
#include <map>
#include <string>

namespace avahost {

// Case-insensitive header map (HTTP header names are case-insensitive).
struct CaseInsensitiveLess {
    bool operator()(const std::string& a, const std::string& b) const {
        return strcasecmp_compat(a, b) < 0;
    }
    static int strcasecmp_compat(const std::string& a, const std::string& b) {
        size_t n = a.size() < b.size() ? a.size() : b.size();
        for (size_t i = 0; i < n; ++i) {
            char ca = static_cast<char>(::tolower(static_cast<unsigned char>(a[i])));
            char cb = static_cast<char>(::tolower(static_cast<unsigned char>(b[i])));
            if (ca != cb) return ca < cb ? -1 : 1;
        }
        if (a.size() == b.size()) return 0;
        return a.size() < b.size() ? -1 : 1;
    }
};

using HttpHeaders = std::map<std::string, std::string, CaseInsensitiveLess>;

struct HttpRequest {
    std::string method = "GET";
    std::string path = "/";     // decoded, without query string
    std::string query;          // raw query string, without leading '?'
    std::string httpVersion = "HTTP/1.1";
    HttpHeaders headers;
    std::string body;

    std::string HeaderOr(const std::string& name, const std::string& fallback = "") const {
        auto it = headers.find(name);
        return it != headers.end() ? it->second : fallback;
    }
};

struct HttpResponse {
    int statusCode = 200;
    std::string statusText = "OK";
    HttpHeaders headers;
    std::string body;

    // When true, HttpServer::HandleConnection skips its normal
    // "<method> <path> -> <status>" log line for this response. Set by
    // AvaHostApp for its own internal, high-frequency endpoints (the
    // Hot Reload poll, plan section 15) so a once-a-second background
    // check doesn't drown out real request logs -- HttpServer itself
    // stays generic and never needs to know that endpoint exists.
    bool skipAccessLog = false;

    void SetHeader(const std::string& name, const std::string& value) { headers[name] = value; }

    static HttpResponse Text(int status, const std::string& text, const std::string& contentType = "text/plain; charset=utf-8") {
        HttpResponse r;
        r.statusCode = status;
        r.body = text;
        r.SetHeader("Content-Type", contentType);
        return r;
    }
    static HttpResponse Html(int status, const std::string& html) {
        return Text(status, html, "text/html; charset=utf-8");
    }
    static HttpResponse NotFound(const std::string& message = "404 Not Found") {
        return Text(404, message);
    }
    static HttpResponse ServerError(const std::string& message) {
        return Text(500, message);
    }
};

} // namespace avahost
