#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "platform/interfaces/IProcessStream.h"
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

    std::string crash_dump_path;
    std::string crash_code;
    std::string crash_address;

    // Set (guarded by `mutex` above) from the worker thread every time a
    // chunk of stdout/stderr arrives from the child, for either the
    // build steps or the run step. Read on the UI thread (also under
    // `mutex`) to tell "still working, just quiet" apart from "nothing
    // has happened in a while" -- see kHungWarningSeconds in
    // build_panel.cpp (Fase 6, plan-debug-mode-avastudio.md: "proceso
    // colgado"). Reset at the start of every StartMultiStepBuild run so a
    // previous run's timestamp never leaks into the next one's warning.
    std::chrono::steady_clock::time_point last_output_at{};

    std::string::size_type log_forwarded_upto = 0;

    // Set (guarded by `mutex` above) from the worker thread via
    // ExecuteStreaming's on_started callback, only for the actual run step
    // (BuildStep::step_label.empty() -- see StartMultiStepBuild), and reset
    // once that step finishes. Lets the UI thread offer a "Stop" button
    // that kills a hung run (Fase 6, plan-debug-mode-avastudio.md:
    // "proceso colgado") without waiting for it to exit on its own.
    avastd::shared_ptr<ava::platform::IProcessStream::IStdinWriter> running_process;

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

    // Ruta del "sello" que se escribe cuando el build de Run (avanative en
    // build_avastudio_run) termina bien -- ver TriggerDesktopUiRunBuild:
    // si algun fuente del runtime es mas nuevo que el sello, el binario
    // cacheado esta desactualizado y se recompila. Solo la toca el hilo de
    // UI (Trigger* la setea, PollBuild la consume), no necesita `mutex`.
    std::string pending_run_stamp;

    // Resumen de una linea de por que fallo el ultimo paso (p. ej. "codigo
    // 0xC0000005 = ACCESS_VIOLATION"). Lo escribe el hilo de trabajo y lo lee
    // PollBuild, ambos bajo `mutex`.
    std::string failure_summary;

    // Fase 3.4 (plan-debug-mode-avastudio.md): checkbox "Debug mode" del
    // selector de Build & Run, independiente de proj.debug_unencrypted.
    // Vive solo aca (no en AvaProjFile/.avaproj) -- misma decision que
    // selected_run_target: preferencia de sesion, no algo que el .avaproj
    // deba recordar entre corridas.
    bool debug_mode = false;

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
// `force_debug_symbols` es el mismo patron para el checkbox "Debug mode"
// (Fase 3.4, plan-debug-mode-avastudio.md): pasa --debug-symbols a
// ava_cli build (ver build_command.cpp) para esta corrida sin tocar
// proj.debug_unencrypted, que sigue significando lo que siempre significo.
TriggerBuildOutcome TriggerBuild(BuildPanelState& state, const AvaProjFile& proj, const AvaProjUserFile& user,
                                  const std::string& explorer_root_dir, LogBridge& log_bridge,
                                  bool project_ambiguous = false,
                                  const std::vector<std::string>& avaproj_candidates = {},
                                  std::optional<AvaProjOutputType> force_output_type = std::nullopt,
                                  bool force_no_ui = false,
                                  bool force_debug_symbols = false);

TriggerBuildOutcome TriggerDesktopUiRunBuild(BuildPanelState& state, const AvaProjFile& proj,
                                              const AvaProjUserFile& user, const std::string& explorer_root_dir,
                                              LogBridge& log_bridge, bool project_ambiguous = false,
                                              const std::vector<std::string>& avaproj_candidates = {});

TriggerBuildOutcome DispatchBuildAndRun(BuildPanelState& state, RunTarget target, const AvaProjFile& proj,
                                         const AvaProjUserFile& user, const std::string& explorer_root_dir,
                                         LogBridge& log_bridge, bool project_ambiguous = false,
                                         const std::vector<std::string>& avaproj_candidates = {});

void PollBuild(BuildPanelState& state, LogBridge& log_bridge);

// Kills the process StartMultiStepBuild's run step (BuildStep with an empty
// step_label) is currently waiting on, if any -- a no-op otherwise (nothing
// running, or the running step is a build step rather than the final run,
// which this deliberately does not let the user kill mid-compile). Safe to
// call from the UI thread while the worker thread is blocked inside
// ExecuteStreaming.
void TerminateRunningProcess(BuildPanelState& state);

std::string ResolveVcpkgInstallTarget(const AvaProjUserFile& user);

void StartVcpkgInstall(BuildPanelState& state, std::string target_dir, std::string triplet);

}
