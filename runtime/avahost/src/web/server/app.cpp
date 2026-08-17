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
#include "parser/AvauiParser.h"
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

constexpr int kMobileBreakpointPx = 700;
constexpr const char* kViewportCookieName = "avaui_vw=";
constexpr const char* kViewportHeightCookieName = "avaui_vh=";

constexpr int kViewportResizeThresholdPx = 24;

constexpr const char* kSessionCookieName = "avahost_session";

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

std::string BuildSessionSetCookie(const std::string& sessionId, int ttlSeconds) {
    return std::string(kSessionCookieName) + "=" + sessionId +
           "; Path=/; Max-Age=" + std::to_string(ttlSeconds) +
           "; HttpOnly; SameSite=Lax";
}

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

bool WriteStaticAssetIfMissing(const std::string& wwwrootDir, const std::string& relPath,
                                const std::string& content) {
    if (wwwrootDir.empty()) return false;
    fs::path filePath = fs::path(wwwrootDir) / relPath;
    std::error_code ec;
    if (fs::exists(filePath, ec)) return !ec;
    fs::create_directories(filePath.parent_path(), ec);
    if (ec) return false;
    std::ofstream out(filePath, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    return static_cast<bool>(out);
}

std::string ViewportDetectScriptTag(const std::string& wwwrootDir) {
    const std::string body =
        "(function(){\n"
        "  if (document.cookie.indexOf('avaui_vw=') !== -1) return;\n"
        "  var step = " + std::to_string(kViewportResizeThresholdPx) + ";\n"
        "  var w = Math.round(window.innerWidth / step) * step;\n"
        "  var h = Math.round(window.innerHeight / step) * step;\n"
        "  document.cookie = 'avaui_vw=' + w + '; path=/; max-age=86400; SameSite=Lax';\n"
        "  document.cookie = 'avaui_vh=' + h + '; path=/; max-age=86400; SameSite=Lax';\n"
        "  location.reload();\n"
        "})();\n";
    if (WriteStaticAssetIfMissing(wwwrootDir, "js/ava-viewport.js", body)) {
        return "<script src=\"/js/ava-viewport.js\"></script>\n";
    }
    return "<script>\n" + body + "</script>\n";
}

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

std::string DeriveTitleFromPath(const std::string& filePath, const std::string& siteFallback) {
    std::string stem = fs::path(filePath).stem().string();
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

    if (options_.watch && request.path == kHotReloadEndpoint) {
        HttpResponse response = HttpResponse::Text(200, std::to_string(reloadVersion_.load()));
        response.skipAccessLog = true;
        return response;
    }

    if (request.method == "POST" && request.path == kEventEndpoint) {
        return HandleEventRoute(request);
    }

    if (auto staticResponse = staticFiles_.TryServe(request.path, request.HeaderOr("If-None-Match"))) {
        return std::move(*staticResponse);
    }

    auto match = router_.Resolve(request.path);
    if (!match) {

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
                                            const std::string& userAgent,
                                            bool isErrorPage,
                                            const std::string& seedStateJson) {
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
            if (cookieWidth < kMobileBreakpointPx) {
                source = ApplyMobileVariantOverrides(source, options_.projectRoot);
            }
        }
    }

    runtime_.SetRequestContext(ctx);

    const std::string incomingCookieId = TryReadCookieValue(cookieHeader, kSessionCookieName);
    bool isNewSession = false;
    const std::string sessionId = sessions_.ResolveSession(incomingCookieId, isNewSession);

    const std::string uaForLog =
        userAgent.empty() ? "unknown"
                           : (userAgent.size() > 80 ? userAgent.substr(0, 80) + "..." : userAgent);

    if (isNewSession) {
        logger_.Info("new session " + sessionId.substr(0, 8) + "... for " + filePath +
                      (incomingCookieId.empty()
                           ? " (no session cookie on request)"
                           : " (request cookie " + incomingCookieId.substr(0, 8) +
                                 "... unknown/expired, minted new)") +
                      " ua=\"" + uaForLog + "\"");
    }

    std::string cachedState;
    const bool haveCachedState = sessions_.TryGetState(sessionId, filePath, cachedState);
    // seedStateJson (Fase: propagar error a 500.avaui) takes priority over
    // the session cache -- it's how RenderAvaUiRoute hands the error page
    // its errorMessage/errorFile/errorLine/errorColumn state on the way
    // in, and an error page has nothing meaningful of its own cached from
    // a prior successful render to prefer instead.
    const std::string cachedForCall =
        !seedStateJson.empty() ? seedStateJson : (haveCachedState ? cachedState : std::string());

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
    // Fase 3: filled in by RenderAvauiDynamicWithLayoutAndState only when
    // the failure was a genuine .avaui syntax error (ParseError), not a
    // runtime/binding failure -- parseErrorInfo.line stays 0 otherwise,
    // which is what the log formatting below checks.
    avalang::ui::parser::ParseErrorInfo parseErrorInfo;
    
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
        if (!haveViewportCookie) renderOptions.extraHead += ViewportDetectScriptTag(options_.wwwrootDir);
        if (options_.watch) renderOptions.extraHead += HotReloadScriptTag();
        renderOptions.componentsDir = options_.componentsDir;
        renderOptions.projectRoot = options_.projectRoot;
        renderOptions.wwwrootDir = options_.wwwrootDir;
        rendered = RenderAvauiDynamicWithLayoutAndState(
            options_.projectRoot, runtime_, source,
            renderOptions,
            cachedForCall,
            pendingHandler,
            pendingCompId,
            pendingValue,
            outStateJson, outHtml, outError,
            filePath, &parseErrorInfo);

        if (rendered && pendingHandler.empty() && !options_.watch) {
            bytecodeCache_.Set(cacheKey, contentHash, outHtml, outStateJson);
        }
    }
    std::string consoleOutput = runtime_.EndConsoleCapture();

    if (!rendered) {
        // Fase 3: when the failure was a real .avaui ParseError,
        // avahost-error.log now gets the same "file:line:col: message" +
        // offending line + "^~~~" caret shape .ava errors already get
        // (frontend_antlr.cpp's formatError) instead of just the bare
        // exception message. `source` is the route's own text, read
        // above -- good enough to underline errors in the route itself;
        // an error inside an imported component or `extends` layout
        // still logs with its own file:line:col header, just without a
        // source excerpt, since we don't have that file's text on hand
        // here.
        std::string logMessage = "avaui render failed for " + filePath +
                                  (pendingHandler.empty() ? "" : " (handler '" + pendingHandler + "')") +
                                  ": " + outError;
        if (parseErrorInfo.line > 0) {
            std::string excerptSource =
                (parseErrorInfo.source == filePath) ? source : std::string();
            logMessage += "\n" + avalang::ui::parser::FormatParseError(parseErrorInfo, excerptSource);
        }
        logger_.Error(logMessage);

        if (!isErrorPage) {
            if (auto errorPage = FindErrorPage(500)) {
                RequestContext errorCtx;
                errorCtx.method = ctx.method;
                errorCtx.path = ctx.path;
                errorCtx.query = ctx.query;

                // Propagate the failure into 500.avaui's own state instead
                // of only writing it to the log -- same shape as
                // logMessage above (message + file:line:col + caret
                // excerpt when it's a real ParseError), just split into
                // fields a .avaui page can bind to individually
                // (errorMessage/errorFile/errorLine/errorColumn) plus a
                // pre-formatted errorDetail for pages that just want to
                // dump the whole thing.
                json errorState = json::object();
                if (options_.IsDevelopment()) {
                    errorState["errorMessage"] = outError;
                    errorState["errorRoute"] = filePath;
                    std::string errorDetail = outError;
                    if (parseErrorInfo.line > 0) {
                        errorState["errorFile"] =
                            parseErrorInfo.source.empty() ? filePath : parseErrorInfo.source;
                        errorState["errorLine"] = std::to_string(parseErrorInfo.line);
                        errorState["errorColumn"] = std::to_string(parseErrorInfo.column);
                        std::string excerptSource =
                            (parseErrorInfo.source == filePath) ? source : std::string();
                        errorDetail += "\n" + avalang::ui::parser::FormatParseError(parseErrorInfo, excerptSource);
                    } else {
                        errorState["errorFile"] = filePath;
                        errorState["errorLine"] = "";
                        errorState["errorColumn"] = "";
                    }
                    errorState["errorDetail"] = errorDetail;
                } else {
                    // Production: 500.avaui still gets errorMessage so it
                    // can show *something*, but never the internals
                    // (file path, line/col, formatted excerpt) -- same
                    // boundary ErrorResponseText already draws for the
                    // non-avaui error path below.
                    errorState["errorMessage"] = "Internal Server Error";
                    errorState["errorRoute"] = "";
                    errorState["errorFile"] = "";
                    errorState["errorLine"] = "";
                    errorState["errorColumn"] = "";
                    errorState["errorDetail"] = "";
                }

                HttpResponse errorPageResponse = RenderAvaUiRoute(
                    *errorPage, errorCtx, "", "", "", 500, cookieHeader, userAgent,
                    /*isErrorPage=*/true, errorState.dump());
                errorPageResponse.SetHeader("Set-Cookie", BuildSessionSetCookie(sessionId, sessions_.TtlSeconds()));
                return errorPageResponse;
            }
        }

        HttpResponse errorResponse = HttpResponse::ServerError(ErrorResponseText(
            "avaui engine render of " + filePath, outError));
        errorResponse.SetHeader("Set-Cookie", BuildSessionSetCookie(sessionId, sessions_.TtlSeconds()));
        return errorResponse;
    }

    const std::string bodyEndTags = BuildBodyEndTags(manifest);
    const std::string eventScript = EventScriptTag();
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
        logger_.Error("error running " + filePath + ": " + error);
        return HttpResponse::ServerError(ErrorResponseText("error running " + filePath, error));
    }
    return HttpResponse::Html(200, output);
}

std::string AvaHostApp::ErrorResponseText(const std::string& context, const std::string& detail) const {
    if (options_.IsDevelopment()) {
        return "500 " + context + ": " + detail;
    }
    return "500 Internal Server Error";
}

void AvaHostApp::StartHotReloadWatcher() {
    watcherRunning_ = true;
    watcherThread_ = std::thread([this]() {
        while (watcherRunning_) {
            try {
                watcher_.PollOnce([this](const std::string& path) {
                    logger_.Info("changed: " + path + " -- reloading");

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
constexpr auto kSessionReapInterval = std::chrono::seconds(60);
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
    const std::string body = std::string(
        "(function(){\n"
        "  var v=null;\n"
        "  setInterval(function(){\n"
        "    fetch('") + kHotReloadEndpoint + "').then(function(r){return r.text();}).then(function(t){\n"
        "      if (v===null) { v=t; return; }\n"
        "      if (t!==v) { location.reload(); }\n"
        "    }).catch(function(){});\n"
        "  }, 1000);\n"
        "})();\n";
    if (WriteStaticAssetIfMissing(options_.wwwrootDir, "js/ava-hotreload.js", body)) {
        return "<script src=\"/js/ava-hotreload.js\"></script>\n";
    }
    return "<script>\n" + body + "</script>\n";
}

std::string AvaHostApp::EventScriptTag() const {
    std::ostringstream out;
    out << "(function(){\n"
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
           "    var closingClones = [];\n"
           "    if (nextViewport && viewport) {\n"
           "      var newIds = {};\n"
           "      nextViewport.querySelectorAll('.ava-overlay-fragment[data-dialog-id]').forEach(function(el){\n"
           "        newIds[el.getAttribute('data-dialog-id')] = true;\n"
           "      });\n"
           "      viewport.querySelectorAll('.ava-overlay-fragment[data-dialog-id]').forEach(function(el){\n"
           "        if (!newIds[el.getAttribute('data-dialog-id')]) closingClones.push(el.cloneNode(true));\n"
           "      });\n"
           "    }\n"
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
           "    closingClones.forEach(function(clone){\n"
           "      clone.classList.add('ava-dialog-closing');\n"
           "      newScrollRoot.appendChild(clone);\n"
           "      var done = false;\n"
           "      var finish = function(){\n"
           "        if (done) return;\n"
           "        done = true;\n"
           "        if (clone.parentNode) clone.parentNode.removeChild(clone);\n"
           "      };\n"
           "      clone.addEventListener('animationend', finish);\n"
           "      setTimeout(finish, 400);\n"
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
           "  function readViewportCookie(name, fallback){\n"
           "    var m = document.cookie.match(new RegExp('(?:^|; )' + name + '=([0-9]+)'));\n"
           "    return m ? parseInt(m[1], 10) : fallback;\n"
           "  }\n"
           "  var resizeStep = " << kViewportResizeThresholdPx << ";\n"
           "  var lastW = readViewportCookie('avaui_vw', 1280);\n"
           "  var lastH = readViewportCookie('avaui_vh', 720);\n"
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
           "})();\n";
    const std::string body = out.str();
    if (WriteStaticAssetIfMissing(options_.wwwrootDir, "js/ava-runtime.js", body)) {
        return "<script src=\"/js/ava-runtime.js\"></script>\n";
    }
    return "<script>\n" + body + "</script>\n";
}

} // namespace avahost