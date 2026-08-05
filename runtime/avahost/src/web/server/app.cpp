#include "web/server/app.h"

#include <cctype>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#include <nlohmann/json.hpp>

#include "config/app_manifest.h"
#include "core/logger.h"
#include "rendering/event_binder.h"
#include "web/protocol/url_codec.h"

#ifdef AVAHOST_HAS_UI_PIPELINE
#include "rendering/ui_pipeline_dynamic_renderer.h"
#include "rendering/ui_pipeline_static_renderer.h"
#endif

namespace fs = std::filesystem;
using nlohmann::json;

namespace avahost {

namespace {
constexpr const char* kHotReloadEndpoint = "/__avahost/hotreload";
constexpr const char* kEventEndpoint = "/__avahost/event";
constexpr auto kHotReloadPollInterval = std::chrono::milliseconds(500);

std::string ReadFile(const std::string& path, bool& ok) {
    std::ifstream file(path, std::ios::binary);
    if (!file) { ok = false; return ""; }
    std::ostringstream contents;
    contents << file.rdbuf();
    ok = true;
    return contents.str();
}

uint64_t ComputeContentHash(const std::string& content) {
    return std::hash<std::string>{}(content);
}

// --- Responsive rendering (per-request viewport instead of a single
// fixed 1280x720 scaled with CSS -- this is "Fase C, opcion 2" from
// docs/AVAUI_NATIVE_RENDERING_FIX_PLAN.md, deferred at the time to its
// own phase; implemented here. The old HTMLRenderer transform: scale()
// approach never reflowed, it only stretched/shrunk the same 1280x720
// layout, so windows with a different aspect ratio either distorted
// (independent sx/sy, never actually used) or letterboxed (uniform
// scale, what shipped) with empty margin on one axis. Below,
// LayoutEngine::Compute instead runs against the browser's *actual*
// window.innerWidth/innerHeight (see avaui_vw/avaui_vh cookies), so
// Row/Column/grow are placed natively for that exact size -- no
// scaling, no distortion, no letterboxing, and HTMLRenderer renders
// `.ava-viewport` at 100%/100% instead of a fixed px box (see
// HTMLRenderer::EmitHTMLHeader/Footer). A resize big enough to matter
// (see kViewportResizeThresholdPx) round-trips through the same
// avaui_vw/avaui_vh + reload mechanism as the first-paint detection
// script below, so LayoutEngine actually re-runs at the new size
// instead of a client-side CSS transform faking it. ---

// Below this width (in the `avaui_vw` cookie, itself window.innerWidth
// from the browser) a request loads the project's hand-authored
// `*_mobile.avaui` variant instead of the desktop source (see
// ApplyMobileVariantOverrides below) -- LayoutEngineImpl has no
// wrap/reflow of its own, so a Row built for a 1280px-wide design
// doesn't automatically restack into something narrow-screen-shaped;
// swapping to an author-provided mobile variant is how AvaUI covers
// that instead. Matches common phone/tablet breakpoints (Tailwind's
// `md:` is 768); picked here rather than exposed as a CLI option
// because AvaUI has no per-project breakpoint config yet. Independent
// of viewport *sizing*, which below now always uses the browser's real
// window.innerWidth/innerHeight rather than a fixed mobile box -- this
// constant only decides which authored source file to render.
constexpr int kMobileBreakpointPx = 700;
constexpr const char* kViewportCookieName = "avaui_vw=";
constexpr const char* kViewportHeightCookieName = "avaui_vh=";

// A resize has to move the window by at least this many logical px on
// either axis before the client bothers updating the viewport
// cookie(s) and re-fetching (see the resize listener EventScriptTag()
// emits). Without a floor, continuous drag-resizing would trigger a
// fetch on every animation frame; this also caps how many distinct
// `cacheKey` viewport buckets bytecodeCache_ accumulates per session
// (see AvaHostApp::RenderAvaUiRoute's cacheKey, which already includes
// viewportWidth x viewportHeight) since the client rounds the reported
// size to this same granularity before writing the cookie. Single
// definition -- ViewportDetectScriptTag() (first paint) and
// EventScriptTag()'s resize listener (every later resize) both read
// this same constant directly, no cross-file/cross-library duplicate to
// keep in sync.
constexpr int kViewportResizeThresholdPx = 24;

// Name of the cookie that carries a browser's session id (see
// core/session_manager.h). HttpOnly (never readable from page JS --
// unlike avaui_vw, which the client-side script above deliberately
// does read/write itself) and SameSite=Lax, the same conservative
// defaults ASP.NET Core's session cookie and most other frameworks
// ship. No Secure flag: AvaHost's HttpServer has no TLS support to
// begin with (see web/server/http_server.cpp), so marking the cookie
// Secure would make browsers silently drop it on every request this
// server can actually receive.
constexpr const char* kSessionCookieName = "avahost_session";

// Generic single-cookie-value reader, used for the session cookie.
// (avaui_vw above predates this and keeps its own inline scan -- kept
// as-is rather than folded into this helper, since it also has to
// parse the value straight to an int and bail out cleanly on anything
// that isn't one; this one just needs the raw opaque string.) Returns
// empty if `name` isn't present in `cookieHeader` at all.
std::string TryReadCookieValue(const std::string& cookieHeader, const std::string& name) {
    const std::string needle = name + "=";
    size_t pos = cookieHeader.find(needle);
    while (pos != std::string::npos) {
        const bool atBoundary = (pos == 0) || (cookieHeader[pos - 1] == ' ');
        if (atBoundary) {
            const size_t valueStart = pos + needle.size();
            size_t valueEnd = cookieHeader.find(';', valueStart);
            return cookieHeader.substr(valueStart,
                valueEnd == std::string::npos ? std::string::npos : valueEnd - valueStart);
        }
        pos = cookieHeader.find(needle, pos + 1);
    }
    return "";
}

// Builds the Set-Cookie header value for `sessionId`, refreshed on
// every response (not only when the session is brand new) so the
// browser's own cookie expiry stays in sync with the server-side
// sliding TTL (SessionManager::ResolveSession already pushed back
// lastAccess for this request) -- otherwise the cookie could expire
// client-side before the still-alive server-side session does, and
// the browser would silently stop sending it.
std::string BuildSessionSetCookie(const std::string& sessionId, int ttlSeconds) {
    return std::string(kSessionCookieName) + "=" + sessionId +
           "; Path=/; Max-Age=" + std::to_string(ttlSeconds) +
           "; HttpOnly; SameSite=Lax";
}

// Reads `cookieName<int>` out of a raw "Cookie" request header (e.g.
// cookieName == "avaui_vw=" matches "avaui_vw=390; other=x"). Returns
// false (leaves outValue untouched) if the cookie is absent or not a
// plain integer -- callers then know to send ViewportDetectScriptTag()
// so the browser sets it and reloads once, instead of silently
// guessing a size. Shared by both the width (avaui_vw) and height
// (avaui_vh) viewport cookies -- previously each had its own bespoke
// parser (TryReadViewportCookie, width-only); unified when height was
// added so the two don't drift the way BuildImportMap's allowlist did
// (see known_component_properties.h's own comment on that).
bool TryReadIntCookie(const std::string& cookieHeader, const char* cookieName, int& outValue) {
    const std::string needle(cookieName);
    size_t pos = cookieHeader.find(needle);
    while (pos != std::string::npos) {
        const bool atBoundary = (pos == 0) || (cookieHeader[pos - 1] == ' ');
        if (atBoundary) {
            const size_t valueStart = pos + needle.size();
            size_t valueEnd = cookieHeader.find(';', valueStart);
            const std::string valueStr = cookieHeader.substr(
                valueStart, valueEnd == std::string::npos ? std::string::npos : valueEnd - valueStart);
            try {
                size_t consumed = 0;
                int parsed = std::stoi(valueStr, &consumed);
                if (consumed > 0) {
                    outValue = parsed;
                    return true;
                }
            } catch (...) {
                // fall through and keep scanning in case of a stray
                // match earlier in the header
            }
        }
        pos = cookieHeader.find(needle, pos + 1);
    }
    return false;
}

// Inline, blocking <head> script emitted only when the incoming
// request had no `avaui_vw`/`avaui_vh` cookies yet (first visit, or an
// expired one). Reads the real window.innerWidth/innerHeight, rounds
// each to kViewportResizeThresholdPx (same granularity the resize
// listener in HTMLRenderer::EmitHTMLFooter uses, so a fresh visit and a
// later resize land on the same cacheKey buckets instead of always
// missing bytecodeCache_), stores them, and reloads once so the *next*
// request -- this same route, now with both cookies set -- picks the
// right viewport server-side. Runs before the rest of <head> so the
// reload happens as early as possible; the desktop-sized first paint
// is a one-time cost on first visit, not on every navigation
// (subsequent requests, including the click-handler POSTs the event
// pipeline already round-trips on every interaction, carry the
// cookies).
std::string ViewportDetectScriptTag() {
    return
        "<script>\n"
        "(function(){\n"
        "  if (document.cookie.indexOf('avaui_vw=') !== -1) return;\n"
        "  var step = " + std::to_string(kViewportResizeThresholdPx) + ";\n"
        "  var w = Math.round(window.innerWidth / step) * step;\n"
        "  var h = Math.round(window.innerHeight / step) * step;\n"
        "  document.cookie = 'avaui_vw=' + w + '; path=/; max-age=86400; SameSite=Lax';\n"
        "  document.cookie = 'avaui_vh=' + h + '; path=/; max-age=86400; SameSite=Lax';\n"
        "  location.reload();\n"
        "})();\n"
        "</script>\n";
}

// Textual, pre-parse rewrite of a route's `extends X.Y` / `import X.Y`
// lines to `X.Y_mobile` when a sibling `X/Y_mobile.avaui` file exists
// on disk -- e.g. `extends layouts.main` becomes
// `extends layouts.main_mobile` if layouts/main_mobile.avaui exists.
// Runs only when a request resolved to the mobile breakpoint, and only
// on the top-level route source (nested layout/component files are
// parsed independently by UiComponentResolver and should reference
// their own *_mobile deps directly in their own text -- see
// samples/web/testproj/layouts/main_mobile.avaui). This lets a project
// opt individual layouts/components into a hand-authored mobile
// variant (LayoutEngineImpl::ArrangeRowOrColumn has no wrap -- a Row
// built for 1280px won't reflow into a stacked/hamburger layout on its
// own, it just overflows at 390px) without AvauiParser or
// UiComponentResolver ever needing to know breakpoints exist.
std::string ApplyMobileVariantOverrides(const std::string& source, const std::string& projectRoot) {
    auto fileExists = [&](const std::string& dotted) {
        fs::path p(projectRoot);
        std::string segment;
        for (char c : dotted) {
            if (c == '.') { p /= segment; segment.clear(); }
            else segment += c;
        }
        p /= segment;
        p += ".avaui";
        std::error_code ec;
        return fs::exists(p, ec);
    };

    std::istringstream in(source);
    std::ostringstream out;
    std::string line;
    while (std::getline(in, line)) {
        for (const std::string keyword : {std::string("extends "), std::string("import ")}) {
            const size_t firstNonSpace = line.find_first_not_of(" \t");
            if (firstNonSpace == std::string::npos) continue;
            if (line.compare(firstNonSpace, keyword.size(), keyword) != 0) continue;

            std::string dotted = line.substr(firstNonSpace + keyword.size());
            while (!dotted.empty() && std::isspace(static_cast<unsigned char>(dotted.back()))) {
                dotted.pop_back();
            }
            const std::string mobileSuffix = "_mobile";
            const bool alreadyMobile = dotted.size() >= mobileSuffix.size() &&
                dotted.compare(dotted.size() - mobileSuffix.size(), mobileSuffix.size(), mobileSuffix) == 0;
            if (dotted.empty() || alreadyMobile) {
                break;
            }
            if (fileExists(dotted + "_mobile")) {
                line = line.substr(0, firstNonSpace) + keyword + dotted + "_mobile";
            }
            break;
        }
        out << line << "\n";
    }
    return out.str();
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
      server_(options_.host, options_.port, logger, options_.workerThreads),
      watcher_(options_.projectRoot, {".ava", ".avaui"}),
      sessions_(options_.sessionTtlSeconds) {
    runtime_.SetCurrentDir(options_.projectRoot);
    runtime_.AddSearchPath(options_.routesDir);
    runtime_.AddSearchPath(options_.componentsDir);
    runtime_.AddSearchPath(options_.projectRoot + "/services");
    runtime_.AddSearchPath(options_.projectRoot + "/models");

    StartSessionReaper();

    plugins_.LoadAll(options_.pluginsDir, options_, logger_);

    if (options_.watch) StartHotReloadWatcher();
}

AvaHostApp::~AvaHostApp() {
    StopHotReloadWatcher();
    StopSessionReaper();
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
            return RenderAvaUiRoute(*errorPage, ctx, "", "", "", 404, request.HeaderOr("Cookie"),
                                     request.HeaderOr("User-Agent"));
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

    return match->isAvaUi
        ? RenderAvaUiRoute(match->filePath, ctx, "", "", "", 200, request.HeaderOr("Cookie"),
                           request.HeaderOr("User-Agent"))
        : RunScriptRoute(match->filePath, ctx);
}

HttpResponse AvaHostApp::RenderAvaUiRoute(const std::string& filePath, const RequestContext& ctx,
                                            const std::string& pendingHandler,
                                            const std::string& pendingCompId,
                                            const std::string& pendingValue,
                                            int statusCode,
                                            const std::string& cookieHeader,
                                            const std::string& userAgent) {
#ifndef AVAHOST_HAS_UI_PIPELINE
    logger_.Error("avaui render of " + filePath +
                   " requested but AvaHost was built without AVA_BUILD_UI");
    return HttpResponse::ServerError(ErrorResponseText(
        "avaui engine render of " + filePath,
        "AvaHost was built without AVA_BUILD_UI; the avaui rendering pipeline is unavailable"));
#else
    bool ok = false;
    std::string source = ReadFile(filePath, ok);
    if (!ok) {
        return HttpResponse::ServerError(ErrorResponseText("could not read route file", filePath));
    }

    // Pick a viewport size for LayoutEngine::Compute from the
    // `avaui_vw`/`avaui_vh` cookies (see TryReadIntCookie above)
    // instead of always defaulting to 1280x720, and instead of the old
    // two-bucket "desktop or exactly 390x844" choice: LayoutEngine now
    // always runs against the browser's actual (rounded) window size,
    // so Row/Column/grow reflow natively for whatever window the
    // person actually has open -- see the "Responsive rendering"
    // comment up top. No cookies yet -> render at the desktop default
    // (safe first paint) and ask the browser to report its real size
    // via ViewportDetectScriptTag, which reloads once.
    int viewportWidth = kDefaultViewportWidth;
    int viewportHeight = kDefaultViewportHeight;
    bool haveViewportCookie = false;
    {
        int cookieWidth = 0;
        int cookieHeight = 0;
        bool haveWidth = TryReadIntCookie(cookieHeader, kViewportCookieName, cookieWidth);
        bool haveHeight = TryReadIntCookie(cookieHeader, kViewportHeightCookieName, cookieHeight);
        if (haveWidth && haveHeight && cookieWidth > 0 && cookieHeight > 0) {
            haveViewportCookie = true;
            viewportWidth = cookieWidth;
            viewportHeight = cookieHeight;
            // Below the mobile breakpoint, still swap in the project's
            // hand-authored `*_mobile.avaui` variant (LayoutEngineImpl
            // has no automatic reflow into a narrow-screen layout on
            // its own) -- but size it against the real window, not a
            // fixed 390x844 phone box, so e.g. a tablet in portrait
            // still fills its actual height instead of Ietterboxing
            // inside an 844px-tall canvas.
            if (cookieWidth < kMobileBreakpointPx) {
                source = ApplyMobileVariantOverrides(source, options_.projectRoot);
            }
        }
    }

    runtime_.SetRequestContext(ctx);

    // Which browser this request belongs to -- see core/session_manager.h.
    // A missing/expired/unrecognized cookie value just means a new
    // session gets created here; the caller never has to distinguish
    // that from an existing one, only whether to send Set-Cookie below.
    const std::string incomingCookieId = TryReadCookieValue(cookieHeader, kSessionCookieName);
    bool isNewSession = false;
    const std::string sessionId = sessions_.ResolveSession(incomingCookieId, isNewSession);

    // Diagnostic-only, never used for routing/session decisions (a
    // spoofed/missing User-Agent must never affect which session a
    // request gets) -- just makes concurrent-browser log lines easier
    // to tell apart at a glance than the session id prefix alone.
    // Capped at 80 chars so one absurdly long UA string can't make a
    // log line unreadable; "unknown" covers curl and other clients
    // that send none at all.
    const std::string uaForLog =
        userAgent.empty() ? "unknown"
                           : (userAgent.size() > 80 ? userAgent.substr(0, 80) + "..." : userAgent);

    if (isNewSession) {
        // Only a short prefix, not the full id -- logs are often less
        // tightly guarded than the cookie jar itself, so the full
        // session id (which is also the bearer credential) shouldn't
        // end up sitting in a log file verbatim.
        logger_.Info("new session " + sessionId.substr(0, 8) + "... for " + filePath +
                      (incomingCookieId.empty()
                           ? " (no session cookie on request)"
                           : " (request cookie " + incomingCookieId.substr(0, 8) +
                                 "... unknown/expired, minted new)") +
                      " ua=\"" + uaForLog + "\"");
    }

    std::string cachedState;
    const bool haveCachedState = sessions_.TryGetState(sessionId, filePath, cachedState);
    const std::string cachedForCall = haveCachedState ? cachedState : std::string();

    // Per-request trace: which of possibly several concurrent browsers
    // this is (session id prefix + User-Agent) and whether it walked in
    // with previously-saved state for this route. Logged at Info level
    // so it shows up with the default ConsoleLogger (minLevel_ = Info)
    // with no config change -- the point is to let you eyeball two
    // tabs/browsers hitting the server and see two different sessions
    // (and which is Chrome vs Firefox, say) in the console, instead of
    // just trusting the fix worked.
    logger_.Info("request " + filePath + " session=" + sessionId.substr(0, 8) + "... " +
                 (isNewSession ? "[new]" : "[existing]") +
                 (haveCachedState ? " state=hit" : " state=miss") +
                 (pendingHandler.empty() ? "" : " handler=" + pendingHandler) +
                 " ua=\"" + uaForLog + "\"");

    AppManifest manifest = LoadAppManifest(options_.projectRoot);

    runtime_.BeginConsoleCapture();
    std::string outStateJson;
    std::string outHtml;
    std::string outError;
    
    // Bytecode cache: if no event handler and file unchanged, serve cached HTML
    uint64_t contentHash = ComputeContentHash(source);
    std::string cacheKey = filePath + "|" + std::to_string(viewportWidth) + "x" + std::to_string(viewportHeight);
    bool rendered = false;
    
    if (pendingHandler.empty() && !options_.watch &&
        bytecodeCache_.Get(cacheKey, contentHash, outHtml, outStateJson)) {
        rendered = true;
    } else {
        UiPipelineRenderOptions renderOptions;
        renderOptions.title = DeriveTitleFromPath(filePath, "AvaHost");
        renderOptions.viewportWidth = viewportWidth;
        renderOptions.viewportHeight = viewportHeight;
        renderOptions.extraHead = BuildHeadTags(manifest);
        if (!haveViewportCookie) renderOptions.extraHead += ViewportDetectScriptTag();
        if (options_.watch) renderOptions.extraHead += HotReloadScriptTag();
        renderOptions.componentsDir = options_.componentsDir;
        renderOptions.projectRoot = options_.projectRoot;
        rendered = RenderAvauiDynamicWithLayoutAndState(
            options_.projectRoot, runtime_, source,
            renderOptions,
            cachedForCall,
            pendingHandler,
            pendingCompId,
            pendingValue,
            outStateJson, outHtml, outError);
        
        // Cache on successful render (only non-watch mode)
        if (rendered && pendingHandler.empty() && !options_.watch) {
            bytecodeCache_.Set(cacheKey, contentHash, outHtml, outStateJson);
        }
    }
    std::string consoleOutput = runtime_.EndConsoleCapture();

    if (!rendered) {
        // Previously this failure only ever reached the client (via
        // ErrorResponseText, and only in Development) -- nothing was
        // logged server-side, so the only trace of *why* a page 500'd
        // was in the access log's plain "GET /x -> 500" line. This is
        // the counterpart to catching a hard crash: recoverable render
        // errors (bad .avaui, a handler that threw inside the VM, a
        // missing layout) should be just as visible in the log as one.
        logger_.Error("avaui render failed for " + filePath +
                       (pendingHandler.empty() ? "" : " (handler '" + pendingHandler + "')") +
                       ": " + outError);
        HttpResponse errorResponse = HttpResponse::ServerError(ErrorResponseText(
            "avaui engine render of " + filePath, outError));
        // The session was already created/resolved above even though
        // the render itself failed -- still send Set-Cookie so a
        // broken first request doesn't force every retry to mint (and
        // immediately abandon) a brand new session.
        errorResponse.SetHeader("Set-Cookie", BuildSessionSetCookie(sessionId, sessions_.TtlSeconds()));
        return errorResponse;
    }

    const std::string bodyEndTags = BuildBodyEndTags(manifest);
    const std::string eventScript = EventScriptTag(viewportWidth, viewportHeight);
    const std::string consoleScript = BuildConsoleScript(consoleOutput);
    if (!bodyEndTags.empty() || !eventScript.empty() || !consoleScript.empty()) {
        const std::string closeBody = "</body>\n</html>\n";
        const size_t closePos = outHtml.rfind(closeBody);
        if (closePos != std::string::npos) {
            outHtml.insert(closePos, bodyEndTags + eventScript + consoleScript);
        }
    }

    if (!outStateJson.empty()) {
        sessions_.SetState(sessionId, filePath, outStateJson);
        logger_.Info("saved state for " + filePath + " session=" + sessionId.substr(0, 8) + "...");
    }

    HttpResponse response = HttpResponse::Html(statusCode, outHtml);
    // Sent on every response, not only when isNewSession -- refreshes
    // the browser's own Max-Age so it stays in lockstep with the
    // sliding server-side TTL ResolveSession already extended for this
    // request (see BuildSessionSetCookie's comment).
    response.SetHeader("Set-Cookie", BuildSessionSetCookie(sessionId, sessions_.TtlSeconds()));
    return response;
#endif
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
    std::string compId = EventBinder::ExtractCompId(request.body);
    std::string controlValue = EventBinder::ExtractControlValue(request.body);

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

    return RenderAvaUiRoute(match->filePath, ctx, handlerName, compId, controlValue, 200,
                             request.HeaderOr("Cookie"), request.HeaderOr("User-Agent"));
}

HttpResponse AvaHostApp::RunScriptRoute(const std::string& filePath, const RequestContext& ctx) {
    bool ok = false;
    std::string source = ReadFile(filePath, ok);
    if (!ok) {
        logger_.Error("could not read route file " + filePath);
        return HttpResponse::ServerError(ErrorResponseText("could not read route file", filePath));
    }

    runtime_.SetRequestContext(ctx);

    std::string error;
    std::string output;
    bool success = runtime_.RunScriptCapturingOutput(source, filePath, output, error);
    if (!success) {
        // Same reasoning as RenderAvaUiRoute above: this used to reach
        // the client only (and only in Development) -- now it's also
        // logged server-side so a broken .ava route shows up in the log
        // even in Production, where the client just sees a generic 500.
        logger_.Error("error running " + filePath + ": " + error);
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
            // An uncaught exception on a background thread (unlike the
            // request-handling thread, which HttpServer::HandleConnection
            // already wraps) calls std::terminate -- by default that
            // takes the *whole process* down instantly and silently,
            // exactly like the crash this session started with, just
            // triggered by a filesystem quirk (e.g.
            // fs::recursive_directory_iterator throwing
            // fs::filesystem_error on a permission-denied entry or a
            // broken symlink under the project root) instead of a bug in
            // the render pipeline. Catching here keeps hot reload
            // degrading gracefully instead of taking the server with it;
            // core/crash_handler.h's std::set_terminate hook is still
            // there as a backstop for anything that somehow gets past
            // this too.
            try {
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
                        sessions_.EraseStateForFile(path);
                        bytecodeCache_.Invalidate(path);
                    }

                    reloadVersion_.fetch_add(1, std::memory_order_relaxed);
                });
            } catch (const std::exception& ex) {
                logger_.Error(std::string("hot-reload watcher: unhandled exception during poll, "
                                           "will keep retrying: ") + ex.what());
            } catch (...) {
                logger_.Error("hot-reload watcher: unhandled non-std exception during poll, "
                               "will keep retrying");
            }
            std::this_thread::sleep_for(kHotReloadPollInterval);
        }
    });
}

void AvaHostApp::StopHotReloadWatcher() {
    watcherRunning_ = false;
    if (watcherThread_.joinable()) watcherThread_.join();
}

namespace {
// How often the reaper wakes up to sweep expired sessions. Independent
// of options_.sessionTtlSeconds -- this is a polling granularity, not
// the expiry itself, so a session is reaped somewhere between TtlSeconds
// and TtlSeconds + this interval after its last access. A minute is
// frequent enough that memory from abandoned sessions doesn't linger
// noticeably, without waking the thread often enough to matter for CPU.
constexpr auto kSessionReapInterval = std::chrono::seconds(60);
// Sleep in short slices so Stop() can join promptly instead of
// blocking for up to a full kSessionReapInterval.
constexpr auto kSessionReapSleepSlice = std::chrono::milliseconds(200);
} // namespace

void AvaHostApp::StartSessionReaper() {
    sessionReaperRunning_ = true;
    sessionReaperThread_ = std::thread([this]() {
        while (sessionReaperRunning_) {
            auto slept = std::chrono::milliseconds(0);
            while (sessionReaperRunning_ && slept < kSessionReapInterval) {
                std::this_thread::sleep_for(kSessionReapSleepSlice);
                slept += kSessionReapSleepSlice;
            }
            if (!sessionReaperRunning_) break;

            // Same defensive wrapping as the hot-reload watcher above:
            // an uncaught exception on a background thread would
            // otherwise call std::terminate and take the whole process
            // down over what should be a routine cleanup pass.
            try {
                sessions_.ReapExpired();
            } catch (const std::exception& ex) {
                logger_.Error(std::string("session reaper: unhandled exception during sweep, "
                                           "will keep retrying: ") + ex.what());
            } catch (...) {
                logger_.Error("session reaper: unhandled non-std exception during sweep, "
                               "will keep retrying");
            }
        }
    });
}

void AvaHostApp::StopSessionReaper() {
    sessionReaperRunning_ = false;
    if (sessionReaperThread_.joinable()) sessionReaperThread_.join();
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

std::string AvaHostApp::EventScriptTag(int viewportWidth, int viewportHeight) const {
    // Client half of the Fase 2 event flow (rendering/event_binder.h /
    // the avaui pipeline's event bridge): delegated `click` listener on <body> so
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
    //
    // Fase C, opcion 2 (responsive resize) reuses the exact same
    // fetch-a-fresh-render-and-graft-it-in mechanism below (applyHtml())
    // instead of the old approach of a full location.reload() -- a
    // reload re-parses <head>, re-runs every <script> (including the
    // hot-reload poll, which briefly runs twice), and re-fetches every
    // asset, all of which this event flow already learned to avoid for
    // the click/data-handler case. Same fix, same code path, applied to
    // resize.
    std::ostringstream out;
    out << "<script>\n"
           "(function(){\n"
           "  function findHandler(el, domEventName){\n"
           "    while (el && el !== document.body) {\n"
           "      for (var n = 1; ; n++) {\n"
           "        var suffix = n === 1 ? '' : ('-' + n);\n"
           "        var evAttr = el.getAttribute('data-event' + suffix);\n"
           "        if (!evAttr) break;\n"
           "        if (evAttr === domEventName) {\n"
           "          var handler = el.getAttribute('data-handler' + suffix);\n"
           "          if (handler) return handler;\n"
           "        }\n"
           "      }\n"
           "      el = el.parentElement;\n"
           "    }\n"
           "    return null;\n"
           "  }\n"
           "  function applyHtml(html){\n"
           "    var next = new DOMParser().parseFromString(html, 'text/html');\n"
           "    if (next.title) document.title = next.title;\n"
           "    var nextViewport = next.getElementById('ava-viewport');\n"
           "    var viewport = document.getElementById('ava-viewport');\n"
           "    var scrollRoot = (nextViewport && viewport) ? viewport : document.body;\n"
           "    var oldScrollviews = scrollRoot.querySelectorAll('.ava-scrollview');\n"
           "    var savedScroll = [];\n"
           "    oldScrollviews.forEach(function(el){\n"
           "      savedScroll.push({top: el.scrollTop, left: el.scrollLeft});\n"
           "    });\n"
           "    if (nextViewport && viewport) {\n"
           "      viewport.innerHTML = nextViewport.innerHTML;\n"
           "    } else {\n"
           "      document.body.innerHTML = next.body.innerHTML;\n"
           "    }\n"
           "    var newScrollRoot = (nextViewport && viewport) ? viewport : document.body;\n"
           "    var newScrollviews = newScrollRoot.querySelectorAll('.ava-scrollview');\n"
           "    newScrollviews.forEach(function(el, i){\n"
           "      var pos = savedScroll[i];\n"
           "      if (!pos) return;\n"
           "      el.scrollTop = pos.top;\n"
           "      el.scrollLeft = pos.left;\n"
           "    });\n"
           "    return newScrollRoot;\n"
           "  }\n"
           "  function fireHandler(handler, preventDefault, ev, sourceEl){\n"
           "    if (preventDefault && ev) ev.preventDefault();\n"
           "    var body = 'handler=' + encodeURIComponent(handler);\n"
           "    var compId = sourceEl ? sourceEl.getAttribute('data-comp-id') : null;\n"
           "    var focusState = null;\n"
           "    if (compId) {\n"
           "      body += '&compId=' + encodeURIComponent(compId) +\n"
           "              '&value=' + encodeURIComponent(sourceEl.value);\n"
           "      if (document.activeElement === sourceEl) {\n"
           "        focusState = {\n"
           "          compId: compId,\n"
           "          start: (typeof sourceEl.selectionStart === 'number') ? sourceEl.selectionStart : null,\n"
           "          end: (typeof sourceEl.selectionEnd === 'number') ? sourceEl.selectionEnd : null\n"
           "        };\n"
           "      }\n"
           "    }\n"
           "    fetch('" << kEventEndpoint << "?path=' + encodeURIComponent(location.pathname), {\n"
           "      method: 'POST',\n"
           "      headers: {'Content-Type': 'application/x-www-form-urlencoded'},\n"
           "      body: body\n"
           "    }).then(function(r){ return r.text(); }).then(function(html){\n"
           "      var newScrollRoot = applyHtml(html);\n"
           "      if (focusState) {\n"
           "        var restored = newScrollRoot.querySelector('[data-comp-id=\"' + focusState.compId + '\"]');\n"
           "        if (restored) {\n"
           "          restored.focus();\n"
           "          if (focusState.start !== null && typeof restored.setSelectionRange === 'function') {\n"
           "            try { restored.setSelectionRange(focusState.start, focusState.end); } catch (e) {}\n"
           "          }\n"
           "        }\n"
           "      }\n"
           "    }).catch(function(){});\n"
           "  }\n"
           "  var eventMap = {\n"
           "    'click': 'click',\n"
           "    'onchange': 'change',\n"
           "    'oninput': 'input',\n"
           "    'onfocus': 'focusin',\n"
           "    'onblur': 'focusout',\n"
           "    'onkeydown': 'keydown',\n"
           "    'onkeyup': 'keyup',\n"
           "    'onmouseenter': 'mouseover',\n"
           "    'onmouseleave': 'mouseout',\n"
           "    'onsubmit': 'submit'\n"
           "  };\n"
           "  Object.keys(eventMap).forEach(function(avauiName){\n"
           "    var domEventName = eventMap[avauiName];\n"
           "    document.body.addEventListener(domEventName, function(ev){\n"
           "      var el = ev.target.closest('[data-handler],[data-handler-2],[data-handler-3],[data-handler-4],[data-handler-5]');\n"
           "      if (!el) return;\n"
           "      var handler = findHandler(el, avauiName);\n"
           "      if (!handler) return;\n"
           "      fireHandler(handler, avauiName === 'click' || avauiName === 'onsubmit', ev, el);\n"
           "    }, avauiName === 'onfocus' || avauiName === 'onblur');\n"
           "  });\n"
           "  document.addEventListener('load', function(ev){\n"
           "    var el = ev.target && ev.target.closest ? ev.target.closest('[data-handler],[data-handler-2],[data-handler-3],[data-handler-4],[data-handler-5]') : null;\n"
           "    if (!el) return;\n"
           "    var handler = findHandler(el, 'onload');\n"
           "    if (!handler) return;\n"
           "    fireHandler(handler, false, ev);\n"
           "  }, true);\n"
           "  document.addEventListener('error', function(ev){\n"
           "    var el = ev.target && ev.target.closest ? ev.target.closest('[data-handler],[data-handler-2],[data-handler-3],[data-handler-4],[data-handler-5]') : null;\n"
           "    if (!el) return;\n"
           "    var handler = findHandler(el, 'onerror');\n"
           "    if (!handler) return;\n"
           "    fireHandler(handler, false, ev);\n"
           "  }, true);\n"
           "\n"
           "  var resizeStep = " << kViewportResizeThresholdPx << ";\n"
           "  var lastW = " << viewportWidth << ", lastH = " << viewportHeight << ";\n"
           "  var resizeTimer = null;\n"
           "  function applyResize(){\n"
           "    var w = Math.round(window.innerWidth / resizeStep) * resizeStep;\n"
           "    var h = Math.round(window.innerHeight / resizeStep) * resizeStep;\n"
           "    if (w === lastW && h === lastH) return;\n"
           "    document.cookie = 'avaui_vw=' + w + '; path=/; max-age=86400; SameSite=Lax';\n"
           "    document.cookie = 'avaui_vh=' + h + '; path=/; max-age=86400; SameSite=Lax';\n"
           "    fetch(location.pathname + location.search).then(function(r){ return r.text(); }).then(function(html){\n"
           "      lastW = w; lastH = h;\n"
           "      applyHtml(html);\n"
           "    }).catch(function(){ location.reload(); });\n"
           "  }\n"
           "  window.addEventListener('resize', function(){\n"
           "    if (resizeTimer) clearTimeout(resizeTimer);\n"
           "    resizeTimer = setTimeout(applyResize, 200);\n"
           "  });\n"
           "})();\n"
           "</script>\n";
    return out.str();
}

} // namespace avahost