#pragma once

// Fase 0 (see PLAN_agente_ia_openrouter.md): Ava Studio's plugin
// system. Scans a `plugins/` folder for .dll/.so files built against
// plugin_api.h, loads them, and keeps the panels they register until
// UnloadAll(). main.cpp owns one PluginHost for the whole session --
// see its construction/LoadAll/UnloadAll calls there.
//
// Deliberately self-contained: unlike AvaHost's plugin_loader.cpp (see
// runtime/avahost/src/plugin/plugin_loader.cpp), this does not reach
// into runtime/avalang/platform's PAL -- that layer's Linux/macOS
// backends are still stubs (see LinLibrary.cpp), and Ava Studio's
// plugin loading has nothing to do with the VM/FFI's own library
// loading, so there is no real benefit to sharing the dependency.
// Windows uses LoadLibrary/GetProcAddress directly, Linux/macOS use
// dlopen/dlsym directly -- see plugin_host.cpp.

#include <condition_variable>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "plugin_api.h"

namespace studio {

// One apply_edit() proposal waiting on the person's Aplicar/Rechazar
// decision -- see AvaHostServices::apply_edit in plugin_api.h. Never
// applied automatically; PendingEditsPanel (panels/pending_edits_panel.h)
// is what actually shows these and calls PluginHost::ApproveEdit/
// RejectEdit.
struct PendingEdit {
    int id = 0;                  // unique for this session, see PluginHost::next_edit_id_
    std::string owner_plugin;    // display name, for the panel's header
    std::string path;            // as given by the plugin, relative to the project root
    std::string description;     // plugin-provided summary, "" if none
    std::string old_content;     // the file's content on disk at proposal time
    std::string new_content;     // what the plugin wants to replace it with
};

// Result of AvaHostServices::run_project, handed back across
// PluginHost's main-thread mailbox (see RunMailbox below). Mirrors the
// bool-return/out-param shape of the C trampoline one level up in C++.
struct RunProjectResult {
    bool has_result = false; // false only when there was no real tab to run
    bool had_error = false;
    std::string output; // RunResult::message either way
    std::string error;  // set instead of output when has_result is false
};

// One panel a loaded plugin registered. main.cpp iterates
// PluginHost::Panels() every frame and docks+draws each one, the same
// way it already calls DrawExplorerPanel/DrawTerminalPanel/etc for the
// built-in panels.
struct RegisteredPanel {
    std::string name;         // docked tab title, and the ImGui window id
    std::string owner_plugin; // the plugin's file name, for log lines
    AvaPanelDrawFn draw = nullptr;
    void* user_data = nullptr;
    AvaDockSlot default_dock_slot = AVA_DOCK_BOTTOM;
    bool is_settings = false;
};

// Read-only host state PluginHost forwards into AvaHostServices, wired
// by main.cpp to whatever live objects it already owns (ExplorerState,
// EditorState, TerminalState/EngineBridge) -- PluginHost itself stays
// unaware of those panel-specific types, so it isn't coupled to their
// shape and doesn't need touching if they're refactored later.
struct PluginHostCallbacks {
    // Returns ExplorerState::root_dir, or "" if nothing is open.
    std::function<std::string()> get_project_root;

    // Fills `path`/`content` from the active editor tab and returns
    // true, or returns false (leaving both untouched) if no real tab
    // is active (no tabs open, or the Welcome tab).
    std::function<bool(std::string& path, std::string& content)> get_active_file;

    // Fills `text`/`had_error` from the last Run/compile result and
    // returns true, or returns false if nothing has run yet this
    // session.
    std::function<bool(std::string& text, bool& had_error)> get_last_run_output;

    // Appends one line to the Output panel's console. PluginHost
    // already prefixes `line` with the calling plugin's name before
    // invoking this -- see LogTrampoline in the .cpp.
    std::function<void(const std::string& line)> log;

    // --- Write services (Fase 5) ----------------------------------------
    // Writes `edit.new_content` to disk at `project_root/edit.path`
    // (creating parent directories for a brand-new file) and, if that
    // path is open in a tab, refreshes that tab's buffer too so it
    // doesn't show stale text. Called only from the main thread, only
    // by PluginHost::ApproveEdit, right after the person clicks
    // "Aplicar" in PendingEditsPanel -- never called for a rejected
    // edit.
    std::function<void(const PendingEdit& edit)> write_approved_edit;

    // Does the actual work behind AvaHostServices::run_project: runs
    // the active tab through the same pipeline the F5 hotkey uses.
    // Called only from the main thread, from PluginHost::
    // PumpMainThreadWork() -- see RunProjectTrampoline's comment for
    // why a plugin's own thread can't just call this directly.
    std::function<RunProjectResult()> run_project_on_main_thread;

    // --- Design services (Fase 6) ----------------------------------------
    // Fills `path`/`avaui_source` from the active editor tab's CURRENT
    // .avaui source and returns true, or returns false (leaving both
    // untouched) if the active tab isn't a .avaui document (no tab
    // open, the Welcome tab, or a plain .ava/.txt/etc tab). "Current"
    // matters here: a .avaui tab's Design view edits `EditorTab::design`
    // directly and never touches the TextEditor buffer get_active_file
    // reads from, so this always resolves through the same
    // WriteAvauiText() conversion ToggleTabViewMode uses when leaving
    // Design view -- an edit made through the canvas moments ago is
    // never missed just because the person hasn't pressed F7. Reads
    // EditorState, which is not thread-safe (same note as
    // run_project_on_main_thread above), so this is only ever invoked
    // from PluginHost::PumpMainThreadWork() on the main thread, even
    // though the design_add_component/design_edit_component
    // trampolines that need it run on a plugin's own worker thread --
    // see DesignDocMailbox in the .cpp for how that hop happens.
    std::function<bool(std::string& path, std::string& avaui_source)> get_active_avaui_document;
};

// One entry in the "Plugins" menu (see titlebar_panel.h) -- every
// .dll/.so PluginHost can see in plugins_dir, whether or not it's
// currently loaded. Deliberately doesn't try to tell an ABI-mismatched
// or otherwise-rejected file apart from a normal one here -- that
// detail only comes out of an actual LoadAll() attempt (and is already
// logged to the Output panel when it happens); ScanAvailable() is just
// "what files are there and is the user's checkbox for it on".
struct PluginInfo {
    std::string file_name; // e.g. "ai_agent.dll" -- same string used in
                            // StudioSettings::disabled_plugins and
                            // LoadedPlugin::display_name.
    bool enabled = true;   // false if `file_name` is in the disabled
                            // list passed to ScanAvailable().
    bool loaded = false;   // true if this file is currently loaded
                            // (i.e. present in loaded_ right now). Can
                            // be false even for an enabled plugin if
                            // the last LoadAll() rejected it (bad ABI,
                            // init failed, etc.) -- see the Output
                            // panel for why in that case.

    // Fase 9: optional metadata a plugin can export (see plugin_api.h's
    // AVA_PLUGIN_DISPLAY_NAME_SYMBOL/VERSION_SYMBOL/AUTHOR_SYMBOL) --
    // any of these is "" if the plugin doesn't export that symbol.
    // Populated for both loaded and not-currently-loaded plugins alike
    // (see ScanAvailable's comment on how it gets this without paying
    // for a dlopen every frame), so the "Plugins" menu can show them
    // even for a disabled plugin.
    std::string plugin_name; // from ava_plugin_display_name(), NOT file_name
    std::string version;     // from ava_plugin_version()
    std::string author;      // from ava_plugin_author()
};

class PluginHost {
public:
    explicit PluginHost(PluginHostCallbacks callbacks);
    ~PluginHost();

    PluginHost(const PluginHost&) = delete;
    PluginHost& operator=(const PluginHost&) = delete;

    // Loads every plugin library in `plugins_dir` (non-recursive) that
    // exports the three required symbols (see plugin_api.h), reports
    // an ABI version this host build understands, AND whose file name
    // is not listed in `disabled_plugins` (see StudioSettings::
    // disabled_plugins -- a disabled file is skipped with a log line,
    // same as any other rejection reason). No-ops (logs nothing,
    // doesn't throw) if the directory doesn't exist -- a fresh install
    // with no plugins yet is the common case, not an error. One log
    // line per plugin: loaded, or the specific reason it was rejected
    // (missing symbol / ABI mismatch / init returned false / threw /
    // disabled by the user).
    void LoadAll(const std::string& plugins_dir, const std::vector<std::string>& disabled_plugins = {});

    // Calls ava_plugin_shutdown() on every loaded plugin (reverse load
    // order) and unloads each library. main.cpp calls this once,
    // before ImGui_ImplOpenGL3_Shutdown() -- a plugin panel must not
    // be drawn (or exist in Panels()) after this returns.
    void UnloadAll();

    // UnloadAll() followed by LoadAll(plugins_dir, disabled_plugins) --
    // what the "Plugins" menu's checkboxes actually trigger, so a
    // toggle takes effect immediately, no restart needed. Same
    // main-thread-only rule as UnloadAll()/LoadAll(): must not be
    // called while anything is iterating Panels() this frame (main.cpp
    // calls it right after reading the titlebar's toggle request, before
    // the frame's panel-drawing loop runs -- see main.cpp).
    void Reload(const std::string& plugins_dir, const std::vector<std::string>& disabled_plugins);

    // Lists every plugin file PluginHost can see in `plugins_dir` right
    // now (non-recursive, same extension filter as LoadAll), without
    // loading or unloading anything -- purely for the "Plugins" menu to
    // draw its checkboxes from. `disabled_plugins` is the same list
    // LoadAll()/Reload() would be given; a name in it comes back with
    // PluginInfo::enabled = false. Safe to call every frame (it's just
    // a directory listing + a lookup against `loaded_`).
    std::vector<PluginInfo> ScanAvailable(const std::string& plugins_dir,
                                           const std::vector<std::string>& disabled_plugins) const;

    // Panels a plugin registered as normal dockable tabs (is_settings
    // == false) -- what main.cpp's tab-drawing loop and the View menu
    // iterate. Filtered from panels_ on every call (cheap: a handful
    // of panels, called once per frame).
    std::vector<RegisteredPanel> Panels() const {
        std::vector<RegisteredPanel> result;
        for (const auto& panel : panels_) {
            if (!panel.is_settings) result.push_back(panel);
        }
        return result;
    }

    // Panels a plugin registered as a Settings section (is_settings ==
    // true) -- drawn inline inside the built-in Settings panel instead
    // of as their own tab. See PLAN_settings_panel.md Fase 1.
    std::vector<RegisteredPanel> SettingsPanels() const {
        std::vector<RegisteredPanel> result;
        for (const auto& panel : panels_) {
            if (panel.is_settings) result.push_back(panel);
        }
        return result;
    }

    // --- Fase 5: write services -----------------------------------------

    // Snapshot of every apply_edit() proposal still awaiting a decision,
    // oldest first. Safe to call every frame from the main thread --
    // copies out under the lock rather than handing back a reference,
    // since a plugin thread can push a new one concurrently with
    // PendingEditsPanel drawing this list.
    std::vector<PendingEdit> PendingEdits() const;

    // Writes the edit to disk (via PluginHostCallbacks::write_approved_edit)
    // and removes it from the pending list. No-op if `id` isn't pending
    // anymore (e.g. double-click). Main-thread only.
    void ApproveEdit(int id);

    // Discards the proposal without writing anything. Main-thread only.
    void RejectEdit(int id);

    // Services one outstanding run_project() request AND one outstanding
    // design_add_component/design_edit_component active-document
    // fetch, if any -- main.cpp calls this once per frame (see the loop
    // in main.cpp). Near-instant no-op on every frame nothing is
    // currently blocked inside either of those.
    void PumpMainThreadWork();

private:
    struct LoadedPlugin {
        void* library_handle = nullptr; // HMODULE (Windows) or dlopen handle, opaque here
        AvaPluginShutdownFn shutdown = nullptr;
        std::string display_name; // file name, e.g. "ai_agent.dll"

        // Fase 9: read once at LoadAll() time (the library's already
        // open right there, no reason to defer it) so ScanAvailable()
        // can hand these back for a loaded plugin with zero extra work.
        std::string meta_name;
        std::string meta_version;
        std::string meta_author;
    };

    // C function pointers can't capture `this`, so every AvaStudioHost
    // callback recovers the owning PluginHost via `host` -- the
    // pointer the host hands the plugin is always `&host_` below, a
    // member of some live PluginHost instance, so
    // reinterpret_cast<PluginHost*>(host) is safe as long as `host`
    // really came from us (which it always does -- plugins never
    // construct their own AvaStudioHost).
    static int RegisterPanelTrampoline(AvaStudioHost* host, const AvaPanelRegistration* registration);
    static const char* GetProjectRootTrampoline(AvaStudioHost* host);
    static bool GetActiveFileTrampoline(AvaStudioHost* host, const char** out_path, const char** out_content,
                                        int* out_selection_start, int* out_selection_end);
    static bool GetLastRunOutputTrampoline(AvaStudioHost* host, const char** out_text, bool* out_had_error);
    static void LogTrampoline(AvaStudioHost* host, const char* message);
    static bool ApplyEditTrampoline(AvaStudioHost* host, const char* path, const char* new_content,
                                     const char* description, const char** out_error);
    // May be called from any thread (see plugin_api.h's comment on
    // AvaHostServices::run_project) -- blocks the calling thread on
    // run_mailbox_ until PumpMainThreadWork() (main thread only)
    // services the request. This is deliberately the *only* place a
    // plugin thread waits on the main thread -- apply_edit above never
    // blocks, it just queues.
    static bool RunProjectTrampoline(AvaStudioHost* host, const char** out_output, bool* out_had_error,
                                      const char** out_error);

    // Fase 6 -- both design services end up here: mutate an IComponent
    // tree, serialize it, and hand the result to the exact same
    // queuing logic ApplyEditTrampoline uses (this is what makes
    // design_add_component/design_edit_component share apply_edit's
    // approval gate instead of needing a second one -- see the note on
    // AvaHostServices::design_add_component in plugin_api.h). Not a
    // *Trampoline itself (no direct AvaStudioHost entry point) --
    // called from the two below after they've computed `new_content`.
    static bool QueueEdit(PluginHost* self, const std::string& path, const std::string& new_content,
                           const std::string& description, std::string& out_error);

    // Blocks the calling (plugin) thread until PumpMainThreadWork()
    // (main thread only) has run callbacks_.get_active_avaui_document
    // -- same hand-off shape as RunProjectTrampoline/run_mailbox_ above,
    // via design_doc_mailbox_ instead, since this also has to touch
    // EditorState. Returns false if no .avaui tab is active.
    static bool FetchActiveAvauiDocument(PluginHost* self, std::string& out_path, std::string& out_source);

    static bool DesignAddComponentTrampoline(AvaStudioHost* host, const char* parent_id, const char* type,
                                              const char* id, const char* properties_kv, const char** out_error);
    static bool DesignEditComponentTrampoline(AvaStudioHost* host, const char* node_id, const char* properties_kv,
                                               const char* new_id, const char** out_error);

    PluginHostCallbacks callbacks_;
    AvaStudioHost host_{};
    std::vector<LoadedPlugin> loaded_;
    std::vector<RegisteredPanel> panels_;

    // Whichever plugin's ava_plugin_init() is currently on the stack --
    // RegisterPanelTrampoline stamps this onto RegisteredPanel::
    // owner_plugin. Only meaningful during LoadAll()'s call into a
    // given plugin; register_panel() is documented (plugin_api.h) as
    // only safe to call from within ava_plugin_init(), so this never
    // needs to be anything fancier than one field reused per plugin.
    std::string loading_plugin_name_;

    // Scratch storage the *Trampoline getters return const char*
    // into. "Host-owned, valid only for this frame" (see
    // plugin_api.h) is honored by simply overwriting these right
    // before each call -- a plugin is expected to copy what it needs
    // before making another host call, exactly like avahost's C API
    // already documents for its own string returns.
    mutable std::string scratch_project_root_;
    mutable std::string scratch_active_path_;
    mutable std::string scratch_active_content_;
    mutable std::string scratch_last_run_text_;
    mutable std::string scratch_apply_edit_error_;
    mutable std::string scratch_run_output_;
    mutable std::string scratch_run_error_;
    mutable std::string scratch_design_error_; // Fase 6 -- design_add_component/design_edit_component

    // Fase 6: single-slot mailbox bridging design_add_component/
    // design_edit_component (called from a plugin's own thread) to
    // callbacks_.get_active_avaui_document (main thread only, same
    // reasoning as RunMailbox above). Only carries the READ side --
    // once the plugin thread has `path`/`avaui_source`, the actual
    // IComponent mutation + WriteAvauiText() serialization happens
    // entirely on that thread (fully local values, no shared state),
    // and the result is queued via QueueEdit (pending_edits_mutex_),
    // not through this mailbox again.
    struct DesignDocMailbox {
        std::mutex mutex;
        std::condition_variable cv;
        bool request_pending = false;
        bool result_ready = false;
        bool doc_available = false; // false = active tab isn't .avaui
        std::string path;
        std::string avaui_source;
    };
    mutable DesignDocMailbox design_doc_mailbox_;

    // --- Fase 5 state ----------------------------------------------------

    mutable std::mutex pending_edits_mutex_;
    std::vector<PendingEdit> pending_edits_;
    int next_edit_id_ = 1;

    // Single-slot mailbox bridging AvaHostServices::run_project (called
    // from a plugin's own thread -- ai_agent's tool-use loop runs on a
    // std::thread, see ai_agent_plugin.cpp's SendMessage) to the main
    // thread, since actually running a script touches EditorState/
    // EngineBridge exactly like the F5 hotkey does, and neither is
    // safe to touch from a second thread while ImGui might be reading
    // them the same frame. Only one request is served at a time --
    // RunProjectTrampoline waits its turn if another is already in
    // flight before posting its own, rather than clobbering it.
    struct RunMailbox {
        std::mutex mutex;
        std::condition_variable cv;
        bool request_pending = false; // a thread is waiting on request_pending -> result_ready
        bool result_ready = false;
        RunProjectResult result;
    };
    mutable RunMailbox run_mailbox_;

    // --- Fase 9: plugin metadata cache -----------------------------------
    // ScanAvailable() runs once per frame (see main.cpp) and has to
    // report plugin_name/version/author even for plugins that are
    // disabled -- i.e. never touched by LoadAll(), so there's no
    // LoadedPlugin to read them from. Reading them requires briefly
    // dlopen-ing the file, which is too expensive to redo every single
    // frame for every disabled plugin. This caches that read per file
    // name, keyed additionally by the file's last-write-time so
    // dropping in an updated build of a disabled plugin is picked up
    // without needing to enable it first.
    struct PluginMetadataCacheEntry {
        std::filesystem::file_time_type mtime{};
        std::string name;
        std::string version;
        std::string author;
    };
    mutable std::unordered_map<std::string, PluginMetadataCacheEntry> metadata_cache_;
};

} // namespace studio
