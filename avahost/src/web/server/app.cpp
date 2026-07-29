#include "web/server/app.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#include <nlohmann/json.hpp>

#include "component/dotted_path.h"
#include "config/app_manifest.h"
#include "core/logger.h"
#include "rendering/event_binder.h"
#include "web/protocol/url_codec.h"

namespace fs = std::filesystem;
using nlohmann::json;

namespace avahost {

namespace {
constexpr const char* kHotReloadEndpoint = "/__avahost/hotreload";
constexpr const char* kEventEndpoint = "/__avahost/event";
constexpr auto kHotReloadPollInterval = std::chrono::milliseconds(500);

// AvaUiDocument::importsJson -- a JSON array of dotted import strings
// ("components.navbar", ...), see core/src/ui/avaui_text.cpp's
// ParseImportLines / ImportsToJson. Malformed/absent JSON just yields
// no imports rather than failing the page.
std::vector<std::string> ParseImportsJsonArray(const std::string& importsJson) {
    std::vector<std::string> out;
    try {
        json arr = json::parse(importsJson.empty() ? "[]" : importsJson);
        if (arr.is_array()) {
            for (const auto& v : arr) {
                if (v.is_string()) out.push_back(v.get<std::string>());
            }
        }
    } catch (const json::exception&) {
        // fall through, return what we have (empty)
    }
    return out;
}

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

// Turns whatever RuntimeHost::EndConsoleCapture() collected for this
// request (one `print(...)` call's text per line) into a <script> that
// replays it as `console.log(...)` in the browser -- print() alone
// only ever reached the server's own terminal, never anything visible
// from the page itself. Empty input -> empty output, so a page that
// never calls print() doesn't grow an empty <script> tag.
//
// Same code/data split as EventScriptTag()/HotReloadScriptTag(): the
// JS shell below is a fixed literal, never touched by request data.
// The only variable part -- the printed lines -- travels as a JSON
// array (nlohmann::json, already a dependency for appsettings.json),
// not as hand-built JS source. A real JSON encoder doesn't need a
// bespoke escape table the way JsStringLiteral did; that's the actual
// fix, not "avoiding JS" (writing to devtools' console always needs
// *some* JS -- that part isn't avoidable).
//
// One extra step: JSON's grammar allows a literal "</script>" inside
// a string, and HTML doesn't care that it's sitting inside a JS
// string -- it still closes the <script> tag early. The standard fix
// (same one Rails/Django use for JSON-in-HTML) is escaping "/" as
// "\/" in the dump, so "</script>" can never appear as those literal
// bytes.
std::string BuildConsoleScript(const std::string& capturedOutput) {
    if (capturedOutput.empty()) return "";

    json lines = json::array();
    size_t start = 0;
    while (start <= capturedOutput.size()) {
        size_t nl = capturedOutput.find('\n', start);
        std::string line = (nl == std::string::npos) ? capturedOutput.substr(start)
                                                       : capturedOutput.substr(start, nl - start);
        if (!line.empty()) lines.push_back(line);
        if (nl == std::string::npos) break;
        start = nl + 1;
    }
    if (lines.empty()) return "";

    std::string payload = lines.dump();
    std::string escaped;
    escaped.reserve(payload.size());
    for (char c : payload) {
        if (c == '/') escaped += "\\/";
        else escaped += c;
    }

    std::ostringstream out;
    out << "<script>\n"
           "(function(lines){\n"
           "  lines.forEach(function(line){ console.log(line); });\n"
           "})(" << escaped << ");\n"
           "</script>\n";
    return out.str();
}
} // namespace

AvaHostApp::AvaHostApp(HostOptions options, Logger& logger)
    : options_(std::move(options)),
      logger_(logger),
      router_(options_.routesDir, runtime_),
      staticFiles_(options_.wwwrootDir, options_.watch),
      componentResolver_(runtime_, options_.projectRoot, options_.componentsDir),
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

std::optional<std::string> AvaHostApp::FindErrorPage(int statusCode) const {
    fs::path candidate = fs::path(options_.layoutsDir) / (std::to_string(statusCode) + ".avaui");
    if (fs::exists(candidate) && fs::is_regular_file(candidate)) return candidate.string();
    return std::nullopt;
}

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

    // 0b. Fase 2 event dispatch endpoint -- always present (unlike Hot
    // Reload, this isn't gated on options_.watch; event handling is a
    // normal-run feature). Only POST is meaningful here; GET/HEAD fall
    // through and hit the router like any other unmatched path.
    if (request.method == "POST" && request.path == kEventEndpoint) {
        return HandleEventRoute(request);
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
        // layouts/404.avaui (if present) renders like any other page --
        // extends layouts.main, Navbar()/Footer(), the works -- instead
        // of the plain-text fallback below. See FindErrorPage's header
        // comment for why this lives under layoutsDir, not routesDir.
        if (auto errorPage = FindErrorPage(404)) {
            RequestContext ctx;
            ctx.method = request.method;
            ctx.path = request.path;
            ctx.query = ParseQueryString(request.query);
            return RenderAvaUiRoute(*errorPage, ctx, "", 404);
        }
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

HttpResponse AvaHostApp::RenderAvaUiRoute(const std::string& filePath, const RequestContext& ctx,
                                           const std::string& pendingHandler, int statusCode) {
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

    // `extends layouts.main` (docs/architecture/17_AVAUI_FILE_FORMAT.md)
    // -- dotted path resolved from projectRoot (component/dotted_path.h).
    // layoutDoc stays default (tree == nullptr) when the page has no
    // `extends`, or when the named layout file doesn't exist -- either
    // way we fall back to rendering the page alone rather than failing
    // the request.
    RuntimeHost::AvaUiDocument layoutDoc;
    if (!doc.extends.empty()) {
        fs::path layoutPath = ResolveDottedAvauiPath(options_.projectRoot, doc.extends);
        bool layoutOk = false;
        std::string layoutSource = ReadFile(layoutPath.string(), layoutOk);
        if (layoutOk) {
            layoutDoc = runtime_.ParseAvaUiFile(layoutSource);
            if (!layoutDoc.ok) {
                return HttpResponse::ServerError(
                    ErrorResponseText(".avaui parse error in " + layoutPath.string(), layoutDoc.error));
            }
        } else {
            // Previously failed silently: the page still rendered (via
            // the layoutDoc.tree == nullptr fallback in
            // HtmlRenderer::RenderDocumentWithLayout), just without its
            // layout and, with it, every bit of CSS/markup the layout
            // was supposed to contribute -- which looks identical to a
            // pure styling bug from the browser. Log it so a missing/
            // misnamed `extends` target shows up immediately instead of
            // being mistaken for one.
            GlobalLogger().Warn("'" + filePath + "': extends '" + doc.extends +
                                 "' but " + layoutPath.string() + " could not be read; rendering without layout");
        }
    }

    // Fase 2 module 1: resolve `Navbar()`-style component-call nodes in
    // both the page tree and (if present) the layout tree, against
    // each file's OWN `import components.navbar`-style lines, merging
    // each resolved component's own `state` into mergedStateJson as we
    // go (page state always wins on key collision -- see
    // ComponentResolver::ResolveImports).
    std::string mergedStateJson = doc.stateJson;
    componentResolver_.ResolveImports(doc.tree, mergedStateJson, ParseImportsJsonArray(doc.importsJson));
    if (layoutDoc.tree) {
        componentResolver_.ResolveImports(layoutDoc.tree, mergedStateJson,
                                           ParseImportsJsonArray(layoutDoc.importsJson));
    }

    // Overlay whatever this page last persisted (see stateCache_'s
    // header comment in app.h) on top of the file's defaults -- cached
    // values win for keys that exist in both, so a mutated `counter`
    // keeps accumulating across requests instead of every render
    // restarting it at the `state` block's initial 0. Keys the cache
    // doesn't have yet (first request ever, or a key just added to the
    // .avaui file after the cache was already populated) simply keep
    // whatever default ResolveImports/the page's own `state` produced
    // above.
    std::string cachedState;
    bool haveCachedState = false;
    {
        std::lock_guard<std::mutex> lock(stateCacheMutex_);
        auto cached = stateCache_.find(filePath);
        if (cached != stateCache_.end()) {
            cachedState = cached->second;
            haveCachedState = true;
        }
    }
    if (haveCachedState) {
        try {
            json merged = json::parse(mergedStateJson.empty() ? "{}" : mergedStateJson);
            json persisted = json::parse(cachedState);
            if (merged.is_object() && persisted.is_object()) {
                for (auto it = persisted.begin(); it != persisted.end(); ++it) {
                    merged[it.key()] = it.value();
                }
                mergedStateJson = merged.dump();
            }
        } catch (const json::exception&) {
            // Malformed cache entry (shouldn't happen -- it's only ever
            // written by ExportStateJson) -- fall back to defaults
            // rather than fail the request.
        }
    }

    // Fase 2 module 3: bind state + code-behind, dispatch the pending
    // handler (if this render was triggered by an event POST -- see
    // HandleEventRoute), then hand the renderer a text evaluator that
    // reflects whatever the handler mutated. Wrapped in a console
    // capture (BeginConsoleCapture/EndConsoleCapture) so any
    // `print(...)` call anywhere in this stretch -- BindCodeBehind,
    // OnLoad, or the click handler -- gets relayed to the browser's
    // console too, not just the server's stdout (see
    // BuildConsoleScript below).
    StateBinder stateBinder(runtime_);
    runtime_.BeginConsoleCapture();
    stateBinder.Bind(mergedStateJson, doc.methodsText);

    // Lifecycle hook: `OnLoad` (docs/architecture/17_AVAUI_FILE_FORMAT.md,
    // "Ciclo de vida") fires once per render, right after state/code-behind
    // are bound and before any click handler or the render itself -- both
    // a plain GET and a click-triggered POST count as "rendering a page"
    // here, since this host keeps no persistent page instance across
    // requests to distinguish an initial navigation from a later
    // interaction (see StateBinder's "state per-request" header comment).
    // `OnShow`/`OnHide`/`OnUnload` stay no-ops in AvaHost by design --
    // they describe a live/interactive host with a page instance that
    // can actually be shown/hidden/unloaded (Ava Studio's preview, or a
    // future stateful client runtime), which a stateless request/response
    // render never has.
    std::string lifecycleError;
    if (!stateBinder.DispatchLifecycle("OnLoad", lifecycleError)) {
        runtime_.EndConsoleCapture(); // discard -- request is failing anyway
        return HttpResponse::ServerError(
            ErrorResponseText("OnLoad in " + filePath, lifecycleError));
    }

    if (!pendingHandler.empty()) {
        std::string handlerError;
        if (!stateBinder.Dispatch(pendingHandler, handlerError)) {
            runtime_.EndConsoleCapture(); // discard -- request is failing anyway
            return HttpResponse::ServerError(
                ErrorResponseText("event handler '" + pendingHandler + "' in " + filePath, handlerError));
        }
    }
    std::string consoleOutput = runtime_.EndConsoleCapture();

    // Capture whatever BindState/Dispatch left on the VM's globals
    // (post-mutation, if a handler ran) back into stateCache_ so the
    // *next* request -- another event dispatch, or a plain page
    // refresh -- starts from here instead of the file's defaults.
    {
        std::lock_guard<std::mutex> lock(stateCacheMutex_);
        stateCache_[filePath] = runtime_.ExportStateJson(mergedStateJson);
    }

    RenderOptions renderOptions;
    renderOptions.title = DeriveTitleFromPath(filePath, "AvaHost");
    renderOptions.evalText = stateBinder.TextEvaluator();
    
    // Recursos globales (Tailwind CDN, css/app.css, y lo que declare el
    // proyecto) via app.ava -- ver config/app_manifest.h. Sin app.ava,
    // LoadAppManifest cae a DefaultAppManifest() y el resultado es el
    // mismo de siempre (Tailwind + css/app.css).
    AppManifest manifest = LoadAppManifest(options_.projectRoot);
    renderOptions.extraHead = BuildHeadTags(manifest);
    if (options_.watch) renderOptions.extraHead += HotReloadScriptTag();
    renderOptions.extraBodyEnd = BuildBodyEndTags(manifest);
    renderOptions.extraBodyEnd += EventScriptTag();
    renderOptions.extraBodyEnd += BuildConsoleScript(consoleOutput);
    std::string html = layoutDoc.tree
        ? renderer_.RenderDocumentWithLayout(doc.tree, layoutDoc.tree, renderOptions)
        : renderer_.RenderDocument(doc.tree, renderOptions);
    return HttpResponse::Html(statusCode, html);
}

HttpResponse AvaHostApp::HandleEventRoute(const HttpRequest& request) {
    auto queryParams = ParseQueryString(request.query);
    std::string pagePath;
    for (const auto& [key, value] : queryParams) {
        if (key == "path") { pagePath = value; break; }
    }
    if (pagePath.empty()) {
        return HttpResponse::Text(400, "400 Bad Request: missing ?path=");
    }

    std::string handlerName = EventBinder::ExtractHandlerName(request.body);
    if (handlerName.empty()) {
        return HttpResponse::Text(400, "400 Bad Request: missing 'handler' field");
    }

    auto match = router_.Resolve(pagePath);
    if (!match || !match->isAvaUi) {
        return HttpResponse::NotFound("404 Not Found: no page matches '" + pagePath + "'");
    }

    RequestContext ctx;
    ctx.method = "POST";
    ctx.path = pagePath;
    for (const auto& [name, rawValue] : match->params) {
        ctx.params.emplace_back(name, UrlDecode(rawValue));
    }
    ctx.query.clear();

    return RenderAvaUiRoute(match->filePath, ctx, handlerName);
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
                    // Editing state/imports/code in this file (or a
                    // component/layout it pulls in, if fs::path(path)
                    // matches a route file exactly -- component/layout
                    // edits aren't tracked back to every page that
                    // imports them here, so those still show stale
                    // cached values until that page's own file is
                    // touched too) shouldn't leave a stale accumulated
                    // value from before the edit sitting in
                    // stateCache_.
                    std::lock_guard<std::mutex> lock(stateCacheMutex_);
                    stateCache_.erase(path);
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

std::string AvaHostApp::EventScriptTag() const {
    // Client half of the Fase 2 event flow (rendering/event_binder.h /
    // runtime/state_binder.h): delegated `click` listener on <body> so
    // it also catches elements a handler re-renders in later (no
    // per-element re-binding needed). Any ancestor-or-self match on
    // [data-handler] POSTs {handler} to kEventEndpoint with this page's
    // own path as ?path=. The server always returns a full re-rendered
    // page (see RenderAvaUiRoute) -- that response contract hasn't
    // changed -- but the client now only swaps in the new <body>'s
    // content (via DOMParser) instead of replacing the whole document
    // with document.open/write/close. Reusing the same <body> element
    // (only its children change) means: the <head> -- CSS links, the
    // Tailwind CDN <script> -- never gets torn down and re-fetched, so
    // there's no flash of unstyled content; and this delegated listener
    // survives the swap (it's on the body element itself, not on any of
    // the children being replaced), so it doesn't need to be re-attached
    // after every update.
    std::ostringstream out;
    out << "<script>\n"
           "(function(){\n"
           "  document.body.addEventListener('click', function(ev){\n"
           "    var el = ev.target.closest('[data-handler]');\n"
           "    if (!el) return;\n"
           "    var handler = el.getAttribute('data-handler');\n"
           "    if (!handler) return;\n"
           "    ev.preventDefault();\n"
           "    fetch('" << kEventEndpoint << "?path=' + encodeURIComponent(location.pathname), {\n"
           "      method: 'POST',\n"
           "      headers: {'Content-Type': 'application/x-www-form-urlencoded'},\n"
           "      body: 'handler=' + encodeURIComponent(handler)\n"
           "    }).then(function(r){ return r.text(); }).then(function(html){\n"
           "      var next = new DOMParser().parseFromString(html, 'text/html');\n"
           "      if (next.title) document.title = next.title;\n"
           "      document.body.innerHTML = next.body.innerHTML;\n"
           "    }).catch(function(){});\n"
           "  });\n"
           "})();\n"
           "</script>\n";
    return out.str();
}

} // namespace avahost