#pragma once
// Wires the full request pipeline from the plan's high-level
// architecture (section 5):
//   HTTP Listener -> Static Files -> Route Resolver -> Runtime ->
//   Component Tree -> HTML Renderer -> HTTP Response
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>

#include "core/host_options.h"
#include "core/logger.h"
#include "core/session_manager.h"
#include "plugin/plugin_loader.h"
#include "rendering/ui_bytecode_cache.h"
#include "runtime/runtime_host.h"
#include "watch/file_watcher.h"
#include "web/server/http_server.h"
#include "web/routing/router.h"
#include "web/static/static_file_server.h"

namespace avahost {

class AvaHostApp {
public:
    AvaHostApp(HostOptions options, Logger& logger);
    ~AvaHostApp();

    // Blocks running the HTTP server until stopped.
    bool Run();

    void Stop();

private:
    HttpResponse HandleRequest(const HttpRequest& request);

    // `pendingHandler`, when non-empty, is dispatched against this
    // render's bound state before the page is rendered -- the Fase 2
    // event flow (HandleEventRoute below). Empty
    // for an ordinary GET, which just binds+renders with no dispatch.
    // `statusCode` is the HTTP status the *response* is sent with --
    // independent of pendingHandler/dispatch. Defaults to 200 for an
    // ordinary page; FindErrorPage's caller passes e.g. 404 so a custom
    // error page still reports the right status to the client (a
    // 404.avaui rendered as 200 would look fine in a browser tab but
    // break anything checking the status code, like a health check or
    // curl script).
    // `cookieHeader` is the raw incoming "Cookie" request header (may
    // be empty). Two independent things are read out of it here:
    //  - the `avaui_vw` value this same route writes on first visit
    //    (see ViewportDetectScriptTag/TryReadViewportCookie in
    //    app.cpp) so the render picks a mobile or desktop viewport
    //    size for LayoutEngine::Compute instead of always using the
    //    1280x720 default -- purely a rendering decision, never
    //    influences routing/state.
    //  - the `avahost_session` value (see kSessionCookieName/
    //    TryReadCookieValue in app.cpp) identifying which browser's
    //    page state to bind against (sessions_, core/session_manager.h)
    //    instead of the old single shared stateCache_.
    // `userAgent` is the raw incoming "User-Agent" request header (may
    // be empty, e.g. curl with none set). Purely diagnostic -- logged
    // alongside the session id so two different browsers/tabs sharing
    // a machine (and therefore not obviously distinguishable by IP
    // alone) show up as recognizably different lines in the console.
    // Never used for routing/rendering decisions, only for logger_
    // calls in RenderAvaUiRoute.
    HttpResponse RenderAvaUiRoute(const std::string& filePath, const RequestContext& ctx,
                                   const std::string& pendingHandler = "",
                                   const std::string& pendingCompId = "",
                                   const std::string& pendingValue = "",
                                   int statusCode = 200,
                                   const std::string& cookieHeader = "",
                                   const std::string& userAgent = "",
                                   bool isErrorPage = false);
    HttpResponse RunScriptRoute(const std::string& filePath, const RequestContext& ctx);

    // Custom error pages, same convention-over-configuration spirit as
    // routes/index.avaui, but stored under layoutsDir (default
    // "layouts") -- not routesDir -- so an error page (layouts/404.avaui,
    // layouts/500.avaui) is never itself reachable as an ordinary
    // filename-convention route (visiting "/404" directly wouldn't
    // resolve, same as "/main" for layouts/main.avaui today). If
    // present, the file is rendered exactly like any other page --
    // same `extends layouts.main`, same Navbar()/Footer(), same
    // state/code-behind -- instead of the plain-text fallback. Returns
    // nullopt when no such file exists, which callers treat as "keep
    // the old plain-text response" so a project that never adds one of
    // these files sees no behavior change at all.
    std::optional<std::string> FindErrorPage(int statusCode) const;

    // Fase 2 module 2/3 (rendering/event_binder.h, the avaui pipeline's
    // state/event bridge): POST /__avahost/event?path=<pagina>, body
    // `handler=OnX`. Resolves `path` the same way an ordinary GET to
    // that path would (router_.Resolve), then re-renders it with the
    // named handler dispatched first -- the response IS the new page
    // HTML, which the client script from EventScriptTag() swaps into
    // #ava-viewport via DOMParser (see EventScriptTag's applyHtml()).
    HttpResponse HandleEventRoute(const HttpRequest& request);

    // <script> tag, injected alongside HotReloadScriptTag(), that POSTs
    // to /__avahost/event whenever an element with [data-handler] fires
    // its bound [data-event] and swaps the response in as the new
    // #ava-viewport content (via DOMParser) -- the client half of the
    // Fase 2 event flow. Also carries the responsive-resize listener
    // (Fase C, opcion 2): both flows fetch a freshly-rendered page and
    // graft it into the live DOM through the same shared applyHtml()
    // helper defined once inside this script, rather than each having
    // its own copy of the DOMParser/scroll-restore logic. viewportWidth/
    // viewportHeight are this response's own render size (the same
    // values RenderAvaUiRoute passed to UiPipelineRenderOptions), so the
    // resize listener's "did the window change enough" check has a
    // correct baseline from the very first paint, not just after a
    // resize has already happened once.
    std::string EventScriptTag(int viewportWidth, int viewportHeight) const;

    // Builds a "500 ..." body, including the given detail only when
    // options_.IsDevelopment() -- Production responses stay generic
    // (plan section 16, appsettings.json "environment").
    std::string ErrorResponseText(const std::string& context, const std::string& detail) const;

    // Hot Reload (plan section 15). Only active when options_.watch --
    // starts/stops the background FileWatcher polling thread. A
    // changed `.avaui`/`.ava` file triggers Router::Rescan() (the one
    // piece of Router state built once at startup); every watched
    // change (including `.css`/`.js`) bumps reloadVersion_, which the
    // client-side script from HotReloadScriptTag() polls for.
    void StartHotReloadWatcher();
    void StopHotReloadWatcher();

    // Background sweep of expired sessions (core/session_manager.h),
    // unlike the hot-reload watcher this runs unconditionally -- always
    // on, not gated by options_.watch -- since session expiry is a
    // memory-bounding concern for every run (dev or production), not a
    // dev-only convenience. Started in the constructor, stopped in the
    // destructor. Sleeps in short increments rather than one long sleep
    // so Stop() doesn't have to wait out a full interval to join.
    void StartSessionReaper();
    void StopSessionReaper();

    // <script> tag polling the hot-reload endpoint and doing
    // location.reload() on the first version change it sees. Plain
    // HTTP polling, not WebSockets -- plan section 3 lists WebSockets
    // as a v1 non-goal, so this deliberately doesn't add one just for
    // reload notifications. Injected into RenderOptions::extraHead by
    // RenderAvaUiRoute when options_.watch is true.
    std::string HotReloadScriptTag() const;

    HostOptions options_;
    Logger& logger_;
    RuntimeHost runtime_;
    Router router_;
    StaticFileServer staticFiles_;
    HttpServer server_;
    UiBytecodeCache bytecodeCache_;
    // Loaded for the app's full lifetime (plan section 17: "Plugins
    // extend AvaHost") -- NOT just for `avahost doctor` diagnostics,
    // which used to be the only place PluginLoader::LoadAll was ever
    // called, so a plugin's OnLoad/OnUnload never actually ran around
    // real request handling.
    PluginLoader plugins_;

    // Hot Reload (plan section 15) -- only started when options_.watch
    // is true (StartHotReloadWatcher/StopHotReloadWatcher). watcher_
    // polls options_.projectRoot every 500ms on watcherThread_;
    // reloadVersion_ is bumped on every detected change and read from
    // both that thread and the (single-threaded) request-handling
    // thread, hence atomic rather than a plain counter.
    FileWatcher watcher_;
    std::thread watcherThread_;
    std::atomic<bool> watcherRunning_{false};
    std::atomic<uint64_t> reloadVersion_{0};

    // Fase 2 module 3, persistence follow-up: `state` is still bound
    // fresh on every request (plan Fase 2 decision A) -- what's new is
    // that the *starting values* now come from here instead of always
    // being the page's `state` block defaults, so a handler's mutation
    // (`counter = counter + 1`) actually accumulates across clicks
    // instead of every request restarting from the file's initial
    // value. Keyed by route file path *within* each browser's own
    // session (core/session_manager.h), updated after every render (GET
    // or event dispatch) via RuntimeHost::ExportStateJson. Previously
    // this was one shared value per page for the whole process --
    // deliberately not per-session -- so every open browser read and
    // mutated the same slot; sessions_ replaces that with one such map
    // per `avahost_session` cookie, so state is private to each browser.
    // Still lost on process restart -- in-memory only, nothing written
    // to disk; sessions themselves are also bounded by
    // options_.sessionTtlSeconds (StartSessionReaper), not kept forever.
    //
    // Read+written from the request-handling thread (RenderAvaUiRoute)
    // but also erased-per-file from watcherThread_ on a `.avaui`/`.ava`
    // change (StartHotReloadWatcher) and swept from sessionReaperThread_
    // on expiry -- SessionManager guards its own map internally the
    // same way Router guards declaredRoutes_ against that same watcher
    // thread, so no separate mutex is needed here.
    SessionManager sessions_;

    std::thread sessionReaperThread_;
    std::atomic<bool> sessionReaperRunning_{false};
};

} // namespace avahost
