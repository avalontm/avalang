#pragma once

#include <condition_variable>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "plugin_api.h"

namespace studio {

struct PendingEdit {
    int id = 0;
    std::string owner_plugin;
    std::string path;
    std::string description;
    std::string old_content;
    std::string new_content;
};

struct RunProjectResult {
    bool has_result = false;
    bool had_error = false;
    std::string output;
    std::string error;
};

struct RegisteredPanel {
    std::string name;
    std::string owner_plugin;
    AvaPanelDrawFn draw = nullptr;
    void* user_data = nullptr;
    AvaDockSlot default_dock_slot = AVA_DOCK_BOTTOM;
    bool is_settings = false;
};

struct PluginHostCallbacks {

    std::function<std::string()> get_project_root;

    std::function<bool(std::string& path, std::string& content)> get_active_file;

    std::function<bool(std::string& text, bool& had_error)> get_last_run_output;

    std::function<void(const std::string& line)> log;

    std::function<void(const PendingEdit& edit)> write_approved_edit;

    std::function<RunProjectResult()> run_project_on_main_thread;

    std::function<bool(std::string& path, std::string& avaui_source)> get_active_avaui_document;
};

struct PluginInfo {
    std::string file_name;

    bool enabled = true;

    bool loaded = false;

    std::string plugin_name;
    std::string version;
    std::string author;
};

class PluginHost {
public:
    explicit PluginHost(PluginHostCallbacks callbacks);
    ~PluginHost();

    PluginHost(const PluginHost&) = delete;
    PluginHost& operator=(const PluginHost&) = delete;

    void LoadAll(const std::string& plugins_dir, const std::vector<std::string>& disabled_plugins = {});

    void UnloadAll();

    void Reload(const std::string& plugins_dir, const std::vector<std::string>& disabled_plugins);

    std::vector<PluginInfo> ScanAvailable(const std::string& plugins_dir,
                                           const std::vector<std::string>& disabled_plugins) const;

    std::vector<RegisteredPanel> Panels() const {
        std::vector<RegisteredPanel> result;
        for (const auto& panel : panels_) {
            if (!panel.is_settings) result.push_back(panel);
        }
        return result;
    }

    std::vector<RegisteredPanel> SettingsPanels() const {
        std::vector<RegisteredPanel> result;
        for (const auto& panel : panels_) {
            if (panel.is_settings) result.push_back(panel);
        }
        return result;
    }

    std::vector<PendingEdit> PendingEdits() const;

    void ApproveEdit(int id);

    void RejectEdit(int id);

    void PumpMainThreadWork();

private:
    struct LoadedPlugin {
        void* library_handle = nullptr;
        AvaPluginShutdownFn shutdown = nullptr;
        std::string display_name;

        std::string meta_name;
        std::string meta_version;
        std::string meta_author;
    };

    static int RegisterPanelTrampoline(AvaStudioHost* host, const AvaPanelRegistration* registration);
    static const char* GetProjectRootTrampoline(AvaStudioHost* host);
    static bool GetActiveFileTrampoline(AvaStudioHost* host, const char** out_path, const char** out_content,
                                        int* out_selection_start, int* out_selection_end);
    static bool GetLastRunOutputTrampoline(AvaStudioHost* host, const char** out_text, bool* out_had_error);
    static void LogTrampoline(AvaStudioHost* host, const char* message);
    static bool ApplyEditTrampoline(AvaStudioHost* host, const char* path, const char* new_content,
                                     const char* description, const char** out_error);

    static bool RunProjectTrampoline(AvaStudioHost* host, const char** out_output, bool* out_had_error,
                                      const char** out_error);

    static bool QueueEdit(PluginHost* self, const std::string& path, const std::string& new_content,
                           const std::string& description, std::string& out_error);

    static bool FetchActiveAvauiDocument(PluginHost* self, std::string& out_path, std::string& out_source);

    static bool DesignAddComponentTrampoline(AvaStudioHost* host, const char* parent_id, const char* type,
                                              const char* id, const char* properties_kv, const char** out_error);
    static bool DesignEditComponentTrampoline(AvaStudioHost* host, const char* node_id, const char* properties_kv,
                                               const char* new_id, const char** out_error);

    PluginHostCallbacks callbacks_;
    AvaStudioHost host_{};
    std::vector<LoadedPlugin> loaded_;
    std::vector<RegisteredPanel> panels_;

    std::string loading_plugin_name_;

    mutable std::string scratch_project_root_;
    mutable std::string scratch_active_path_;
    mutable std::string scratch_active_content_;
    mutable std::string scratch_last_run_text_;
    mutable std::string scratch_apply_edit_error_;
    mutable std::string scratch_run_output_;
    mutable std::string scratch_run_error_;
    mutable std::string scratch_design_error_;

    struct DesignDocMailbox {
        std::mutex mutex;
        std::condition_variable cv;
        bool request_pending = false;
        bool result_ready = false;
        bool doc_available = false;
        std::string path;
        std::string avaui_source;
    };
    mutable DesignDocMailbox design_doc_mailbox_;

    mutable std::mutex pending_edits_mutex_;
    std::vector<PendingEdit> pending_edits_;
    int next_edit_id_ = 1;

    struct RunMailbox {
        std::mutex mutex;
        std::condition_variable cv;
        bool request_pending = false;
        bool result_ready = false;
        RunProjectResult result;
    };
    mutable RunMailbox run_mailbox_;

    struct PluginMetadataCacheEntry {
        std::filesystem::file_time_type mtime{};
        std::string name;
        std::string version;
        std::string author;
    };
    mutable std::unordered_map<std::string, PluginMetadataCacheEntry> metadata_cache_;
};

}
