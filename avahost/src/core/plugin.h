#pragma once
// AvaHost.PluginLoader contract. Plugins extend AvaHost (middleware,
// services, renderers, CLI commands, config, diagnostics) but must never
// touch parser/compiler/lexer/VM internals -- see plan section 17.
//
// A plugin is a shared library (.dll/.so) exporting exactly one C symbol,
// `avahost_create_plugin`, returning a heap-allocated IPlugin*. AvaHost
// owns the pointer after that call and destroys it via IPlugin::~IPlugin
// before unloading the library.
#include <string>

#include "core/host_options.h"
#include "core/logger.h"

namespace avahost {

class IPlugin {
public:
    virtual ~IPlugin() = default;

    // Short machine name, e.g. "avahost-cors". Used in logs and by
    // `avahost doctor` to list loaded plugins.
    virtual std::string Name() const = 0;

    // Called once after the library is loaded and before the host starts
    // serving requests. Plugins register whatever they need through the
    // logger/options passed in (future versions add a real service
    // registry / middleware pipeline hook here -- v0.1 keeps this
    // intentionally minimal, see roadmap section 20).
    virtual void OnLoad(const HostOptions& options, Logger& logger) = 0;

    // Called once before the host process exits, in reverse load order.
    virtual void OnUnload() {}
};

// Every plugin shared library must export this, with C linkage so the
// symbol name isn't mangled and dlsym/GetProcAddress can find it.
using CreatePluginFn = IPlugin* (*)();
constexpr const char* kPluginEntryPointSymbol = "avahost_create_plugin";

} // namespace avahost
