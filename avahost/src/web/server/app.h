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

#include "component/component_resolver.h"
#include "core/host_options.h"
#include "core/logger.h"
#include "plugin/plugin_loader.h"
#include "rendering/html_renderer.h"
#include "runtime/runtime_host.h"
#include "runtime/state_binder.h"
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

    // `pendingHandler`, when non-empty, is dispatched (StateBinder::
    // Dispatch) against this render's bound state before the page is
    // rendered -- the Fase 2 event flow (HandleEventRoute below). Empty
    // for an ordinary GET, which just binds+renders with no dispatch.
    // `statusCode` is the HTTP status the *response* is sent with --
    // independent of pendingHandler/dispatch. Defaults to 200 for an
    // ordinary page; FindErrorPage's caller passes e.g. 404 so a custom
    // error page still reports the right status to the client (a
    // 404.avaui rendered as 200 would look fine in a browser tab but
    // break anything checking the status code, like a health check or
    // curl script).
    HttpResponse RenderAvaUiRoute(const std::string& filePath, const RequestContext& ctx,
                                   const std::string& pendingHandler = "", int statusCode = 200);
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

    // Fase 2 module 2/3 (rendering/event_binder.h, runtime/
    // state_binder.h): POST /__avahost/event?path=<pagina>, body
    // `handler=OnX`. Resolves `path` the same way an ordinary GET to
    // that path would (router_.Resolve), then re-renders it with the
    // named handler dispatched first -- the response IS the new page
    // HTML, which the client script from EventScriptTag() swaps in via
    // document.open/write/close.
    HttpResponse HandleEventRoute(const HttpRequest& request);

    // <script> tag, injected alongside HotReloadScriptTag(), that POSTs
    // to /__avahost/event whenever an element with [data-handler] fires
    // its bound [data-event] and swaps the response in as the new
    // document -- the client half of the Fase 2 event flow.
    std::string EventScriptTag() const;

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
    HtmlRenderer renderer_;
    // Fase 2 module 1 (component/component_resolver.h) -- resolves
    // `Navbar()`-style component-call nodes in a page/layout tree
    // before rendering. Constructed with runtime_ + options_, both of
    // which are already initialized above it in this member list (see
    // AvaHostApp's constructor init order in app.cpp).
    ComponentResolver componentResolver_;
    HttpServer server_;
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
    // value. Keyed by route file path, updated after every render (GET
    // or event dispatch) via RuntimeHost::ExportStateJson. Deliberately
    // NOT per-session/per-user (no cookies involved) -- this is a
    // single shared value per page for the life of the running process,
    // same simplicity level as the rest of v0.1's single-VM model. True
    // per-session state is still future work (see decision A in
    // AVAHOST_FASE2_PROGRESS.md). Lost on process restart -- in-memory
    // only, nothing written to disk.
    //
    // Read+written from the request-handling thread (RenderAvaUiRoute)
    // but also erased from watcherThread_ on a `.avaui`/`.ava` change
    // (StartHotReloadWatcher) -- guarded by stateCacheMutex_ the same
    // way Router guards declaredRoutes_ against that same watcher
    // thread.
    std::unordered_map<std::string, std::string> stateCache_;
    std::mutex stateCacheMutex_;
};

} // namespace avahost
