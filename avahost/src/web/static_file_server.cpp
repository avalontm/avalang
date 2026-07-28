#include "web/static_file_server.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace fs = std::filesystem;

namespace avahost {

std::string MimeTypeForExtension(const std::string& extension) {
    static const std::unordered_map<std::string, std::string> kMimeTypes = {
        {".html", "text/html; charset=utf-8"},
        {".htm",  "text/html; charset=utf-8"},
        {".css",  "text/css; charset=utf-8"},
        {".json", "application/json; charset=utf-8"},
        {".png",  "image/png"},
        {".jpg",  "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".gif",  "image/gif"},
        {".svg",  "image/svg+xml"},
        {".webp", "image/webp"},
        {".ico",  "image/x-icon"},
        {".woff", "font/woff"},
        {".woff2","font/woff2"},
        {".ttf",  "font/ttf"},
        {".txt",  "text/plain; charset=utf-8"},
    };
    auto it = kMimeTypes.find(extension);
    return it != kMimeTypes.end() ? it->second : "application/octet-stream";
}

namespace {
// Weak ETag from size + last-write-time -- cheap (no hashing/reading
// the file twice) and changes the instant a save touches the file, so
// it's safe to use even while `avahost run --watch` is live-editing.
std::string ComputeETag(const fs::path& filePath) {
    std::error_code ec;
    auto size = fs::file_size(filePath, ec);
    if (ec) return "";
    auto mtime = fs::last_write_time(filePath, ec);
    if (ec) return "";
    std::ostringstream tag;
    tag << "\"" << size << "-" << mtime.time_since_epoch().count() << "\"";
    return tag.str();
}
} // namespace

StaticFileServer::StaticFileServer(std::string wwwrootDir, bool devMode)
    : wwwrootDir_(std::move(wwwrootDir)), devMode_(devMode) {}

std::optional<HttpResponse> StaticFileServer::TryServe(const std::string& requestPath,
                                                        const std::string& ifNoneMatch) const {
    if (requestPath.find("..") != std::string::npos) return std::nullopt; // reject path traversal

    fs::path root = fs::path(wwwrootDir_);
    fs::path relative = requestPath;
    if (!relative.empty() && relative.generic_string().front() == '/') {
        relative = fs::path(relative.generic_string().substr(1));
    }
    fs::path fullPath = root / relative;

    // Path traversal guard: resolved path must stay inside wwwroot.
    std::error_code ec;
    fs::path canonicalRoot = fs::weakly_canonical(root, ec);
    fs::path canonicalFile = fs::weakly_canonical(fullPath, ec);
    if (ec) return std::nullopt;
    auto rootStr = canonicalRoot.string();
    auto fileStr = canonicalFile.string();
    if (fileStr.compare(0, rootStr.size(), rootStr) != 0) return std::nullopt;

    if (!fs::exists(fullPath) || !fs::is_regular_file(fullPath)) return std::nullopt;

    const std::string etag = ComputeETag(fullPath);
    const std::string cacheControl = devMode_ ? "no-cache" : "public, max-age=86400";

    // Efficient static file serving: ETag + If-None-Match headers let the
    // browser skip re-downloading unchanged files. The browser still asks on
    // every navigation, but a match costs a bodyless 304 instead of the full
    // file, and a real edit (new size/mtime -> new ETag) is picked up
    // immediately -- no separate cache-busting needed for --watch mode.
    if (!etag.empty() && !ifNoneMatch.empty() && ifNoneMatch == etag) {
        HttpResponse notModified;
        notModified.statusCode = 304;
        notModified.SetHeader("ETag", etag);
        notModified.SetHeader("Cache-Control", cacheControl);
        return notModified;
    }

    std::ifstream file(fullPath, std::ios::binary);
    if (!file) return std::nullopt;

    std::ostringstream contents;
    contents << file.rdbuf();

    HttpResponse response = HttpResponse::Text(200, contents.str(),
                                                MimeTypeForExtension(fullPath.extension().string()));
    if (!etag.empty()) response.SetHeader("ETag", etag);
    response.SetHeader("Cache-Control", cacheControl);
    return response;
}

} // namespace avahost
