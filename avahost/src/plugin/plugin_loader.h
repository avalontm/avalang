#pragma once
// AvaHost.PluginLoader -- loads shared libraries from pluginsDir and
// instantiates their IPlugin via the avahost_create_plugin symbol
// (plan section 17). dlopen/dlsym on Linux, LoadLibrary/GetProcAddress
// on Windows, behind this one file.
#include <memory>
#include <string>
#include <vector>

#include "core/host_options.h"
#include "core/logger.h"
#include "core/plugin.h"

namespace avahost {

class PluginLoader {
public:
    ~PluginLoader();

    // Scans `pluginsDir` for shared libraries (.dll on Windows, .so on
    // Linux) and loads each one, calling IPlugin::OnLoad. Failures to
    // load one plugin are logged and skipped, not fatal to the host.
    void LoadAll(const std::string& pluginsDir, const HostOptions& options, Logger& logger);

    // Calls IPlugin::OnUnload on every loaded plugin, in reverse load
    // order, and closes the shared libraries.
    void UnloadAll();

    size_t Count() const { return plugins_.size(); }
    const std::vector<std::string>& Names() const { return names_; }

private:
    struct LoadedPlugin {
        void* libraryHandle = nullptr;
        IPlugin* instance = nullptr;
    };

    std::vector<LoadedPlugin> plugins_;
    std::vector<std::string> names_;
};

} // namespace avahost
