#include "plugin/plugin_loader.h"

#include <filesystem>

#if defined(_WIN32)
  #include <windows.h>
#else
  #include <dlfcn.h>
#endif

namespace fs = std::filesystem;

namespace avahost {

namespace {
#if defined(_WIN32)
    constexpr const char* kPluginExtension = ".dll";
#else
    constexpr const char* kPluginExtension = ".so";
#endif

void* OpenLibrary(const std::string& path) {
#if defined(_WIN32)
    return reinterpret_cast<void*>(LoadLibraryA(path.c_str()));
#else
    return dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
}

void* GetSymbol(void* handle, const char* name) {
#if defined(_WIN32)
    return reinterpret_cast<void*>(GetProcAddress(reinterpret_cast<HMODULE>(handle), name));
#else
    return dlsym(handle, name);
#endif
}

void CloseLibrary(void* handle) {
#if defined(_WIN32)
    FreeLibrary(reinterpret_cast<HMODULE>(handle));
#else
    dlclose(handle);
#endif
}
} // namespace

PluginLoader::~PluginLoader() {
    UnloadAll();
}

void PluginLoader::LoadAll(const std::string& pluginsDir, const HostOptions& options, Logger& logger) {
    if (!fs::exists(pluginsDir)) return;

    for (const auto& entry : fs::directory_iterator(pluginsDir)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != kPluginExtension) continue;

        std::string path = entry.path().string();
        void* handle = OpenLibrary(path);
        if (!handle) {
            logger.Warn("plugin: failed to load library " + path);
            continue;
        }

        void* symbol = GetSymbol(handle, kPluginEntryPointSymbol);
        if (!symbol) {
            logger.Warn("plugin: " + path + " does not export '" + std::string(kPluginEntryPointSymbol) + "'");
            CloseLibrary(handle);
            continue;
        }

        auto createFn = reinterpret_cast<CreatePluginFn>(symbol);
        IPlugin* instance = createFn();
        if (!instance) {
            logger.Warn("plugin: " + path + " entry point returned null");
            CloseLibrary(handle);
            continue;
        }

        instance->OnLoad(options, logger);
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
            CloseLibrary(it->libraryHandle);
        }
    }
    plugins_.clear();
    names_.clear();
}

} // namespace avahost
