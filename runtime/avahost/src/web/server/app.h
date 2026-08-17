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

    bool Run();

    void Stop();

private:
    HttpResponse HandleRequest(const HttpRequest& request);

    HttpResponse RenderAvaUiRoute(const std::string& filePath, const RequestContext& ctx,
                                   const std::string& pendingHandler = "",
                                   const std::string& pendingCompId = "",
                                   const std::string& pendingValue = "",
                                   int statusCode = 200,
                                   const std::string& cookieHeader = "",
                                   const std::string& userAgent = "",
                                   bool isErrorPage = false,
                                   const std::string& seedStateJson = "");
    HttpResponse RunScriptRoute(const std::string& filePath, const RequestContext& ctx);

    std::optional<std::string> FindErrorPage(int statusCode) const;

    HttpResponse HandleEventRoute(const HttpRequest& request);

    std::string EventScriptTag() const;

    std::string ErrorResponseText(const std::string& context, const std::string& detail) const;

    void StartHotReloadWatcher();
    void StopHotReloadWatcher();
    void StartSessionReaper();
    void StopSessionReaper();

    std::string HotReloadScriptTag() const;

    HostOptions options_;
    Logger& logger_;
    RuntimeHost runtime_;
    Router router_;
    StaticFileServer staticFiles_;
    HttpServer server_;
    UiBytecodeCache bytecodeCache_;
    PluginLoader plugins_;
    FileWatcher watcher_;
    std::thread watcherThread_;
    std::atomic<bool> watcherRunning_{false};
    std::atomic<uint64_t> reloadVersion_{0};
    SessionManager sessions_;

    std::thread sessionReaperThread_;
    std::atomic<bool> sessionReaperRunning_{false};
};

} // namespace avahost