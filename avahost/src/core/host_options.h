#pragma once
// AvaHost.Core -- in-memory hosting options, filled from appsettings.json
// by AvaHost.Configuration and/or overridden by CLI flags. Plain data,
// no parsing logic here (see config/app_config.h for that).
#include <string>

namespace avahost {

struct HostOptions {
    std::string host = "localhost";
    int port = 8080;
    std::string environment = "Development"; // Development | Production
    bool watch = false;

    // Resolved at startup from the current working directory (the
    // project root, i.e. the folder containing appsettings.json).
    std::string projectRoot = ".";
    std::string routesDir   = "routes";
    std::string wwwrootDir  = "wwwroot";
    std::string layoutsDir  = "layouts";
    std::string componentsDir = "components";
    std::string pluginsDir  = "plugins";

    bool IsDevelopment() const { return environment == "Development"; }
};

} // namespace avahost
