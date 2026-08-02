#pragma once
// Minimal HTTP/1.1 request-line + headers + body parser. No chunked
// transfer-encoding support in v0.1 (Content-Length only) -- enough for
// what a dev server / simple app host needs to receive from a browser.
#include <string>

#include "web/protocol/http_types.h"

namespace avahost {

class HttpParser {
public:
    // Parses `raw` (already-fully-read request bytes) into `outRequest`.
    // Returns false with outError set on a malformed request line.
    static bool Parse(const std::string& raw, HttpRequest& outRequest, std::string& outError);

    // True once `buffer` contains a full request: headers terminated by
    // \r\n\r\n, and (if Content-Length is present) that many body bytes
    // too. Used by http_server's read loop to know when to stop reading.
    static bool IsComplete(const std::string& buffer);
};

} // namespace avahost
