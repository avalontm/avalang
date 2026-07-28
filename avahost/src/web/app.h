#pragma once
// Wires the full request pipeline from the plan's high-level
// architecture (section 5):
//   HTTP Listener -> Static Files -> Route Resolver -> Runtime ->
//   Component Tree -> HTML Renderer -> HTTP Response
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>

#include "core/host_options.h"
#include "core/logger.h"
#include "plugin/plugin_loader.h"
#include "rendering/html_renderer.h"
#include "runtime/runtime_host.h"
#include "watch/file_watcher.h"
#include "web/http_server.h"
#include "web/router.h"
#include "web/static_file_server.h"

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
    HttpResponse RenderAvaUiRoute(const std::string& filePath, const RequestContext& ctx);
    HttpResponse RunScriptRoute(const std::string& filePath, const RequestContext& ctx);

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
};

} // namespace avahost
