#include "config/app_manifest.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace avahost {

namespace {

std::string Trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

bool StartsWith(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

bool EndsWith(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// Parses one `import "location"` line. Returns false if the line
// isn't an import line at all (blank, comment, anything else --
// app.ava may grow real AvaLang bootstrap code around these lines in
// the future, so unrecognized lines are just skipped, not errors).
bool ParseImportLine(const std::string& rawLine, std::string& outLocation) {
    std::string line = Trim(rawLine);
    if (line.empty() || StartsWith(line, "#")) return false; // AvaLang line comment (grammar/AvaLang.g4)
    if (!StartsWith(line, "import")) return false;

    std::string rest = Trim(line.substr(std::string("import").size()));
    if (rest.size() < 2 || rest.front() != '"') return false;

    size_t closingQuote = rest.find('"', 1);
    if (closingQuote == std::string::npos) return false;

    outLocation = rest.substr(1, closingQuote - 1);
    return !outLocation.empty();
}

std::string ResolveHref(const ResourceImport& resource) {
    if (resource.IsUrl()) return resource.location;
    // Project-relative local path -> server-absolute static file URL
    // (StaticFileServer maps request paths directly onto wwwrootDir).
    return resource.location.front() == '/' ? resource.location : "/" + resource.location;
}

} // namespace

bool ResourceImport::IsUrl() const {
    return StartsWith(location, "http://") || StartsWith(location, "https://");
}

namespace {
// Local paths (no query string) still require a literal suffix -- an
// exact match is cheap and unambiguous. Only URLs get the looser,
// substring-based sniff below, since query strings/CDN paths hide the
// "real" extension (e.g. fonts.googleapis.com/css2?family=Inter).
bool LooksLikeCss(const std::string& location) {
    if (EndsWith(location, ".css")) return true;
    if (!StartsWith(location, "http://") && !StartsWith(location, "https://")) return false;
    // Heuristic for URL imports only: real stylesheet CDNs almost
    // always carry "css" as its own delimited token in the path/query
    // (Google Fonts' "/css2?family=...", "bootstrap.min.css",
    // Font Awesome's "all.css"). JS "shim" CDNs that generate CSS at
    // runtime never do -- but note @tailwindcss/browser and
    // cdn.tailwindcss.com both contain "css" *inside a word*
    // ("tailwind[css]"), so a bare substring search would wrongly
    // flag those as stylesheets. Requiring the char right before
    // "css" to be a non-letter (or the very start of the string)
    // rejects that case while still matching "/css2", ".css",
    // "-css.min" etc. Not foolproof -- a definitive answer needs the
    // real Content-Type from an HTTP request, which avahost doesn't
    // make -- but it covers the common CDN ecosystem without
    // requiring the manifest author to tag the import type by hand.
    size_t pos = 0;
    while ((pos = location.find("css", pos)) != std::string::npos) {
        bool boundaryOk = pos == 0 || !std::isalpha(static_cast<unsigned char>(location[pos - 1]));
        if (boundaryOk) return true;
        pos += 3;
    }
    return false;
}
} // namespace

bool ResourceImport::IsStylesheet() const {
    return LooksLikeCss(location);
}

bool ResourceImport::IsBodyScript() const {
    return !LooksLikeCss(location) && EndsWith(location, ".js");
}

bool ResourceImport::IsHeadScript() const {
    return !IsStylesheet() && !IsBodyScript();
}

AppManifest DefaultAppManifest() {
    // No .js entry here on purpose: AvaLang app logic lives in
    // .ava/.avaui and is compiled/run by the AvaLang runtime, not
    // written by hand as browser JS. IsBodyScript()/.js handling
    // stays available for the rare case a project needs a genuine
    // third-party JS *library* (not app logic), but nothing scaffolds
    // or defaults to one -- `avahost new` never creates a js/app.js
    // file (see cli_commands.cpp). The Tailwind entry below is
    // tooling (it generates CSS), not app code, so it's fine here.
    AppManifest manifest;
    manifest.resources.push_back({"css/app.css"});
    manifest.resources.push_back({"https://cdn.jsdelivr.net/npm/@tailwindcss/browser@4"});
    return manifest;
}

AppManifest LoadAppManifest(const std::string& projectRoot) {
    fs::path path = fs::path(projectRoot) / "app.ava";
    std::ifstream file(path);
    if (!file) return DefaultAppManifest(); // no app.ava -- fall back, don't fail startup

    AppManifest manifest;
    std::string line;
    while (std::getline(file, line)) {
        std::string location;
        if (ParseImportLine(line, location)) {
            manifest.resources.push_back({location});
        }
    }
    return manifest;
}

std::string BuildHeadTags(const AppManifest& manifest) {
    std::ostringstream out;
    // Pass 1: CDN/head scripts (e.g. Tailwind) -- these generate CSS at
    // runtime by scanning the DOM, so they must run before any
    // stylesheet or paint.
    for (const auto& resource : manifest.resources) {
        if (resource.IsHeadScript()) {
            out << "<script src=\"" << ResolveHref(resource) << "\"></script>\n";
        }
    }
    // Pass 2: stylesheets, always after pass 1 -- guarantees CSS loads
    // after Tailwind regardless of the order app.ava declares things
    // in, while staying in <head> so there's no flash of unstyled
    // content. No runtime JS needed for this -- AvaHost decides the
    // order server-side, same as it decides everything else.
    for (const auto& resource : manifest.resources) {
        if (resource.IsStylesheet()) {
            out << "<link rel=\"stylesheet\" href=\"" << ResolveHref(resource) << "\" />\n";
        }
    }
    return out.str();
}

std::string BuildBodyEndTags(const AppManifest& manifest) {
    std::ostringstream out;
    for (const auto& resource : manifest.resources) {
        if (!resource.IsBodyScript()) continue;
        out << "<script src=\"" << ResolveHref(resource) << "\"></script>\n";
    }
    return out.str();
}

} // namespace avahost
