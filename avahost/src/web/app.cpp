#include "web/app.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "web/url_codec.h"

namespace fs = std::filesystem;

namespace avahost {

namespace {
constexpr const char* kHotReloadEndpoint = "/__avahost/hotreload";
constexpr auto kHotReloadPollInterval = std::chrono::milliseconds(500);

std::string ReadFile(const std::string& path, bool& ok) {
    std::ifstream file(path, std::ios::binary);
    if (!file) { ok = false; return ""; }
    std::ostringstream contents;
    contents << file.rdbuf();
    ok = true;
    return contents.str();
}

// `title` in `properties` (docs/architecture/17_AVAUI_FILE_FORMAT.md)
// is optional -- when a page doesn't declare one, this stands in
// instead of a fixed site-wide string, so an untitled page still gets
// something meaningful rather than every page saying "AvaHost".
// "index" on its own isn't a useful title, so it falls back to
// `siteFallback` (normally the site name) instead of literally "Index".
std::string DeriveTitleFromPath(const std::string& filePath, const std::string& siteFallback) {
    std::string stem = fs::path(filePath).stem().string(); // filename, no extension
    if (stem.empty()) return siteFallback;

    std::string lowerStem = stem;
    for (char& c : lowerStem) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
    if (lowerStem == "index") return siteFallback;

    std::string title;
    title.reserve(stem.size());
    bool startOfWord = true;
    for (char c : stem) {
        if (c == '-' || c == '_') {
            title += ' ';
            startOfWord = true;
            continue;
        }
        title += startOfWord ? static_cast<char>(::toupper(static_cast<unsigned char>(c))) : c;
        startOfWord = false;
    }
    return title;
}
} // namespace

AvaHostApp::AvaHostApp(HostOptions options, Logger& logger)
    : options_(std::move(options)),
      logger_(logger),
      router_(options_.routesDir, runtime_),
      staticFiles_(options_.wwwrootDir, options_.watch),
      server_(options_.host, options_.port, logger),
      watcher_(options_.projectRoot, {".ava", ".avaui"}) {
    runtime_.SetCurrentDir(options_.projectRoot);
    runtime_.AddSearchPath(options_.routesDir);
    runtime_.AddSearchPath(options_.componentsDir);
    runtime_.AddSearchPath(options_.projectRoot + "/services");
    runtime_.AddSearchPath(options_.projectRoot + "/models");

    plugins_.LoadAll(options_.pluginsDir, options_, logger_);

    if (options_.watch) StartHotReloadWatcher();
}

AvaHostApp::~AvaHostApp() {
    StopHotReloadWatcher();
    plugins_.UnloadAll();
}

bool AvaHostApp::Run() {
    return server_.Run([this](const HttpRequest& request) { return HandleRequest(request); });
}

void AvaHostApp::Stop() { server_.Stop(); }

HttpResponse AvaHostApp::HandleRequest(const HttpRequest& request) {
    if (request.method != "GET" && request.method != "HEAD" && request.method != "POST") {
        return HttpResponse::Text(405, "405 Method Not Allowed");
    }

    // 0. Hot Reload polling endpoint (plan section 15) -- only exists
    // when options_.watch is on, so it never appears as a route in a
    // normal `avahost run` (no "watch": true). The client script from
    // HotReloadScriptTag() polls this and reloads on the first change.
    if (options_.watch && request.path == kHotReloadEndpoint) {
        HttpResponse response = HttpResponse::Text(200, std::to_string(reloadVersion_.load()));
        response.skipAccessLog = true;
        return response;
    }

    // 1. Static files never go through AvaLang (plan section 13).
    if (auto staticResponse = staticFiles_.TryServe(request.path, request.HeaderOr("If-None-Match"))) {
        return std::move(*staticResponse);
    }

    // 2. Filename-convention routing (plan section 11) plus declared
    // `route "..."` templates for parameters (plan section 20 v0.2 --
    // see web/router.h).
    auto match = router_.Resolve(request.path);
    if (!match) {
        return HttpResponse::NotFound("404 Not Found: no static file or route matches '" + request.path + "'");
    }

    RequestContext ctx;
    ctx.method = request.method;
    ctx.path = request.path;
    // Router::RouteMatch::params carries raw path-segment text (router.h
    // never url-decodes -- it only ever compares against literal
    // filesystem names); decode here, same treatment as the query string.
    for (const auto& [name, rawValue] : match->params) {
        ctx.params.emplace_back(name, UrlDecode(rawValue));
    }
    ctx.query = ParseQueryString(request.query);

    return match->isAvaUi ? RenderAvaUiRoute(match->filePath, ctx) : RunScriptRoute(match->filePath, ctx);
}

HttpResponse AvaHostApp::RenderAvaUiRoute(const std::string& filePath, const RequestContext& ctx) {
    bool ok = false;
    std::string source = ReadFile(filePath, ok);
    if (!ok) {
        return HttpResponse::ServerError(ErrorResponseText("could not read route file", filePath));
    }

    runtime_.SetRequestContext(ctx);
    RuntimeHost::AvaUiDocument doc = runtime_.ParseAvaUiFile(source);
    if (!doc.ok) {
        return HttpResponse::ServerError(ErrorResponseText(".avaui parse error in " + filePath, doc.error));
    }

    // methods (event handlers) are parsed but not yet wired into request
    // handling -- that needs the state/event bridge from Ava Studio's
    // design/state_eval.cpp ported here, still plan section 20 v0.2
    // (Dependency Injection). `doc.extends` (layout) IS applied below.

    // `extends "name"` (docs/architecture/17_AVAUI_FILE_FORMAT.md)
    // resolves to layouts/name.avaui. layoutDoc stays default (tree ==
    // nullptr) when the page has no `extends`, or when the named
    // layout file doesn't exist -- either way we fall back to
    // rendering the page alone rather than failing the request.
    RuntimeHost::AvaUiDocument layoutDoc;
    if (!doc.extends.empty()) {
        fs::path layoutPath = fs::path(options_.layoutsDir) / (doc.extends + ".avaui");
        bool layoutOk = false;
        std::string layoutSource = ReadFile(layoutPath.string(), layoutOk);
        if (layoutOk) {
            layoutDoc = runtime_.ParseAvaUiFile(layoutSource);
            if (!layoutDoc.ok) {
                return HttpResponse::ServerError(
                    ErrorResponseText(".avaui parse error in " + layoutPath.string(), layoutDoc.error));
            }
        }
    }

    RenderOptions renderOptions;
    renderOptions.title = DeriveTitleFromPath(filePath, "AvaHost");
    
    // Inyectar CSS por defecto: Tailwind CDN + app.css local
    std::ostringstream headTags;
    headTags << "<script src=\"https://cdn.jsdelivr.net/npm/@tailwindcss/browser@4\"></script>\n";
    
    // Si wwwroot/css/app.css existe, incluirlo también
    fs::path appCssPath = fs::path(options_.wwwrootDir) / "css" / "app.css";
    if (fs::exists(appCssPath)) {
        headTags << "<link href=\"/css/app.css\" rel=\"stylesheet\" />\n";
    }
    
    renderOptions.extraHead = headTags.str();
    if (options_.watch) renderOptions.extraHead += HotReloadScriptTag();
    std::string html = layoutDoc.tree
        ? renderer_.RenderDocumentWithLayout(doc.tree, layoutDoc.tree, renderOptions)
        : renderer_.RenderDocument(doc.tree, renderOptions);
    return HttpResponse::Html(200, html);
}

HttpResponse AvaHostApp::RunScriptRoute(const std::string& filePath, const RequestContext& ctx) {
    bool ok = false;
    std::string source = ReadFile(filePath, ok);
    if (!ok) {
        return HttpResponse::ServerError(ErrorResponseText("could not read route file", filePath));
    }

    runtime_.SetRequestContext(ctx);

    std::string error;
    std::string output;
    bool success = runtime_.RunScriptCapturingOutput(source, filePath, output, error);
    if (!success) {
        return HttpResponse::ServerError(ErrorResponseText("error running " + filePath, error));
    }
    return HttpResponse::Html(200, output);
}

std::string AvaHostApp::ErrorResponseText(const std::string& context, const std::string& detail) const {
    // Detailed compiler/runtime errors are a Development convenience
    // (appsettings.json "environment") -- Production must never leak
    // source paths, script internals or stack-shaped messages to an
    // HTTP client.
    if (options_.IsDevelopment()) {
        return "500 " + context + ": " + detail;
    }
    return "500 Internal Server Error";
}

void AvaHostApp::StartHotReloadWatcher() {
    watcherRunning_ = true;
    watcherThread_ = std::thread([this]() {
        while (watcherRunning_) {
            watcher_.PollOnce([this](const std::string& path) {
                logger_.Info("changed: " + path + " -- reloading");

                // The declared-routes table (route "..." lines) is the
                // only thing Router builds once at startup; a script's
                // own content is already re-read from disk on every
                // request (see ReadFile calls above), so no separate
                // "recompile" step is needed for that plan section 15
                // step -- it happens naturally on the next request.
                std::string ext = fs::path(path).extension().string();
                if (ext == ".avaui" || ext == ".ava") {
                    router_.Rescan();
                }

                reloadVersion_.fetch_add(1, std::memory_order_relaxed);
            });
            std::this_thread::sleep_for(kHotReloadPollInterval);
        }
    });
}

void AvaHostApp::StopHotReloadWatcher() {
    watcherRunning_ = false;
    if (watcherThread_.joinable()) watcherThread_.join();
}

std::string AvaHostApp::HotReloadScriptTag() const {
    // Plain HTTP polling, not WebSockets (plan section 3 excludes
    // WebSockets from v1) -- checks kHotReloadEndpoint once a second
    // and reloads the page the first time the version differs from the
    // one seen when the page loaded. `v` starts null so the page's own
    // first load never triggers a reload against itself.
    return std::string(
        "<script>\n"
        "(function(){\n"
        "  var v=null;\n"
        "  setInterval(function(){\n"
        "    fetch('") + kHotReloadEndpoint + "').then(function(r){return r.text();}).then(function(t){\n"
        "      if (v===null) { v=t; return; }\n"
        "      if (t!==v) { location.reload(); }\n"
        "    }).catch(function(){});\n"
        "  }, 1000);\n"
        "})();\n"
        "</script>\n";
}

} // namespace avahost