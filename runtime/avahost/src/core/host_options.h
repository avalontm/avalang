#pragma once
// AvaHost.Core -- in-memory hosting options, filled from appsettings.json
// by AvaHost.Configuration and/or overridden by CLI flags. Plain data,
// no parsing logic here (see config/app_config.h for that).
#include <string>

namespace avahost {

struct HostOptions {
    std::string host = "localhost";
    int port = 8080;
    std::string environment = "Development";
    bool watch = false;

    // Resolved at startup from the current working directory (the
    // project root, i.e. the folder containing appsettings.json).
    std::string projectRoot = ".";
    std::string routesDir   = "routes";
    std::string wwwrootDir  = "wwwroot";
    std::string layoutsDir  = "layouts";
    std::string componentsDir = "components";
    std::string pluginsDir  = "plugins";

    // How long (in seconds) a per-browser session's page state survives
    // with no requests before it's reaped -- see core/session_manager.h.
    // Sliding: any request from that session pushes the deadline back
    // out, it isn't a fixed time-since-issued lifetime. Overridable via
    // appsettings.json's "sessionTtlSeconds" (config/app_config.cpp);
    // 1800s (30 minutes) matches the common default session timeout used
    // by ASP.NET Core and most other web frameworks -- not a value
    // specific to this project.
    int sessionTtlSeconds = 1800;

    // Number of worker threads for handling concurrent HTTP connections.
    // Each thread processes requests sequentially but threads run in parallel.
    // 0 = auto-detect (hardware_concurrency), 1 = single-threaded (original).
    // Overridable via appsettings.json's "workerThreads".
    int workerThreads = 0;

    bool IsDevelopment() const { return environment == "Development"; }
};

} // namespace avahost
