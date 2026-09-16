#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "project/avaproj_file.h"
#include "project/avaproj_user_file.h"
#include "util/log_bridge.h"

namespace studio {

enum class RunTarget { kConsole, kDesktopUi, kLibrary };

struct BuildPanelState {
    std::atomic<bool> building{false};

    std::mutex mutex;
    std::string log;
    bool has_result = false;
    bool last_success = false;
    std::string result_path;

    bool logged_to_output = false;

    bool show_result_dialog = false;
    bool dialog_success = false;
    std::string dialog_result_path;

    std::string::size_type log_forwarded_upto = 0;

    // Set on the main thread right before the worker thread is launched
    // (StartBuild, build_panel.cpp) -- read on the main thread while
    // drawing the panel to show elapsed time. Never touched from the
    // worker thread, so it doesn't need `mutex`.
    std::chrono::steady_clock::time_point build_started_at{};

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

    RunTarget selected_run_target = RunTarget::kConsole;
    RunTarget last_built_target = RunTarget::kConsole;
    bool run_target_preselected = false;

    bool launch_on_success = false;

    std::vector<std::string> launch_extra_args;

    ~BuildPanelState() {
        if (worker.joinable()) worker.join();
        if (vcpkg_worker.joinable()) vcpkg_worker.join();
    }
};

enum class BuildBrowseField {
    kNone,
    kAvaCliPath,
    kKeyFile,
    kVcpkgRoot,
    kCompilerPathDesktop,
    kCompilerPathBarekernel,
};

struct BuildPanelResult {
    BuildBrowseField browse_requested = BuildBrowseField::kNone;

    bool dirty = false;

    // El usuario pidió abrir las Propiedades del proyecto (destino, entry
    // file, out dir, ofuscación, etc. -- todo lo que ahora vive en el
    // .avaproj y ya no se edita en esta ventana).
    bool open_project_properties = false;
};

BuildPanelResult DrawBuildPanel(BuildPanelState& state, AvaProjFile& proj, AvaProjUserFile& user,
                                 const std::string& explorer_root_dir, BuildBrowseField browsed_field,
                                 const std::string& browsed_value, LogBridge& log_bridge, bool project_ambiguous,
                                 const std::vector<std::string>& avaproj_candidates, bool* p_open = nullptr);

std::string NormalizeEntryFilePath(const std::string& project_dir, const std::string& picked_path);

struct TriggerBuildOutcome {
    bool entry_file_missing = false;
    std::string project_dir;
};

struct BuildStep {
    std::string exe_path;
    std::vector<std::string> args;
    std::string step_label;
};

void StartMultiStepBuild(BuildPanelState& state, std::vector<BuildStep> steps, std::string expected_result_path);

// Fix: `project_ambiguous`/`avaproj_candidates` vienen de
// ProjectConfig::ambiguous_avaproj/avaproj_candidates (project_config.h) --
// cuando `explorer_root_dir` contiene mas de un .avaproj, TriggerBuild
// rechaza el build con un mensaje que lista los candidatos en vez de
// adivinar cual proyecto compilar.
//
// `force_output_type`/`force_no_ui` permiten a un caller pedir esta corrida
// con un output_type/uses_ui distinto al guardado en `proj`, sin tocar el
// .avaproj real -- usado por el target Console (force kExe + force_no_ui)
// y el target Library (force kLibrary) del selector de Build & Run.
TriggerBuildOutcome TriggerBuild(BuildPanelState& state, const AvaProjFile& proj, const AvaProjUserFile& user,
                                  const std::string& explorer_root_dir, LogBridge& log_bridge,
                                  bool project_ambiguous = false,
                                  const std::vector<std::string>& avaproj_candidates = {},
                                  std::optional<AvaProjOutputType> force_output_type = std::nullopt,
                                  bool force_no_ui = false);

TriggerBuildOutcome TriggerDesktopUiRunBuild(BuildPanelState& state, const AvaProjFile& proj,
                                              const AvaProjUserFile& user, const std::string& explorer_root_dir,
                                              LogBridge& log_bridge, bool project_ambiguous = false,
                                              const std::vector<std::string>& avaproj_candidates = {});

TriggerBuildOutcome DispatchBuildAndRun(BuildPanelState& state, RunTarget target, const AvaProjFile& proj,
                                         const AvaProjUserFile& user, const std::string& explorer_root_dir,
                                         LogBridge& log_bridge, bool project_ambiguous = false,
                                         const std::vector<std::string>& avaproj_candidates = {});

void PollBuild(BuildPanelState& state, LogBridge& log_bridge);

std::string ResolveVcpkgInstallTarget(const AvaProjUserFile& user);

void StartVcpkgInstall(BuildPanelState& state, std::string target_dir, std::string triplet);

}
