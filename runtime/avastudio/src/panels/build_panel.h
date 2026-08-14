#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

#include "util/log_bridge.h"
#include "util/settings.h"

namespace studio {

// The "Build" panel -- a friendly front-end over `ava_cli build`
// (runtime/avacli/src/build_command.cpp), which in turn drives
// runtime/avapack (see runtime/avapack/README.md) to package an AvaLang
// project into a standalone .exe. This panel never talks to
// CMake/avapack directly; it only shells out to ava_cli, exactly like a
// developer would from the command line, so there is exactly one code
// path that knows how packaging actually works.
//
// Runtime (non-persisted) state for one Ava Studio session -- the build
// itself runs on a background std::thread (a packaging build can take
// anywhere from ~1s (incremental, prebuilt avalang.dll/avalang_ui.dll)
// to a minute+ (first --clean build), so running it inline on the UI
// thread would freeze the whole app). `mutex` guards every field below
// it; `building` is a separate atomic so DrawBuildPanel() can cheaply
// check "is a build in flight" every frame without taking the lock.
struct BuildPanelState {
    std::atomic<bool> building{false};

    std::mutex mutex;
    std::string log;              // guarded by mutex -- stdout+stderr so far/at completion
    bool has_result = false;      // guarded by mutex -- a build has finished at least once
    bool last_success = false;    // guarded by mutex -- exit code of the last finished build
    std::string result_path;      // guarded by mutex -- final .exe path, only valid if last_success

    // Guards against re-printing the "Build succeeded/failed" header line
    // into the Output panel (panels/logs_panel.h) more than once per
    // build -- DrawBuildPanel() flips this to true the frame has_result
    // first becomes true, and StartBuild() resets it to false so the
    // *next* build's header gets printed too. Only ever touched from the
    // main thread (unlike the fields above, the background worker never
    // sets this), so it does not need the mutex.
    bool logged_to_output = false;

    // How much of `log` (byte offset) has already been forwarded to the
    // Output panel, line by line -- guarded by mutex since the worker
    // thread appends to `log` live (via IProcessStream, see StartBuild)
    // while DrawBuildPanel() reads/advances this every frame, so the
    // build's output shows up in Output in real time instead of only
    // once the whole thing finishes. Reset to 0 alongside `log.clear()`
    // in StartBuild().
    std::string::size_type log_forwarded_upto = 0;

    std::thread worker;

    // --- vcpkg install (see StartVcpkgInstall in build_panel.cpp) ------
    // Same background-worker shape as the build fields above, kept
    // separate so an "Install vcpkg" run and a "Build Executable" run
    // never share state -- they're independent operations, and a slow
    // vcpkg bootstrap (first-time clone + antlr4/curl compile, several
    // minutes) shouldn't block the Build button or vice versa.
    std::atomic<bool> installing_vcpkg{false};
    std::mutex vcpkg_mutex;
    std::string vcpkg_log;                // guarded by vcpkg_mutex
    bool vcpkg_has_result = false;        // guarded by vcpkg_mutex
    bool vcpkg_last_success = false;      // guarded by vcpkg_mutex
    std::string vcpkg_installed_dir;      // guarded by vcpkg_mutex -- only valid if vcpkg_last_success
    bool vcpkg_logged_to_output = false;  // main-thread only, same convention as logged_to_output
    // Same live-forwarding offset as log_forwarded_upto above, for vcpkg_log.
    std::string::size_type vcpkg_log_forwarded_upto = 0; // guarded by vcpkg_mutex
    std::thread vcpkg_worker;

    ~BuildPanelState() {
        if (worker.joinable()) worker.join();
        if (vcpkg_worker.joinable()) vcpkg_worker.join();
    }
};

// Which field on the panel a "Browse..." click refers to -- main.cpp
// owns the actual native dialog (needs a GLFWwindow*, see
// platform/win32_titlebar.h), same round-trip pattern as
// settings_panel.h's out_browse_requested/browsed_folder. main.cpp opens
// a folder picker for kProjectDir/kOutputDir/kRepoRoot, and a file
// picker for kKeyFile/kAvaCliPath.
enum class BuildBrowseField {
    kNone,
    kProjectDir,
    kOutputDir,
    kRepoRoot,
    kAvaCliPath,
    kKeyFile,
    kEntryFile,
    kVcpkgRoot,
};

struct BuildPanelResult {
    // Set the one frame a "Browse..."/"Auto-detect" button that needs a
    // native dialog was clicked -- "" (kNone)/otherwise.
    BuildBrowseField browse_requested = BuildBrowseField::kNone;

    // Set the one frame any settings.build_* field actually changed (a
    // browsed value was applied, a checkbox flipped, a text field lost
    // focus after editing, etc.) -- main.cpp calls SaveSettings() when
    // this comes back true, same convention as settings_panel.h's
    // out_settings_dirty.
    bool settings_dirty = false;
};

// `explorer_root_dir`: the Explorer panel's currently open folder --
// used as the default for settings.build_project_dir when that field is
// still empty (first run), so the common case ("package the project I
// have open") needs zero configuration.
//
// `browsed_field`/`browsed_value`: normally kNone/"". The one frame
// after the person picks something from a native dialog opened because
// of a previous BuildPanelResult::browse_requested, main.cpp passes it
// back here so this panel can drop it into the right settings.build_*
// field.
//
// `log_bridge`: the Output panel's general log stream (panels/logs_panel.h)
// -- a running build's stdout+stderr gets forwarded here live, one line
// at a time as ava_cli actually produces it (see
// BuildPanelState::log_forwarded_upto), prefixed "[build]", in addition
// to staying in this panel's own scrollable log box, so you can watch a
// build progress from the Output tab too without needing this panel to
// be sized tall enough to show it.
//
// `p_open`: same ImGui::Begin p_open convention as every other panel
// (see settings_panel.h) -- pass the address of panel_open["Build"].
BuildPanelResult DrawBuildPanel(BuildPanelState& state, StudioSettings& settings,
                                 const std::string& explorer_root_dir, BuildBrowseField browsed_field,
                                 const std::string& browsed_value, LogBridge& log_bridge, bool* p_open = nullptr);

} // namespace studio
