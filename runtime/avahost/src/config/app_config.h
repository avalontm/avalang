#pragma once
// AvaHost.Configuration -- loads appsettings.json (plan section 16)
// into HostOptions. The only place in AvaHost that depends on
// nlohmann/json.
#include <string>

#include "core/host_options.h"

namespace avahost {

class AppConfig {
public:
    // Loads projectRoot/appsettings.json into `options` (options is
    // updated in place; fields not present in the file keep their
    // HostOptions defaults). `options.projectRoot` must already be set.
    // Returns false only on a malformed (not "missing") file.
    static bool Load(HostOptions& options, std::string& outError);
};

} // namespace avahost
