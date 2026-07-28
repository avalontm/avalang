#pragma once
// AvaHost.Web -- small URL helpers shared by the router (dynamic segment
// values) and the request context builder (query string). Kept separate
// from http_parser.h because http_parser only splits raw bytes into
// method/path/query/headers -- it never interprets percent-encoding, and
// callers that need decoded values (route params, request.query) go
// through here instead.
#include <string>
#include <vector>

namespace avahost {

// Decodes a single percent-encoded URL component: "%XX" -> byte, "+" ->
// space (application/x-www-form-urlencoded convention, used both in query
// strings and in path segments produced by most HTTP clients/browsers).
// Malformed escapes (stray '%' not followed by 2 hex digits) are passed
// through verbatim rather than rejected -- this is a best-effort decode
// for routing/display, not a strict validator.
std::string UrlDecode(const std::string& encoded);

// Parses a raw query string ("a=1&b=hello+world&c") into ordered
// key/value pairs, both percent-decoded. A key with no '=' (like "c"
// above) decodes to an empty-string value, same convention as most web
// frameworks. Empty segments (from "&&" or a leading/trailing '&') are
// skipped. Order is preserved (first occurrence order) since callers
// build an AvaLang dict from this, and AvaDict entries preserve
// insertion order (see public/src/c_api.cpp's DictObj).
std::vector<std::pair<std::string, std::string>> ParseQueryString(const std::string& raw);

} // namespace avahost
