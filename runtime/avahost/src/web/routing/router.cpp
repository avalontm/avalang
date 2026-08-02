#include "web/routing/router.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace avahost {

namespace {

// "/" -> {}; "/products" -> {"products"}; "/products/42/" -> {"products","42"}.
// A trailing slash is equivalent to no trailing slash (both collapse to
// the same segment list) -- matches ordinary web server semantics
// closely enough for this host. Also used to split a `route "..."`
// template into segments, since the same "/a/b/c" shape applies there.
std::vector<std::string> SplitPath(const std::string& path) {
    std::vector<std::string> segments;
    std::string current;
    for (char c : path) {
        if (c == '/') {
            if (!current.empty()) { segments.push_back(current); current.clear(); }
        } else {
            current += c;
        }
    }
    if (!current.empty()) segments.push_back(current);
    return segments;
}

bool ReadFile(const fs::path& path, std::string& outText) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    std::ostringstream contents;
    contents << file.rdbuf();
    outText = contents.str();
    return true;
}

// Known route constraints (docs/architecture/17_AVAUI_FILE_FORMAT.md
// lists int/long/guid/slug/alpha as examples, "..." for more). An
// unrecognized constraint name matches anything -- this router only
// rejects a captured value when it KNOWS the constraint it's checking,
// same "fail open on the unknown" spirit as the rest of this parser
// (see avaui_text.cpp's forgiving-by-default parsing).
bool IsDigits(const std::string& s) {
    if (s.empty()) return false;
    return std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isdigit(c) != 0; });
}

bool MatchesConstraint(const std::string& constraint, const std::string& value) {
    if (constraint.empty()) return true;
    if (constraint == "int" || constraint == "long") {
        return IsDigits(value);
    }
    if (constraint == "guid") {
        // 8-4-4-4-12 hex digits, e.g. 3fa85f64-5717-4562-b3fc-2c963f66afa6.
        static const size_t kLen[] = {8, 4, 4, 4, 12};
        std::vector<std::string> parts;
        std::string current;
        for (char c : value) {
            if (c == '-') { parts.push_back(current); current.clear(); }
            else current += c;
        }
        parts.push_back(current);
        if (parts.size() != 5) return false;
        for (size_t i = 0; i < 5; ++i) {
            if (parts[i].size() != kLen[i]) return false;
            for (unsigned char c : parts[i]) {
                if (!std::isxdigit(c)) return false;
            }
        }
        return true;
    }
    if (constraint == "alpha") {
        return !value.empty() && std::all_of(value.begin(), value.end(),
            [](unsigned char c) { return std::isalpha(c) != 0; });
    }
    if (constraint == "slug") {
        // lowercase letters, digits, and hyphens; no leading/trailing/
        // doubled hyphen.
        if (value.empty() || value.front() == '-' || value.back() == '-') return false;
        char prev = '\0';
        for (unsigned char c : value) {
            const bool ok = std::islower(c) || std::isdigit(c) || c == '-';
            if (!ok || (c == '-' && prev == '-')) return false;
            prev = static_cast<char>(c);
        }
        return true;
    }
    return true; // unrecognized constraint name -- don't reject on it
}

} // namespace

Router::Router(std::string routesDir, const RuntimeHost& runtime)
    : routesDir_(std::move(routesDir)), runtime_(&runtime) {
    ScanDeclaredRoutes(runtime);
}

void Router::Rescan() {
    std::lock_guard<std::mutex> lock(mutex_);
    declaredRoutes_.clear();
    ScanDeclaredRoutes(*runtime_);
}

void Router::ScanDeclaredRoutes(const RuntimeHost& runtime) {
    if (!fs::exists(routesDir_)) return;

    for (const auto& entry : fs::recursive_directory_iterator(routesDir_)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension().string() != ".avaui") continue; // only .avaui supports `route`

        std::string text;
        if (!ReadFile(entry.path(), text)) continue;

        for (const auto& parsed : runtime.ParseRouteDeclarations(text)) {
            DeclaredRoute route;
            route.pathTemplate = parsed.pathTemplate;
            route.filePath = entry.path().string();

            for (const auto& templateSeg : SplitPath(parsed.pathTemplate)) {
                // A param occupies a whole segment: "{name}", "{name?}",
                // "{name:constraint}" -- same shape ParseRouteLines'
                // kParamRe produces, one RouteParam per `{...}` found
                // left-to-right in the template. Segments that aren't
                // themselves a single "{...}" token are literal text.
                bool matchedParam = false;
                if (templateSeg.size() >= 2 && templateSeg.front() == '{' && templateSeg.back() == '}') {
                    std::string inner = templateSeg.substr(1, templateSeg.size() - 2);
                    for (const auto& param : parsed.params) {
                        std::string expected = param.name + (param.optional ? "?" : "");
                        if (!param.constraint.empty()) expected += ":" + param.constraint;
                        if (inner == expected) {
                            TemplateSegment seg;
                            seg.isParam = true;
                            seg.paramName = param.name;
                            seg.optional = param.optional;
                            seg.constraint = param.constraint;
                            route.segments.push_back(std::move(seg));
                            matchedParam = true;
                            break;
                        }
                    }
                }
                if (!matchedParam) {
                    TemplateSegment seg;
                    seg.isParam = false;
                    seg.literal = templateSeg;
                    route.segments.push_back(std::move(seg));
                }
            }

            declaredRoutes_.push_back(std::move(route));
        }
    }
}

namespace {

// Tries to match `segments` against `templateSegs`, honoring a
// trailing optional param (it may be present or entirely absent from
// the request path). Returns true and fills `outParams` on success.
bool MatchTemplate(const std::vector<Router::TemplateSegment>& templateSegs,
                    const std::vector<std::string>& segments,
                    std::vector<std::pair<std::string, std::string>>& outParams) {
    outParams.clear();
    size_t ti = 0, si = 0;
    while (ti < templateSegs.size()) {
        const auto& seg = templateSegs[ti];
        const bool lastTemplateSeg = (ti + 1 == templateSegs.size());

        if (si >= segments.size()) {
            // Ran out of request path -- only OK if every remaining
            // template segment is a trailing optional param.
            if (seg.isParam && seg.optional && lastTemplateSeg) return true;
            return false;
        }

        if (seg.isParam) {
            const std::string& value = segments[si];
            if (!MatchesConstraint(seg.constraint, value)) return false;
            outParams.emplace_back(seg.paramName, value);
            ++si;
        } else {
            if (segments[si] != seg.literal) return false;
            ++si;
        }
        ++ti;
    }
    return si == segments.size();
}

} // namespace

std::optional<RouteMatch> Router::Resolve(const std::string& requestPath) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> segments = SplitPath(requestPath);

    // 1. Declared `route "..."` templates, in file-scan order.
    for (const auto& route : declaredRoutes_) {
        std::vector<std::pair<std::string, std::string>> params;
        if (MatchTemplate(route.segments, segments, params)) {
            RouteMatch match;
            match.filePath = route.filePath;
            match.isAvaUi = true;
            match.params = std::move(params);
            return match;
        }
    }

    // 2. Filename convention -- literal directories/files only, index
    // for a directory whose own path was requested. No "[name]"
    // bracket support: that convention doesn't exist in this
    // language, dynamic segments are declared via `route` instead.
    fs::path dir(routesDir_);
    for (size_t i = 0; i < segments.size(); ++i) {
        const bool isLast = (i + 1 == segments.size());
        fs::path candidate = dir / segments[i];

        if (isLast) {
            for (const auto& [ext, isAvaUi] : std::vector<std::pair<std::string, bool>>{{".avaui", true}, {".ava", false}}) {
                fs::path file = candidate;
                file += ext;
                if (fs::exists(file) && fs::is_regular_file(file)) {
                    RouteMatch match;
                    match.filePath = file.string();
                    match.isAvaUi = isAvaUi;
                    return match;
                }
            }
        }

        if (!fs::exists(candidate) || !fs::is_directory(candidate)) return std::nullopt;
        dir = candidate;
    }

    // Ran out of path (covers "/" and any directory whose "index" file
    // serves the directory's own path) -- try dir/index.
    for (const auto& [ext, isAvaUi] : std::vector<std::pair<std::string, bool>>{{".avaui", true}, {".ava", false}}) {
        fs::path file = dir / "index";
        file += ext;
        if (fs::exists(file) && fs::is_regular_file(file)) {
            RouteMatch match;
            match.filePath = file.string();
            match.isAvaUi = isAvaUi;
            return match;
        }
    }
    return std::nullopt;
}

std::vector<RouteMatch> Router::AllRouteFiles() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<RouteMatch> files;

    for (const auto& route : declaredRoutes_) {
        RouteMatch match;
        match.filePath = route.filePath;
        match.isAvaUi = true;
        files.push_back(std::move(match));
    }

    if (!fs::exists(routesDir_)) return files;

    for (const auto& entry : fs::recursive_directory_iterator(routesDir_)) {
        if (!entry.is_regular_file()) continue;
        auto ext = entry.path().extension().string();
        if (ext != ".avaui" && ext != ".ava") continue;

        const std::string filePath = entry.path().string();
        bool hasDeclaredRoute = std::any_of(declaredRoutes_.begin(), declaredRoutes_.end(),
            [&](const DeclaredRoute& r) { return r.filePath == filePath; });
        if (hasDeclaredRoute) continue;

        RouteMatch match;
        match.filePath = filePath;
        match.isAvaUi = (ext == ".avaui");
        files.push_back(std::move(match));
    }
    return files;
}

std::vector<std::string> Router::ListRoutes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> routes;

    for (const auto& route : declaredRoutes_) {
        routes.push_back(route.pathTemplate);
    }

    if (!fs::exists(routesDir_)) return routes;

    for (const auto& entry : fs::recursive_directory_iterator(routesDir_)) {
        if (!entry.is_regular_file()) continue;
        auto ext = entry.path().extension().string();
        if (ext != ".avaui" && ext != ".ava") continue;

        // Skip .avaui files already covered by a declared route -- a
        // file with `route` lines is reached only through those, not
        // through its filename (see router.h).
        const std::string filePath = entry.path().string();
        bool hasDeclaredRoute = std::any_of(declaredRoutes_.begin(), declaredRoutes_.end(),
            [&](const DeclaredRoute& r) { return r.filePath == filePath; });
        if (hasDeclaredRoute) continue;

        fs::path relative = fs::relative(entry.path(), routesDir_);
        relative.replace_extension();
        std::string routePath = "/" + relative.generic_string();

        // routes/index -> "/", routes/products/index -> "/products"
        const std::string suffix = "/index";
        if (routePath.size() >= suffix.size() &&
            routePath.compare(routePath.size() - suffix.size(), suffix.size(), suffix) == 0) {
            routePath = routePath.substr(0, routePath.size() - suffix.size());
            if (routePath.empty()) routePath = "/";
        }
        routes.push_back(routePath);
    }
    return routes;
}

} // namespace avahost
