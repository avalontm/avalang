#include "plugin/plugin_loader.h"

#include <filesystem>

#if defined(_WIN32)
#include "platform/windows/WinLibrary.h"
#else
#include "platform/linux/LinLibrary.h"
#endif

namespace fs = std::filesystem;

namespace avahost {

namespace {
#if defined(_WIN32)
constexpr const char* kPluginExtension = ".dll";
#else
// Linux/macOS plugin shared objects use the platform's native extension
// instead of .dll -- see PluginLoader() below for the matching loader.
constexpr const char* kPluginExtension = ".so";
#endif
} // namespace

PluginLoader::PluginLoader()
#if defined(_WIN32)
    : loader_(std::make_unique<ava::platform::windows::WinLibraryLoader>()) {}
#else
    : loader_(std::make_unique<ava::platform::linux_::LinLibraryLoader>()) {}
#endif

PluginLoader::~PluginLoader() {
    UnloadAll();
}

void PluginLoader::LoadAll(const std::string& pluginsDir, const HostOptions& options, Logger& logger) {
    if (!fs::exists(pluginsDir)) return;

    for (const auto& entry : fs::directory_iterator(pluginsDir)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != kPluginExtension) continue;

        std::string path = entry.path().string();
        ava::platform::ILibraryHandle* handle = loader_->Load(path);
        if (!handle) {
            logger.Warn("plugin: failed to load library " + path);
            continue;
        }

        void* symbol = handle->ResolveSymbol(kPluginEntryPointSymbol);
        if (!symbol) {
            logger.Warn("plugin: " + path + " does not export '" + std::string(kPluginEntryPointSymbol) + "'");
            loader_->Unload(handle);
            continue;
        }

        auto createFn = reinterpret_cast<CreatePluginFn>(symbol);
        IPlugin* instance = nullptr;
        // A plugin is third-party code running in-process; letting an
        // exception from its entry point or OnLoad() escape unwinds
        // straight through LoadAll (called once, synchronously, from
        // AvaHostApp's constructor in CmdRun/CmdWatch) and either
        // std::terminates the whole host before it ever starts listening,
        // or -- worse -- corrupts state and crashes later on the first
        // request in a way that looks unrelated to plugin loading. One
        // bad plugin should fail to load and be skipped, not take
        // AvaHost down before "listening on ..." is even printed.
        try {
            instance = createFn();
            if (!instance) {
                logger.Warn("plugin: " + path + " entry point returned null");
                loader_->Unload(handle);
                continue;
            }
            instance->OnLoad(options, logger);
        } catch (const std::exception& ex) {
            logger.Error("plugin: " + path + " threw during load: " + std::string(ex.what()));
            if (instance) delete instance;
            loader_->Unload(handle);
            continue;
        } catch (...) {
            logger.Error("plugin: " + path + " threw a non-std exception during load");
            if (instance) delete instance;
            loader_->Unload(handle);
            continue;
        }

        logger.Info("plugin loaded: " + instance->Name() + " (" + path + ")");

        plugins_.push_back(LoadedPlugin{handle, instance});
        names_.push_back(instance->Name());
    }
}

void PluginLoader::UnloadAll() {
    for (auto it = plugins_.rbegin(); it != plugins_.rend(); ++it) {
        if (it->instance) {
            it->instance->OnUnload();
            delete it->instance;
        }
        if (it->libraryHandle) {
            loader_->Unload(it->libraryHandle);
        }
    }
    plugins_.clear();
    names_.clear();
}

} // namespace avahost
