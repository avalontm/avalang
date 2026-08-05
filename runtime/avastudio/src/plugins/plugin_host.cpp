#include "plugin_host.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>

#include "plugin_ui_bridge.h"

#include "design/avaui_text.h"
#include "design/design_document.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace fs = std::filesystem;

namespace studio {

namespace {

#if defined(_WIN32)
constexpr const char* kPluginExtension = ".dll";
#elif defined(__APPLE__)
constexpr const char* kPluginExtension = ".dylib";
#else
constexpr const char* kPluginExtension = ".so";
#endif

void* LoadLibraryPortable(const std::string& path) {
#if defined(_WIN32)
    return static_cast<void*>(LoadLibraryA(path.c_str()));
#else
    // RTLD_LOCAL (not _GLOBAL): a plugin's own symbols shouldn't leak
    // into other plugins or the host's global symbol table -- each
    // plugin only talks to Ava Studio through the AvaStudioHost struct
    // it's handed, never by being able to see another module's symbols.
    return dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
}

void* ResolveSymbolPortable(void* handle, const char* name) {
#if defined(_WIN32)
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle), name));
#else
    return dlsym(handle, name);
#endif
}

void UnloadLibraryPortable(void* handle) {
    if (!handle) return;
#if defined(_WIN32)
    FreeLibrary(static_cast<HMODULE>(handle));
#else
    dlclose(handle);
#endif
}

// A metadata symbol returning null (either because the plugin doesn't
// export it, or a badly-behaved one that exports it but returns null
// anyway) is "not provided", same as not exporting it at all -- never
// a reason to reject the plugin, so this always hands back a valid
// (possibly empty) std::string rather than propagating the null.
std::string SafeCStr(const char* s) {
    return s ? std::string(s) : std::string();
}

// Fase 9: reads the three optional metadata symbols (see plugin_api.h)
// from an already-open library handle. Called from both LoadAll() (the
// library's open anyway) and ScanAvailable()'s cache-miss path (which
// opens the library just for this, then closes it again) -- pulled out
// here so both places resolve the exact same three symbol names.
struct PluginMetadata {
    std::string name;
    std::string version;
    std::string author;
};

PluginMetadata ReadPluginMetadata(void* handle) {
    PluginMetadata metadata;
    auto name_fn =
        reinterpret_cast<AvaPluginMetadataFn>(ResolveSymbolPortable(handle, AVA_PLUGIN_DISPLAY_NAME_SYMBOL));
    auto version_fn = reinterpret_cast<AvaPluginMetadataFn>(ResolveSymbolPortable(handle, AVA_PLUGIN_VERSION_SYMBOL));
    auto author_fn = reinterpret_cast<AvaPluginMetadataFn>(ResolveSymbolPortable(handle, AVA_PLUGIN_AUTHOR_SYMBOL));
    if (name_fn) metadata.name = SafeCStr(name_fn());
    if (version_fn) metadata.version = SafeCStr(version_fn());
    if (author_fn) metadata.author = SafeCStr(author_fn());
    return metadata;
}

// Same rule read_file already enforces on the plugin side (see
// ai_agent's tools.cpp) -- duplicated here rather than shared, since
// this is the host's own security boundary for a *write*, not
// something a plugin gets to self-certify. Rejects `../` traversal and
// absolute-path overrides; any resolution failure is a rejection too.
bool ResolveWithinRoot(const std::string& root, const std::string& requested_relative_path,
                       fs::path* out_resolved) {
    if (root.empty() || requested_relative_path.empty()) return false;

    std::error_code ec;
    fs::path root_canonical = fs::weakly_canonical(fs::path(root), ec);
    if (ec) return false;

    fs::path candidate = root_canonical / fs::path(requested_relative_path);
    fs::path candidate_canonical = fs::weakly_canonical(candidate, ec);
    if (ec) return false;

    auto [root_end, _] = std::mismatch(root_canonical.begin(), root_canonical.end(), candidate_canonical.begin(),
                                        candidate_canonical.end());
    if (root_end != root_canonical.end()) return false;

    *out_resolved = candidate_canonical;
    return true;
}

// Fase 6: AvaHostServices::design_add_component/design_edit_component
// take properties as a flat "key=value;key2=value2" string rather than
// a JSON blob -- keeps the plugin_api.h C boundary free of any JSON
// dependency, same spirit as every other service here being plain
// strings. No escaping of ';'/'=' inside a value: a property value
// that needs either just isn't reachable through this call (a plugin
// wanting that can still fall back to apply_edit against the raw
// .avaui text). Empty entries (blank segments, or a segment with no
// '=') are skipped rather than erroring -- tolerant of a trailing ';'
// or accidental double ';;' from whatever built the string.
std::vector<PropertyRow> ParsePropertiesKv(const std::string& kv) {
    std::vector<PropertyRow> out;
    size_t pos = 0;
    while (pos <= kv.size()) {
        const size_t semi = kv.find(';', pos);
        const std::string entry = kv.substr(pos, semi == std::string::npos ? std::string::npos : semi - pos);
        const size_t eq = entry.find('=');
        if (eq != std::string::npos) {
            std::string key = entry.substr(0, eq);
            std::string value = entry.substr(eq + 1);
            if (!key.empty()) out.push_back(PropertyRow{std::move(key), std::move(value)});
        }
        if (semi == std::string::npos) break;
        pos = semi + 1;
    }
    return out;
}

} // namespace

PluginHost::PluginHost(PluginHostCallbacks callbacks) : callbacks_(std::move(callbacks)) {
    host_.abi_version = AVA_STUDIO_PLUGIN_ABI_VERSION;
    host_._internal_host_reserved = this;
    plugins_ui::FillUiApi(host_.ui);
    host_.services.get_project_root = &GetProjectRootTrampoline;
    host_.services.get_active_file = &GetActiveFileTrampoline;
    host_.services.get_last_run_output = &GetLastRunOutputTrampoline;
    host_.services.log = &LogTrampoline;
    host_.services.apply_edit = &ApplyEditTrampoline;
    host_.services.run_project = &RunProjectTrampoline;
    host_.services.design_add_component = &DesignAddComponentTrampoline;
    host_.services.design_edit_component = &DesignEditComponentTrampoline;
    host_.register_panel = &RegisterPanelTrampoline;
}

PluginHost::~PluginHost() {
    UnloadAll();
}

void PluginHost::LoadAll(const std::string& plugins_dir, const std::vector<std::string>& disabled_plugins) {
    std::error_code ec;
    if (!fs::exists(plugins_dir, ec) || !fs::is_directory(plugins_dir, ec)) {
        return; // no plugins/ folder yet -- a fresh install, not an error
    }

    for (const auto& entry : fs::directory_iterator(plugins_dir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != kPluginExtension) continue;

        const std::string path = entry.path().string();
        const std::string display_name = entry.path().filename().string();

        // The "Plugins" menu's checkboxes (see titlebar_panel.h) --
        // skipped exactly like any other rejection reason below, just
        // without ever touching the library at all.
        if (std::find(disabled_plugins.begin(), disabled_plugins.end(), display_name) != disabled_plugins.end()) {
            if (callbacks_.log) callbacks_.log("[plugin_host] " + display_name + " desactivado por el usuario -- omitido");
            continue;
        }

        void* handle = LoadLibraryPortable(path);
        if (!handle) {
            if (callbacks_.log) callbacks_.log("[plugin_host] failed to load " + display_name);
            continue;
        }

        auto abi_version_fn =
            reinterpret_cast<AvaPluginAbiVersionFn>(ResolveSymbolPortable(handle, AVA_PLUGIN_ABI_VERSION_SYMBOL));
        auto init_fn = reinterpret_cast<AvaPluginInitFn>(ResolveSymbolPortable(handle, AVA_PLUGIN_INIT_SYMBOL));
        auto shutdown_fn =
            reinterpret_cast<AvaPluginShutdownFn>(ResolveSymbolPortable(handle, AVA_PLUGIN_SHUTDOWN_SYMBOL));

        if (!abi_version_fn || !init_fn || !shutdown_fn) {
            if (callbacks_.log) {
                callbacks_.log("[plugin_host] " + display_name +
                               " does not export ava_plugin_abi_version/ava_plugin_init/ava_plugin_shutdown -- skipped");
            }
            UnloadLibraryPortable(handle);
            continue;
        }

        // Checked BEFORE init_fn ever sees a live AvaStudioHost* --
        // see the ABI-compat note above AVA_STUDIO_PLUGIN_ABI_VERSION
        // in plugin_api.h. A plugin reporting a NEWER ABI than this
        // host build understands is rejected outright (it may rely on
        // fields this host doesn't set); an OLDER one is fine, since
        // fields are only ever appended.
        const int plugin_abi = abi_version_fn();
        if (plugin_abi > AVA_STUDIO_PLUGIN_ABI_VERSION || plugin_abi <= 0) {
            if (callbacks_.log) {
                callbacks_.log("[plugin_host] " + display_name + " reports ABI version " +
                               std::to_string(plugin_abi) + ", this build supports up to " +
                               std::to_string(AVA_STUDIO_PLUGIN_ABI_VERSION) + " -- skipped");
            }
            UnloadLibraryPortable(handle);
            continue;
        }

        // A plugin is third-party code running in-process -- one bad
        // plugin throwing during init should be skipped, not take down
        // Ava Studio before its first frame (mirrors AvaHost's
        // PluginLoader::LoadAll, see runtime/avahost/src/plugin/
        // plugin_loader.cpp).
        bool init_ok = false;
        loading_plugin_name_ = display_name;
        try {
            init_ok = init_fn(&host_);
        } catch (const std::exception& ex) {
            if (callbacks_.log) {
                callbacks_.log("[plugin_host] " + display_name + " threw during init: " + std::string(ex.what()));
            }
            init_ok = false;
        } catch (...) {
            if (callbacks_.log) callbacks_.log("[plugin_host] " + display_name + " threw a non-std exception during init");
            init_ok = false;
        }
        loading_plugin_name_.clear();

        if (!init_ok) {
            if (callbacks_.log) callbacks_.log("[plugin_host] " + display_name + " init failed -- skipped");
            // Any panels it may have registered before failing are left
            // in panels_ -- Fase 0 doesn't support a partial rollback,
            // so a plugin should only call register_panel() once it
            // knows it will return true. Documented in plugin_api.h.
            UnloadLibraryPortable(handle);
            continue;
        }

        if (callbacks_.log) callbacks_.log("[plugin_host] loaded " + display_name);
        PluginMetadata metadata = ReadPluginMetadata(handle);
        loaded_.push_back(LoadedPlugin{handle, shutdown_fn, display_name, metadata.name, metadata.version,
                                        metadata.author});
    }
}

void PluginHost::UnloadAll() {
    for (auto it = loaded_.rbegin(); it != loaded_.rend(); ++it) {
        if (it->shutdown) {
            try {
                it->shutdown();
            } catch (...) {
                // Swallow -- we're tearing down the app either way, and
                // a plugin misbehaving on shutdown shouldn't stop the
                // rest from unloading cleanly.
            }
        }
        UnloadLibraryPortable(it->library_handle);
    }
    loaded_.clear();
    panels_.clear();
}

void PluginHost::Reload(const std::string& plugins_dir, const std::vector<std::string>& disabled_plugins) {
    UnloadAll();
    LoadAll(plugins_dir, disabled_plugins);
}

std::vector<PluginInfo> PluginHost::ScanAvailable(const std::string& plugins_dir,
                                                   const std::vector<std::string>& disabled_plugins) const {
    std::vector<PluginInfo> result;

    std::error_code ec;
    if (!fs::exists(plugins_dir, ec) || !fs::is_directory(plugins_dir, ec)) {
        return result; // no plugins/ folder yet -- nothing to list
    }

    for (const auto& entry : fs::directory_iterator(plugins_dir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != kPluginExtension) continue;

        PluginInfo info;
        info.file_name = entry.path().filename().string();
        info.enabled =
            std::find(disabled_plugins.begin(), disabled_plugins.end(), info.file_name) == disabled_plugins.end();

        // Fase 9: metadata for the menu. A loaded plugin already had it
        // read once in LoadAll() -- reuse that, zero extra cost. A
        // disabled (or currently-rejected) one has no LoadedPlugin to
        // read from, so fall back to the mtime-keyed cache below rather
        // than dlopen-ing it fresh every frame (see metadata_cache_'s
        // comment in plugin_host.h for why that matters here).
        auto loaded_it = std::find_if(loaded_.begin(), loaded_.end(), [&info](const LoadedPlugin& lp) {
            return lp.display_name == info.file_name;
        });
        info.loaded = loaded_it != loaded_.end();

        if (info.loaded) {
            info.plugin_name = loaded_it->meta_name;
            info.version = loaded_it->meta_version;
            info.author = loaded_it->meta_author;
        } else {
            std::error_code mtime_ec;
            const fs::file_time_type mtime = fs::last_write_time(entry.path(), mtime_ec);

            auto cache_it = metadata_cache_.find(info.file_name);
            const bool cache_miss =
                mtime_ec || cache_it == metadata_cache_.end() || cache_it->second.mtime != mtime;
            if (cache_miss) {
                PluginMetadataCacheEntry fresh;
                fresh.mtime = mtime;
                void* handle = LoadLibraryPortable(entry.path().string());
                if (handle) {
                    PluginMetadata metadata = ReadPluginMetadata(handle);
                    fresh.name = std::move(metadata.name);
                    fresh.version = std::move(metadata.version);
                    fresh.author = std::move(metadata.author);
                    UnloadLibraryPortable(handle);
                }
                cache_it = metadata_cache_.insert_or_assign(info.file_name, std::move(fresh)).first;
            }
            info.plugin_name = cache_it->second.name;
            info.version = cache_it->second.version;
            info.author = cache_it->second.author;
        }

        result.push_back(std::move(info));
    }

    // Stable, predictable ordering for the menu regardless of what the
    // filesystem's directory_iterator happens to hand back -- alpha by
    // file name, same as Explorer already sorts its own listing.
    std::sort(result.begin(), result.end(),
              [](const PluginInfo& a, const PluginInfo& b) { return a.file_name < b.file_name; });
    return result;
}

int PluginHost::RegisterPanelTrampoline(AvaStudioHost* host, const AvaPanelRegistration* registration) {
    if (!host || !registration || !registration->name || !registration->draw) return -1;
    auto* self = static_cast<PluginHost*>(host->_internal_host_reserved);

    for (const auto& existing : self->panels_) {
        // Duplicates are rejected per (name, is_settings) pair, not name
        // alone -- a plugin's normal chat tab and its Settings section
        // can legitimately share a display name (see Fase 6 of
        // PLAN_settings_panel.md, ai_agent's "AI Agent (OpenRouter)").
        if (existing.name == registration->name && existing.is_settings == registration->is_settings) return -1;
    }

    RegisteredPanel panel;
    panel.name = registration->name;
    panel.owner_plugin = self->loading_plugin_name_;
    panel.draw = registration->draw;
    panel.user_data = registration->user_data;
    panel.default_dock_slot = registration->default_dock_slot;
    panel.is_settings = registration->is_settings;

    self->panels_.push_back(std::move(panel));
    return static_cast<int>(self->panels_.size()) - 1;
}

const char* PluginHost::GetProjectRootTrampoline(AvaStudioHost* host) {
    auto* self = static_cast<PluginHost*>(host->_internal_host_reserved);
    self->scratch_project_root_ = self->callbacks_.get_project_root ? self->callbacks_.get_project_root() : "";
    return self->scratch_project_root_.c_str();
}

bool PluginHost::GetActiveFileTrampoline(AvaStudioHost* host, const char** out_path, const char** out_content,
                                         int* out_selection_start, int* out_selection_end) {
    auto* self = static_cast<PluginHost*>(host->_internal_host_reserved);
    if (!self->callbacks_.get_active_file) return false;

    if (!self->callbacks_.get_active_file(self->scratch_active_path_, self->scratch_active_content_)) {
        return false;
    }
    if (out_path) *out_path = self->scratch_active_path_.c_str();
    if (out_content) *out_content = self->scratch_active_content_.c_str();
    // Selection reporting lands with Fase 3 -- see plugin_api.h's note
    // on get_active_file. -1/-1 means "not available yet", not "empty
    // selection at offset -1".
    if (out_selection_start) *out_selection_start = -1;
    if (out_selection_end) *out_selection_end = -1;
    return true;
}

bool PluginHost::GetLastRunOutputTrampoline(AvaStudioHost* host, const char** out_text, bool* out_had_error) {
    auto* self = static_cast<PluginHost*>(host->_internal_host_reserved);
    if (!self->callbacks_.get_last_run_output) return false;

    bool had_error = false;
    if (!self->callbacks_.get_last_run_output(self->scratch_last_run_text_, had_error)) {
        return false;
    }
    if (out_text) *out_text = self->scratch_last_run_text_.c_str();
    if (out_had_error) *out_had_error = had_error;
    return true;
}

void PluginHost::LogTrampoline(AvaStudioHost* host, const char* message) {
    auto* self = static_cast<PluginHost*>(host->_internal_host_reserved);
    if (!self->callbacks_.log || !message) return;
    // loading_plugin_name_ is only set while a plugin's ava_plugin_init
    // is on the stack -- a plugin logging later (e.g. from inside its
    // own draw() callback) falls back to a generic prefix instead of a
    // stale/empty name.
    const std::string prefix = self->loading_plugin_name_.empty() ? "[plugin]" : "[" + self->loading_plugin_name_ + "]";
    self->callbacks_.log(prefix + " " + message);
}

bool PluginHost::QueueEdit(PluginHost* self, const std::string& path, const std::string& new_content,
                            const std::string& description, std::string& out_error) {
    if (path.empty()) {
        out_error = "falta el parametro 'path'";
        return false;
    }

    const std::string root = self->callbacks_.get_project_root ? self->callbacks_.get_project_root() : "";
    if (root.empty()) {
        out_error = "no hay ningun proyecto abierto";
        return false;
    }

    fs::path resolved;
    if (!ResolveWithinRoot(root, path, &resolved)) {
        out_error = "ruta invalida o fuera de la carpeta del proyecto: " + path;
        return false;
    }

    // The diff PendingEditsPanel shows is against what's on disk right
    // now -- a brand-new file (resolved doesn't exist yet) diffs against
    // "", same as `git diff` would show it as all-added lines.
    std::string old_content;
    std::error_code ec;
    if (fs::exists(resolved, ec) && fs::is_regular_file(resolved, ec)) {
        std::ifstream file(resolved, std::ios::binary);
        if (file) {
            std::ostringstream buffer;
            buffer << file.rdbuf();
            old_content = buffer.str();
        }
    }

    PendingEdit edit;
    // loading_plugin_name_ is only set while a plugin's ava_plugin_init is
    // on the stack (see LogTrampoline's comment) -- every caller of
    // QueueEdit runs well after that, from the plugin's own worker
    // thread, so it's always empty here. Fase 5 doesn't need per-plugin
    // attribution badly enough to plumb a caller identity through every
    // AvaHostServices call just for this, so the panel gets a generic
    // label instead.
    edit.owner_plugin = "plugin";
    edit.path = path;
    edit.description = description;
    edit.old_content = std::move(old_content);
    edit.new_content = new_content;

    {
        std::lock_guard<std::mutex> lock(self->pending_edits_mutex_);
        edit.id = self->next_edit_id_++;
        self->pending_edits_.push_back(std::move(edit));
    }
    return true;
}

bool PluginHost::ApplyEditTrampoline(AvaStudioHost* host, const char* path, const char* new_content,
                                      const char* description, const char** out_error) {
    auto* self = static_cast<PluginHost*>(host->_internal_host_reserved);

    if (!path || path[0] == '\0' || !new_content) {
        self->scratch_apply_edit_error_ = "faltan parametros: se esperan 'path' y 'new_content'";
        if (out_error) *out_error = self->scratch_apply_edit_error_.c_str();
        return false;
    }

    std::string error;
    if (!QueueEdit(self, path, new_content, description ? description : "", error)) {
        self->scratch_apply_edit_error_ = error;
        if (out_error) *out_error = self->scratch_apply_edit_error_.c_str();
        return false;
    }
    return true;
}

bool PluginHost::RunProjectTrampoline(AvaStudioHost* host, const char** out_output, bool* out_had_error,
                                      const char** out_error) {
    auto* self = static_cast<PluginHost*>(host->_internal_host_reserved);
    RunMailbox& mbox = self->run_mailbox_;

    std::unique_lock<std::mutex> lock(mbox.mutex);
    // Wait out any request already in flight (from this plugin or
    // another) before posting ours -- see the mailbox's comment in
    // plugin_host.h for why only one is served at a time.
    mbox.cv.wait(lock, [&] { return !mbox.request_pending; });
    mbox.request_pending = true;
    mbox.result_ready = false;

    // PumpMainThreadWork() polls request_pending once per frame rather
    // than waiting on the cv itself, so no notify is needed to wake it --
    // just wait for it to flip result_ready.
    mbox.cv.wait(lock, [&] { return mbox.result_ready; });

    RunProjectResult result = std::move(mbox.result);
    mbox.request_pending = false;
    mbox.result_ready = false;
    lock.unlock();
    mbox.cv.notify_all(); // let the next waiting RunProjectTrampoline post its request

    self->scratch_run_output_ = result.output;
    self->scratch_run_error_ = result.error;

    if (!result.has_result) {
        if (out_error) *out_error = self->scratch_run_error_.c_str();
        return false;
    }
    if (out_output) *out_output = self->scratch_run_output_.c_str();
    if (out_had_error) *out_had_error = result.had_error;
    return true;
}

bool PluginHost::FetchActiveAvauiDocument(PluginHost* self, std::string& out_path, std::string& out_source) {
    DesignDocMailbox& mbox = self->design_doc_mailbox_;

    std::unique_lock<std::mutex> lock(mbox.mutex);
    // Same one-at-a-time queueing as RunMailbox above (see its comment
    // in plugin_host.h) -- design_doc_mailbox_ is a separate slot from
    // run_mailbox_ so a design fetch never has to wait behind an
    // in-flight run_project(), and vice versa.
    mbox.cv.wait(lock, [&] { return !mbox.request_pending; });
    mbox.request_pending = true;
    mbox.result_ready = false;

    mbox.cv.wait(lock, [&] { return mbox.result_ready; });

    const bool available = mbox.doc_available;
    out_path = mbox.path;
    out_source = mbox.avaui_source;
    mbox.request_pending = false;
    mbox.result_ready = false;
    lock.unlock();
    mbox.cv.notify_all();

    return available;
}

bool PluginHost::DesignAddComponentTrampoline(AvaStudioHost* host, const char* parent_id, const char* type,
                                               const char* id, const char* properties_kv, const char** out_error) {
    auto* self = static_cast<PluginHost*>(host->_internal_host_reserved);

    auto fail = [&](const std::string& message) {
        self->scratch_design_error_ = message;
        if (out_error) *out_error = self->scratch_design_error_.c_str();
        return false;
    };

    if (!type || type[0] == '\0') return fail("falta el parametro 'type'");

    std::string path;
    std::string source;
    if (!FetchActiveAvauiDocument(self, path, source)) {
        return fail("no hay ningun documento .avaui activo en el editor");
    }

    design::DesignDocument doc;
    std::string parse_error;
    if (!design::LoadAvauiFile(path, doc, parse_error)) {
        return fail("no se pudo interpretar el documento activo: " + parse_error);
    }

    const std::string new_uid =
        design::AddComponentNode(doc, parent_id ? parent_id : "", type, id ? id : "",
                                  ParsePropertiesKv(properties_kv ? properties_kv : ""));
    if (new_uid.empty()) {
        return fail("no se pudo agregar el componente -- revisa que 'parent_id' exista y sea un contenedor, "
                     "y que 'type' sea un tipo de componente conocido");
    }

    const std::string new_source = design::WriteAvauiText(doc.Root(), doc.code_behind, doc.initial_state, doc.imports);
    std::string description = "Agente: agregar componente '" + std::string(type) + "'";
    if (id && id[0] != '\0') description += " (" + std::string(id) + ")";

    std::string queue_error;
    if (!QueueEdit(self, path, new_source, description, queue_error)) return fail(queue_error);
    return true;
}

bool PluginHost::DesignEditComponentTrampoline(AvaStudioHost* host, const char* node_id, const char* properties_kv,
                                                const char* new_id, const char** out_error) {
    auto* self = static_cast<PluginHost*>(host->_internal_host_reserved);

    auto fail = [&](const std::string& message) {
        self->scratch_design_error_ = message;
        if (out_error) *out_error = self->scratch_design_error_.c_str();
        return false;
    };

    if (!node_id || node_id[0] == '\0') return fail("falta el parametro 'node_id'");

    std::string path;
    std::string source;
    if (!FetchActiveAvauiDocument(self, path, source)) {
        return fail("no hay ningun documento .avaui activo en el editor");
    }

    design::DesignDocument doc;
    std::string parse_error;
    if (!design::LoadAvauiFile(path, doc, parse_error)) {
        return fail("no se pudo interpretar el documento activo: " + parse_error);
    }

    const bool has_new_id = new_id && new_id[0] != '\0';
    const std::string new_id_str = has_new_id ? new_id : "";
    if (!design::EditComponentNode(doc, node_id, ParsePropertiesKv(properties_kv ? properties_kv : ""),
                                    has_new_id ? &new_id_str : nullptr)) {
        return fail("no se encontro ningun componente con id '" + std::string(node_id) + "' en el documento activo");
    }

    const std::string new_source = design::WriteAvauiText(doc.Root(), doc.code_behind, doc.initial_state, doc.imports);
    const std::string description = "Agente: editar componente '" + std::string(node_id) + "'";

    std::string queue_error;
    if (!QueueEdit(self, path, new_source, description, queue_error)) return fail(queue_error);
    return true;
}

std::vector<PendingEdit> PluginHost::PendingEdits() const {
    std::lock_guard<std::mutex> lock(pending_edits_mutex_);
    return pending_edits_;
}

void PluginHost::ApproveEdit(int id) {
    PendingEdit edit;
    bool found = false;
    {
        std::lock_guard<std::mutex> lock(pending_edits_mutex_);
        for (auto it = pending_edits_.begin(); it != pending_edits_.end(); ++it) {
            if (it->id == id) {
                edit = std::move(*it);
                pending_edits_.erase(it);
                found = true;
                break;
            }
        }
    }
    if (!found) return; // already resolved (e.g. a stale double-click) -- nothing to do
    if (callbacks_.write_approved_edit) callbacks_.write_approved_edit(edit);
}

void PluginHost::RejectEdit(int id) {
    std::lock_guard<std::mutex> lock(pending_edits_mutex_);
    for (auto it = pending_edits_.begin(); it != pending_edits_.end(); ++it) {
        if (it->id == id) {
            pending_edits_.erase(it);
            break;
        }
    }
}

void PluginHost::PumpMainThreadWork() {
    {
        RunMailbox& mbox = run_mailbox_;

        bool have_request = false;
        {
            std::lock_guard<std::mutex> lock(mbox.mutex);
            have_request = mbox.request_pending && !mbox.result_ready;
        }
        if (have_request) {
            RunProjectResult result;
            if (callbacks_.run_project_on_main_thread) {
                result = callbacks_.run_project_on_main_thread();
            } else {
                result.has_result = false;
                result.error = "run_project no esta disponible en este host";
            }

            std::lock_guard<std::mutex> lock(mbox.mutex);
            mbox.result = std::move(result);
            mbox.result_ready = true;
            mbox.cv.notify_all();
        }
    }

    {
        DesignDocMailbox& mbox = design_doc_mailbox_;

        bool have_request = false;
        {
            std::lock_guard<std::mutex> lock(mbox.mutex);
            have_request = mbox.request_pending && !mbox.result_ready;
        }
        if (have_request) {
            std::string path;
            std::string source;
            const bool available =
                callbacks_.get_active_avaui_document && callbacks_.get_active_avaui_document(path, source);

            std::lock_guard<std::mutex> lock(mbox.mutex);
            mbox.doc_available = available;
            mbox.path = std::move(path);
            mbox.avaui_source = std::move(source);
            mbox.result_ready = true;
            mbox.cv.notify_all();
        }
    }
}

} // namespace studio
