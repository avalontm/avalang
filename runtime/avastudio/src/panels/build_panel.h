#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

#include "util/log_bridge.h"
#include "util/settings.h"

namespace studio {

struct BuildPanelState {
    std::atomic<bool> building{false};

    std::mutex mutex;
    std::string log;
    bool has_result = false;
    bool last_success = false;
    std::string result_path;

    bool logged_to_output = false;

    std::string::size_type log_forwarded_upto = 0;

    std::thread worker;

    std::atomic<bool> installing_vcpkg{false};
    std::mutex vcpkg_mutex;
    std::string vcpkg_log;
    bool vcpkg_has_result = false;
    bool vcpkg_last_success = false;
    std::string vcpkg_installed_dir;
    bool vcpkg_logged_to_output = false;

    std::string::size_type vcpkg_log_forwarded_upto = 0;
    std::thread vcpkg_worker;

    ~BuildPanelState() {
        if (worker.joinable()) worker.join();
        if (vcpkg_worker.joinable()) vcpkg_worker.join();
    }
};

enum class BuildBrowseField {
    kNone,
    kProjectDir,
    kOutputDir,
    kAvaCliPath,
    kKeyFile,
    kEntryFile,
    kVcpkgRoot,
    kCompilerPathDesktop,
    kCompilerPathBarekernel,
};

struct BuildPanelResult {

    BuildBrowseField browse_requested = BuildBrowseField::kNone;

    bool settings_dirty = false;
};

BuildPanelResult DrawBuildPanel(BuildPanelState& state, StudioSettings& settings,
                                 const std::string& explorer_root_dir, BuildBrowseField browsed_field,
                                 const std::string& browsed_value, LogBridge& log_bridge, bool* p_open = nullptr);

// Same "which folder is the project" resolution the panel and Run Project/Check already agree
// on: settings.build_project_dir if the user pinned one under the panel's Project section,
// otherwise whatever's currently open in the editor (explorer_root_dir). Exposed so the Run
// menu's Build action (main.cpp) can resolve the same default without duplicating the ternary.
std::string ResolveBuildProjectDir(const StudioSettings& settings, const std::string& explorer_root_dir);

// Converts an absolute path picked via a file dialog into the relative form build_entry_file
// wants (relative to project_dir) when possible, falling back to the absolute path when the
// picked file lives outside project_dir. Shared by the Entry script browse button in
// DrawBuildPanel and by main.cpp's "entry file missing -> let the user pick one" recovery flow
// (see TriggerBuildOutcome below) so both agree on the same normalization.
std::string NormalizeEntryFilePath(const std::string& project_dir, const std::string& picked_path);

// Result of TriggerBuild. `entry_file_missing` is set when a build could not even be started
// because the configured/detected entry .ava file does not exist on disk (e.g. it was left over
// from a different project folder) -- distinct from `build_entry_file` being empty, which is
// its own setup error logged by TriggerBuild itself. The caller (main.cpp) uses this signal to
// pop a file picker instead of letting ava_cli fail later with a raw "--entry no existe" error.
// `project_dir` is the resolved project folder, for the picker's initial directory.
struct TriggerBuildOutcome {
    bool entry_file_missing = false;
    std::string project_dir;
};

// Kicks off an actual build using the paths currently configured in `settings` (+ the usual
// auto-detection fallbacks for ava_cli/repo-root/entry file), the same request the panel's old
// in-panel Build button used to send. No-op if a build is already running. Now the only way to
// start a build -- called from the Run menu / Ctrl+B / Command Palette in main.cpp -- so setup
// errors (missing ava_cli, repo root not found, etc.) are logged straight to `log_bridge`
// instead of waiting for the panel to be open to surface them.
TriggerBuildOutcome TriggerBuild(BuildPanelState& state, const StudioSettings& settings,
                                  const std::string& explorer_root_dir, LogBridge& log_bridge);

// Frame-polled unconditionally (main.cpp's loop, alongside PollScriptRun) so a build's log and
// success/failure line always reach the Output panel even while the Build panel itself is
// closed -- the panel no longer owns any of the "did it work" reporting.
void PollBuild(BuildPanelState& state, LogBridge& log_bridge);

// Resolves the same "where should vcpkg live" default the Install vcpkg
// button already computed inline (settings.build_vcpkg_root, falling back to
// an auto-detected vcpkg next to the repo root/executable). Pulled out to a
// public function (Fase 3, Command Palette) so the "Install vcpkg" command
// doesn't have to duplicate this resolution logic outside the panel.
std::string ResolveVcpkgInstallTarget(const StudioSettings& settings);

// Kicks off the (possibly clone+bootstrap+install) vcpkg worker thread on
// `state`; no-op if an install is already running. Exposed publicly for the
// same reason as ResolveVcpkgInstallTarget above -- the Command Palette's
// "Install vcpkg" entry calls this directly instead of only being reachable
// from the button click inside DrawBuildPanel.
void StartVcpkgInstall(BuildPanelState& state, std::string target_dir, std::string triplet);

}
