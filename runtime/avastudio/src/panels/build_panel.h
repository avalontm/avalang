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
    kRepoRoot,
    kAvaCliPath,
    kKeyFile,
    kEntryFile,
    kVcpkgRoot,
    kToolchainDir,
};

struct BuildPanelResult {

    BuildBrowseField browse_requested = BuildBrowseField::kNone;

    bool settings_dirty = false;
};

BuildPanelResult DrawBuildPanel(BuildPanelState& state, StudioSettings& settings,
                                 const std::string& explorer_root_dir, BuildBrowseField browsed_field,
                                 const std::string& browsed_value, LogBridge& log_bridge, bool* p_open = nullptr);

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
