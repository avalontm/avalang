#pragma once
// AvaHost.PluginLoader -- loads shared libraries from pluginsDir and
// instantiates their IPlugin via the avahost_create_plugin symbol
// (plan section 17).
//
// Goes through the PAL's ILibraryLoader (core/platform/interfaces/
// ILibrary.h) instead of calling LoadLibrary/dlopen directly -- picks
// WinLibraryLoader (.dll) on Windows or LinLibraryLoader (.so) on
// Linux at compile time (see plugin_loader.cpp). macOS (.dylib) is
// still unwired.
#include <memory>
#include <string>
#include <vector>

#include "core/host_options.h"
#include "core/logger.h"
#include "core/plugin.h"
#include "platform/interfaces/ILibrary.h"

namespace avahost {

class PluginLoader {
public:
    PluginLoader();
    ~PluginLoader();

    // Scans `pluginsDir` for .dll files and loads each one, calling
    // IPlugin::OnLoad. Failures to load one plugin are logged and
    // skipped, not fatal to the host.
    void LoadAll(const std::string& pluginsDir, const HostOptions& options, Logger& logger);

    // Calls IPlugin::OnUnload on every loaded plugin, in reverse load
    // order, and closes the shared libraries.
    void UnloadAll();

    size_t Count() const { return plugins_.size(); }
    const std::vector<std::string>& Names() const { return names_; }

private:
    struct LoadedPlugin {
        ava::platform::ILibraryHandle* libraryHandle = nullptr;
        IPlugin* instance = nullptr;
    };

    std::unique_ptr<ava::platform::ILibraryLoader> loader_;
    std::vector<LoadedPlugin> plugins_;
    std::vector<std::string> names_;
};

} // namespace avahost
