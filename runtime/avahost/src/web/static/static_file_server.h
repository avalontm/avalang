#pragma once
// AvaHost.StaticFiles -- serves wwwroot/ directly, never through
// AvaLang (plan section 13).
#include <optional>
#include <string>

#include "web/protocol/http_types.h"

namespace avahost {

class StaticFileServer {
public:
    // `devMode` controls the Cache-Control policy: true (avahost run
    // --watch) sends "no-cache" so every navigation revalidates against
    // the file on disk (still cheap -- see ifNoneMatch below -- but
    // never shows a stale file while editing). false (production)
    // sends a long max-age since the ETag guards correctness anyway.
    StaticFileServer(std::string wwwrootDir, bool devMode);

    // Returns a response only when `requestPath` maps to an existing
    // file under wwwroot; nullopt means "not a static file, try the
    // router next". `ifNoneMatch` is the request's If-None-Match header
    // (empty if absent); when it matches the file's current ETag this
    // returns a bodyless 304 instead of re-sending the file.
    std::optional<HttpResponse> TryServe(const std::string& requestPath,
                                          const std::string& ifNoneMatch = "") const;

private:
    std::string wwwrootDir_;
    bool devMode_;
};

std::string MimeTypeForExtension(const std::string& extension);

} // namespace avahost
