#include "panels/build_panel.h"

#include <algorithm>
#include <filesystem>
#include <functional>
#include <vector>

#include "imgui.h"
#include "imgui_stdlib.h" // ImGui::InputText/InputTextMultiline(const char*, std::string*, ...)
#include "palette.h"
#include "platform/Platform.h"
#include "platform/interfaces/IProcessStream.h"
#include "util/ava_cli_locator.h"
#include "util/process_log.h"

#if defined(_WIN32)
    #define AVASTUDIO_EXE_SUFFIX ".exe"
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#else
    #define AVASTUDIO_EXE_SUFFIX ""
    #include <unistd.h>
    #include <limits.h>
#endif

namespace studio {

namespace {
namespace fs = std::filesystem;

// --- auto-detection ------------------------------------------------
// SelfExecutableDir() / DetectAvaCliPath() used to live here; both moved
// to util/ava_cli_locator.h/.cpp (verbatim, same logic) so the new
// out-of-process script runner (panels/terminal_panel.cpp) can reuse
// them instead of duplicating this. Nothing else in this file changes.

// A directory "looks like" the AvaLang repo root if it has the same two
// markers runtime/avacli/src/build_command.cpp itself checks for before
// accepting --repo-root -- reusing that exact check here means this
// panel never suggests a root ava_cli would reject anyway.
bool LooksLikeRepoRoot(const fs::path& dir) {
    std::error_code ec;
    return fs::exists(dir / "CMakeLists.txt", ec) &&
           fs::exists(dir / "runtime" / "avapack" / "CMakeLists.txt", ec);
}

// Walks upward from `start` (inclusive) looking for the repo root.
// Tried against both ava_studio.exe's own location (works for any
// project, as long as ava_studio.exe still lives inside the checkout --
// true for build_cli/ builds) and, as a fallback, the currently open
// project folder (works if the project itself lives inside the repo,
// e.g. samples/<name>).
fs::path DetectRepoRoot(const fs::path& start) {
    fs::path dir = start;
    std::error_code ec;
    for (int i = 0; i < 8 && !dir.empty(); ++i) {
        if (LooksLikeRepoRoot(dir)) return dir;
        fs::path parent = dir.parent_path();
        if (parent == dir) break;
        dir = parent;
    }
    return {};
}

// A directory "looks like" a vcpkg checkout if it has vcpkg.exe (already
// bootstrapped) -- vcpkg.bat/vcpkg (POSIX) also exist pre-bootstrap, but
// there's nothing useful to point --repo-root-style detection at until
// bootstrap-vcpkg has actually run once.
bool LooksLikeVcpkgRoot(const fs::path& dir) {
    std::error_code ec;
#if defined(_WIN32)
    return fs::exists(dir / "vcpkg.exe", ec);
#else
    return fs::exists(dir / "vcpkg", ec);
#endif
}

// Mirrors scripts/build/install.bat's own fallback order: an already
// exported VCPKG_ROOT wins (so a machine set up via install.bat/manually
// keeps working with zero configuration here), otherwise fall back to
// install.bat's own default clone target, "<repo_root>/vcpkg". If neither
// of those pan out (e.g. a packaged AvaLang install where the source repo
// isn't present), try next to ava_studio.exe itself -- covers an
// install_studio.bat-style layout where vcpkg/ was dropped alongside the
// executable instead of inside a full repo checkout.
fs::path DetectVcpkgRoot(const fs::path& repo_root) {
    std::string env_value;
    auto platform = ava::platform::Platform::Create();
    if (platform && platform->Environment().GetEnvVar("VCPKG_ROOT", env_value) && !env_value.empty()) {
        fs::path from_env(env_value);
        std::error_code ec;
        if (fs::exists(from_env, ec)) return from_env;
    }
    if (!repo_root.empty()) {
        fs::path candidate = repo_root / "vcpkg";
        if (LooksLikeVcpkgRoot(candidate)) return candidate;
    }
    const fs::path self_dir = SelfExecutableDir();
    if (!self_dir.empty() && self_dir != repo_root) {
        fs::path candidate = self_dir / "vcpkg";
        if (LooksLikeVcpkgRoot(candidate)) return candidate;
    }
    return {};
}


// in a shallow (depth-limited) walk, alphabetically -- good enough for
// "just point me at something" without scanning a huge tree.
std::string DetectEntryFile(const fs::path& project_dir) {
    std::error_code ec;
    if (fs::exists(project_dir / "main.ava", ec)) return "main.ava";

    std::vector<std::string> found;
    constexpr int kMaxDepth = 3;
    std::function<void(const fs::path&, int)> walk = [&](const fs::path& dir, int depth) {
        if (depth > kMaxDepth) return;
        std::error_code walk_ec;
        for (const auto& entry : fs::directory_iterator(dir, walk_ec)) {
            if (entry.is_directory()) {
                walk(entry.path(), depth + 1);
            } else if (entry.path().extension() == ".ava") {
                found.push_back(fs::relative(entry.path(), project_dir, ec).generic_string());
            }
        }
    };
    walk(project_dir, 0);
    if (found.empty()) return "";
    std::sort(found.begin(), found.end());
    return found.front();
}

// FlushLogToOutput() used to live here too -- moved to
// util/process_log.h/.cpp (verbatim), same reason as above.

// --- background build -----------------------------------------------
void StartBuild(BuildPanelState& state, std::vector<std::string> args, std::string ava_cli_path,
                 std::string expected_result_path) {
    if (state.building.load()) return; // one build at a time
    if (state.worker.joinable()) state.worker.join(); // previous run already finished, just reap it

    {
        std::lock_guard<std::mutex> lock(state.mutex);
        state.log.clear();
        state.log_forwarded_upto = 0;
        state.has_result = false;
    }
    state.logged_to_output = false;
    state.building = true;

    state.worker = std::thread([&state, args = std::move(args), ava_cli_path = std::move(ava_cli_path),
                                 expected_result_path = std::move(expected_result_path)]() {
        auto platform = ava::platform::Platform::Create();
        ava::platform::IProcess& process = platform->Process();
        // IProcessStream is additive (see IProcessStream.h) -- backends
        // that implement it (Windows) get live output as ava_cli
        // actually produces it; backends that don't (Linux/Mac stubs)
        // fall back to the old blocking Execute() below, same as before.
        auto* streaming = dynamic_cast<ava::platform::IProcessStream*>(&process);

        bool launched = false;
        int exit_code = -1;

        if (streaming) {
            launched = streaming->ExecuteStreaming(
                ava_cli_path, args,
                [&state](const std::string& chunk) {
                    std::lock_guard<std::mutex> lock(state.mutex);
                    state.log += chunk;
                },
                exit_code);
        } else {
            ava::platform::ProcessResult result;
            launched = process.Execute(ava_cli_path, args, result);
            if (launched) {
                std::lock_guard<std::mutex> lock(state.mutex);
                state.log = result.stdout_output;
                if (!result.stderr_output.empty()) {
                    if (!state.log.empty()) state.log += "\n";
                    state.log += result.stderr_output;
                }
                exit_code = result.exit_code;
            }
        }

        std::lock_guard<std::mutex> lock(state.mutex);
        if (!launched) {
            state.log = "error: could not run '" + ava_cli_path +
                         "' -- check the ava_cli path under Advanced.\n";
            state.last_success = false;
        } else {
            state.last_success = (exit_code == 0);
            if (state.last_success) state.result_path = expected_result_path;
        }
        state.has_result = true;
        state.building = false;
    });
}

// --- background vcpkg install ----------------------------------------
// Same steps as scripts/build/install.bat's own "3. vcpkg" /
// "4. antlr4 C++ runtime" / "4b. libcurl" sections, just run from Ava
// Studio instead of a terminal: clone (if not already present),
// bootstrap, then `vcpkg install antlr4:<triplet>` and
// `vcpkg install curl:<triplet>` -- antlr4 is what upgrades avalang.dll
// from the stub frontend to the real one, curl is what the ai_agent
// plugin needs to configure at all. Runs every step even after one
// fails (matching this panel's "show everything, let the person read
// the log" philosophy from the Build side) but only reports overall
// success if all of them exited 0.
void StartVcpkgInstall(BuildPanelState& state, std::string target_dir, std::string triplet) {
    if (state.installing_vcpkg.load()) return; // one install at a time
    if (state.vcpkg_worker.joinable()) state.vcpkg_worker.join(); // reap a finished previous run

    {
        std::lock_guard<std::mutex> lock(state.vcpkg_mutex);
        state.vcpkg_log.clear();
        state.vcpkg_log_forwarded_upto = 0;
        state.vcpkg_has_result = false;
    }
    state.vcpkg_logged_to_output = false;
    state.installing_vcpkg = true;

    state.vcpkg_worker = std::thread([&state, target_dir = std::move(target_dir),
                                       triplet = std::move(triplet)]() {
        auto platform = ava::platform::Platform::Create();
        ava::platform::IProcess& process = platform->Process();
        // Same IProcessStream/Execute fallback split as StartBuild above.
        auto* streaming = dynamic_cast<ava::platform::IProcessStream*>(&process);
        bool ok = true;

        // Appends straight into state.vcpkg_log live (under the lock) so
        // DrawBuildPanel() sees each step's output as it happens instead
        // of only once the whole install finishes.
        auto append = [&](const std::string& text) {
            if (text.empty()) return;
            std::lock_guard<std::mutex> lock(state.vcpkg_mutex);
            state.vcpkg_log += text;
        };
        auto run_step = [&](const std::string& what, const std::string& cmd,
                             const std::vector<std::string>& args) -> bool {
            if (!ok) return false; // a previous step already failed -- stop the chain
            append("$ " + what + "\n");

            bool launched = false;
            int exit_code = -1;
            if (streaming) {
                launched = streaming->ExecuteStreaming(
                    cmd, args, [&](const std::string& chunk) { append(chunk); }, exit_code);
            } else {
                ava::platform::ProcessResult r;
                launched = process.Execute(cmd, args, r);
                if (launched) {
                    append(r.stdout_output);
                    append(r.stderr_output);
                    exit_code = r.exit_code;
                }
            }

            if (!launched) {
                append("error: could not run '" + cmd + "' (" + what + ") -- is it on PATH?\n");
                ok = false;
                return false;
            }
            if (exit_code != 0) {
                ok = false;
                return false;
            }
            return true;
        };

        const fs::path vcpkg_dir(target_dir);
        const fs::path vcpkg_exe = vcpkg_dir /
#if defined(_WIN32)
            "vcpkg.exe";
#else
            "vcpkg";
#endif
        std::error_code ec;
        if (fs::exists(vcpkg_exe, ec)) {
            append("[OK] vcpkg already present at " + target_dir + " -- skipping clone/bootstrap.\n");
        } else {
            run_step("git clone", "git", {"clone", "https://github.com/microsoft/vcpkg", target_dir});
            if (ok) {
#if defined(_WIN32)
                run_step("bootstrap-vcpkg", (vcpkg_dir / "bootstrap-vcpkg.bat").string(),
                          {"-disableMetrics"});
#else
                run_step("bootstrap-vcpkg", (vcpkg_dir / "bootstrap-vcpkg.sh").string(),
                          {"-disableMetrics"});
#endif
            }
        }
        if (ok) run_step("vcpkg install antlr4", vcpkg_exe.string(), {"install", "antlr4:" + triplet});
        if (ok) run_step("vcpkg install curl", vcpkg_exe.string(), {"install", "curl:" + triplet});

        std::lock_guard<std::mutex> lock(state.vcpkg_mutex);
        state.vcpkg_last_success = ok;
        if (ok) state.vcpkg_installed_dir = target_dir;
        state.vcpkg_has_result = true;
        state.installing_vcpkg = false;
    });
}

// A labeled text field with an optional Browse button next to it,
// shared by every row in this panel so the layout (label above,
// input+button below, fixed gap) stays identical across all of them.
bool DrawPathRow(const char* label, const char* hint, std::string& value, const char* browse_id,
                  BuildBrowseField field, BuildPanelResult& result) {
    ImGui::TextColored(palette::FromHex(palette::kTextMuted), "%s", label);
    ImGui::SetNextItemWidth(-90.0f);
    bool edited = ImGui::InputTextWithHint((std::string("##") + label).c_str(), hint, &value);
    bool committed = ImGui::IsItemDeactivatedAfterEdit();
    ImGui::SameLine();
    // "##" + label makes the ID unique per row even though every row
    // shows the same visible "Browse..." text -- otherwise ImGui hashes
    // the literal label into the same ID and every Browse button in
    // this window becomes the same widget.
    if (ImGui::Button((std::string(browse_id) + "##" + label).c_str(), ImVec2(80.0f, 0.0f))) {
        result.browse_requested = field;
    }
    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    (void)edited;
    return committed;
}

} // namespace

BuildPanelResult DrawBuildPanel(BuildPanelState& state, StudioSettings& settings,
                                 const std::string& explorer_root_dir, BuildBrowseField browsed_field,
                                 const std::string& browsed_value, LogBridge& log_bridge, bool* p_open) {
    BuildPanelResult result;

    // Apply a completed native-dialog round trip before drawing, so the
    // field shows the new value this same frame -- same pattern as
    // settings_panel.h's browsed_folder.
    if (browsed_field != BuildBrowseField::kNone && !browsed_value.empty()) {
        switch (browsed_field) {
            case BuildBrowseField::kProjectDir:  settings.build_project_dir  = browsed_value; break;
            case BuildBrowseField::kOutputDir:   settings.build_out_dir      = browsed_value; break;
            case BuildBrowseField::kRepoRoot:    settings.build_repo_root    = browsed_value; break;
            case BuildBrowseField::kAvaCliPath:  settings.build_ava_cli_path = browsed_value; break;
            case BuildBrowseField::kKeyFile:     settings.build_key_file     = browsed_value; break;
            case BuildBrowseField::kVcpkgRoot:   settings.build_vcpkg_root   = browsed_value; break;
            case BuildBrowseField::kEntryFile: {
                // --entry wants a path relative to --project (see
                // build_command.cpp's --help), but the file dialog
                // returns an absolute path -- convert it, falling back
                // to the absolute path only if it turns out to live
                // outside the project folder.
                const fs::path project_dir = settings.build_project_dir.empty()
                                                  ? fs::path(explorer_root_dir)
                                                  : fs::path(settings.build_project_dir);
                std::error_code ec;
                fs::path rel = fs::relative(fs::path(browsed_value), project_dir, ec);
                settings.build_entry_file =
                    (!ec && !rel.empty() && rel.native().rfind(fs::path("..").native(), 0) != 0)
                        ? rel.generic_string()
                        : browsed_value;
                break;
            }
            case BuildBrowseField::kNone: break;
        }
        result.settings_dirty = true;
    }

    ImGui::Begin("Build", p_open);

    ImGui::TextWrapped(
        "Packages the current project into a standalone .exe (see runtime/avapack/README.md). "
        "This runs 'ava_cli build' in the background -- everything below just fills in its flags.");
    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    // --- Project ---------------------------------------------------
    ImGui::TextColored(palette::FromHex(palette::kInfo), "Project");
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    const std::string project_hint = "(default: " + explorer_root_dir + ")";
    if (DrawPathRow("Project folder", project_hint.c_str(), settings.build_project_dir,
                     "Browse...", BuildBrowseField::kProjectDir, result)) {
        result.settings_dirty = true;
    }
    const fs::path project_dir =
        settings.build_project_dir.empty() ? fs::path(explorer_root_dir) : fs::path(settings.build_project_dir);

    ImGui::TextColored(palette::FromHex(palette::kTextMuted), "Entry script (.ava)");
    ImGui::SetNextItemWidth(-180.0f);
    bool entry_edited = ImGui::InputTextWithHint("##EntryScript", "(default: auto-detect main.ava)",
                                                  &settings.build_entry_file);
    if (ImGui::IsItemDeactivatedAfterEdit()) result.settings_dirty = true;
    (void)entry_edited;
    ImGui::SameLine();
    if (ImGui::Button("Detect", ImVec2(70.0f, 0.0f))) {
        std::error_code ec;
        if (fs::exists(project_dir, ec)) {
            settings.build_entry_file = DetectEntryFile(project_dir);
            result.settings_dirty = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Browse...##EntryScript", ImVec2(80.0f, 0.0f))) {
        result.browse_requested = BuildBrowseField::kEntryFile;
    }
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    const std::string out_hint = "(default: " + (project_dir / "dist").string() + ")";
    if (DrawPathRow("Output folder", out_hint.c_str(), settings.build_out_dir, "Browse...",
                     BuildBrowseField::kOutputDir, result)) {
        result.settings_dirty = true;
    }

    // --- Options -----------------------------------------------------
    ImGui::Dummy(ImVec2(0.0f, 4.0f));
    ImGui::TextColored(palette::FromHex(palette::kInfo), "Options");
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    if (ImGui::Checkbox("Obfuscate", &settings.build_obfuscate)) result.settings_dirty = true;
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "Compiles the entry to .avbc bytecode instead of embedding plain .ava source.\n"
            "Also produces a <out>.avmap SymbolMap next to the build -- keep it yourself,\n"
            "never distribute it alongside the .exe.");
    }
    if (settings.build_obfuscate) {
        ImGui::Indent();
        if (ImGui::Checkbox("Obfuscate strings", &settings.build_obfuscate_strings)) result.settings_dirty = true;
        if (ImGui::Checkbox("Flatten control flow", &settings.build_flatten_control_flow)) result.settings_dirty = true;
        ImGui::Unindent();
    }

    if (ImGui::Checkbox("Zero-disk", &settings.build_zero_disk)) result.settings_dirty = true;
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "Resolves the packaged project against an in-memory virtual filesystem instead\n"
            "of a temp dir -- no .ava/.avbc touches disk, not even briefly.");
    }

    if (ImGui::Checkbox("Debug build (unencrypted)", &settings.build_debug_unencrypted)) result.settings_dirty = true;
    if (settings.build_debug_unencrypted) {
        ImGui::SameLine();
        ImGui::TextColored(palette::FromHex(palette::kWarning), "do not distribute this .exe");
    }
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    if (DrawPathRow("AES key file (optional)", "(default: random key per build)", settings.build_key_file,
                     "Browse...", BuildBrowseField::kKeyFile, result)) {
        result.settings_dirty = true;
    }

    // --- Advanced (repo root / ava_cli path -- usually auto-detected) --
    ImGui::Dummy(ImVec2(0.0f, 4.0f));
    if (ImGui::CollapsingHeader("Advanced")) {
        ImGui::Indent();
        if (DrawPathRow("AvaLang repo root", "(default: auto-detect)", settings.build_repo_root, "Browse...",
                         BuildBrowseField::kRepoRoot, result)) {
            result.settings_dirty = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Auto-detect##Repo")) {
            fs::path detected = DetectRepoRoot(SelfExecutableDir());
            if (detected.empty()) detected = DetectRepoRoot(project_dir);
            if (!detected.empty()) {
                settings.build_repo_root = detected.string();
                result.settings_dirty = true;
            }
        }

        if (DrawPathRow("ava_cli path", "(default: auto-detect next to ava_studio.exe)",
                         settings.build_ava_cli_path, "Browse...", BuildBrowseField::kAvaCliPath, result)) {
            result.settings_dirty = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Auto-detect##AvaCli")) {
            fs::path detected = DetectAvaCliPath();
            if (!detected.empty()) {
                settings.build_ava_cli_path = detected.string();
                result.settings_dirty = true;
            }
        }

        // --- VCPKG_ROOT ------------------------------------------------
        // Only needed if avalang.dll has to be recompiled from source
        // inside build_pack\ (see runtime/avapack/README.md's Fase 0 --
        // ava_cli build prefers a prebuilt avalang.dll/avalang_ui.dll
        // next to itself and skips vcpkg entirely when it finds one).
        // Set as the VCPKG_ROOT/AVA_VCPKG_TRIPLET environment variables
        // on ava_studio.exe's own process right before launching
        // ava_cli, the same way scripts/build/build_studio.bat/
        // install.bat rely on VCPKG_ROOT already being exported --
        // ava_cli.exe (a child process) inherits them automatically.
        const fs::path repo_root_for_vcpkg =
            settings.build_repo_root.empty() ? DetectRepoRoot(SelfExecutableDir()) : fs::path(settings.build_repo_root);
        const fs::path detected_vcpkg = DetectVcpkgRoot(repo_root_for_vcpkg);
        const std::string vcpkg_hint =
            detected_vcpkg.empty() ? "(not found -- click Install vcpkg below)"
                                    : "(default: " + detected_vcpkg.string() + ")";
        if (DrawPathRow("VCPKG_ROOT (optional)", vcpkg_hint.c_str(), settings.build_vcpkg_root, "Browse...",
                         BuildBrowseField::kVcpkgRoot, result)) {
            result.settings_dirty = true;
        }

        const bool vcpkg_installing = state.installing_vcpkg.load();
        ImGui::BeginDisabled(vcpkg_installing || state.building.load());
        if (ImGui::Button(vcpkg_installing ? "Installing vcpkg..." : "Install vcpkg")) {
            fs::path install_target = settings.build_vcpkg_root.empty()
                                           ? (detected_vcpkg.empty()
                                                  ? (repo_root_for_vcpkg.empty()
                                                         ? SelfExecutableDir() / "vcpkg"
                                                         : repo_root_for_vcpkg / "vcpkg")
                                                  : detected_vcpkg)
                                           : fs::path(settings.build_vcpkg_root);
            StartVcpkgInstall(state, install_target.string(), "x64-windows-static-md");
        }
        ImGui::EndDisabled();
        if (vcpkg_installing) {
            ImGui::SameLine();
            ImGui::TextColored(palette::FromHex(palette::kTextMuted),
                                "cloning + bootstrapping + building antlr4/curl -- can take several minutes...");
        }
        {
            std::lock_guard<std::mutex> vcpkg_lock(state.vcpkg_mutex);
            // Live: forward whatever vcpkg has printed so far every frame,
            // whether the install is still running or already finished.
            FlushLogToOutput(state.vcpkg_log, state.vcpkg_log_forwarded_upto, state.vcpkg_has_result,
                              "[vcpkg]   ", log_bridge);
            if (state.vcpkg_has_result) {
                if (state.vcpkg_last_success) {
                    ImGui::TextColored(palette::FromHex(palette::kSuccess), "vcpkg ready at %s",
                                        state.vcpkg_installed_dir.c_str());
                    if (settings.build_vcpkg_root.empty()) {
                        // Auto-fill so future builds (and future panel
                        // opens) point straight at it instead of
                        // re-running DetectVcpkgRoot's env-var/heuristic
                        // fallback every time.
                        settings.build_vcpkg_root = state.vcpkg_installed_dir;
                        result.settings_dirty = true;
                    }
                } else {
                    ImGui::TextColored(palette::FromHex(palette::kError),
                                        "vcpkg install failed -- see log in Output.");
                }
                if (!state.vcpkg_logged_to_output) {
                    log_bridge.Log(state.vcpkg_last_success ? "[vcpkg] install succeeded -> " +
                                                                   state.vcpkg_installed_dir
                                                             : "[vcpkg] install failed:");
                    state.vcpkg_logged_to_output = true;
                }
            }
        }
        ImGui::Unindent();
    }

    // --- Build action --------------------------------------------------
    ImGui::Dummy(ImVec2(0.0f, 10.0f));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, 8.0f));

    const bool building = state.building.load();
    ImGui::BeginDisabled(building || state.installing_vcpkg.load());
    if (ImGui::Button(building ? "Building..." : "Build Executable", ImVec2(200.0f, 36.0f))) {
        fs::path ava_cli = settings.build_ava_cli_path.empty() ? DetectAvaCliPath()
                                                                : fs::path(settings.build_ava_cli_path);
        fs::path repo_root = settings.build_repo_root.empty()
                                  ? [&]() {
                                        fs::path d = DetectRepoRoot(SelfExecutableDir());
                                        return d.empty() ? DetectRepoRoot(project_dir) : d;
                                    }()
                                  : fs::path(settings.build_repo_root);
        std::string entry =
            settings.build_entry_file.empty() ? DetectEntryFile(project_dir) : settings.build_entry_file;
        fs::path out_dir = settings.build_out_dir.empty() ? (project_dir / "dist") : fs::path(settings.build_out_dir);

        std::error_code ec;
        std::string setup_error;
        if (ava_cli.empty() || !fs::exists(ava_cli, ec)) {
            setup_error = "could not find ava_cli(.exe) -- set its path under Advanced.";
        } else if (repo_root.empty() || !LooksLikeRepoRoot(repo_root)) {
            setup_error = "could not find the AvaLang repo root -- set it under Advanced.";
        } else if (!fs::exists(project_dir, ec) || !fs::is_directory(project_dir, ec)) {
            setup_error = "project folder does not exist.";
        } else if (entry.empty()) {
            setup_error = "could not find an entry .ava file -- set 'Entry script' above.";
        } else {
            // Export VCPKG_ROOT (+ the fixed triplet the rest of this repo's
            // scripts assume, see install.bat's WHY comment) on ava_studio's
            // own process before launching ava_cli -- CreateProcess with a
            // null environment block (WinProcess.cpp) inherits the caller's
            // env, so ava_cli's own std::getenv("VCPKG_ROOT") picks this up
            // exactly as if it had been set in the shell ava_studio.exe was
            // launched from. Only set when we actually have a value: an
            // empty/missing vcpkg is fine (ava_cli falls back to a prebuilt
            // avalang.dll if one sits next to it, see build_command.cpp).
            fs::path vcpkg_root =
                settings.build_vcpkg_root.empty() ? DetectVcpkgRoot(repo_root) : fs::path(settings.build_vcpkg_root);
            if (!vcpkg_root.empty()) {
                auto env_platform = ava::platform::Platform::Create();
                if (env_platform) {
                    env_platform->Environment().SetEnvVar("VCPKG_ROOT", vcpkg_root.string());
                    env_platform->Environment().SetEnvVar("AVA_VCPKG_TRIPLET", "x64-windows-static-md");
                }
            }

            // Directory mode: a trailing separator tells ava_cli build to
            // save the binary INSIDE this folder (named after `entry`)
            // rather than treating the whole string as the final .exe's
            // literal path -- see build_command.cpp's out_is_dir check.
            std::string out_arg = out_dir.string();
            if (out_arg.empty() || (out_arg.back() != '/' && out_arg.back() != '\\')) out_arg += "/";

            std::vector<std::string> args = {
                "build",
                "--project", project_dir.string(),
                "--entry", entry,
                "--out", out_arg,
                "--repo-root", repo_root.string(),
            };
            if (!settings.build_key_file.empty()) {
                args.push_back("--key-file");
                args.push_back(settings.build_key_file);
            }
            if (settings.build_obfuscate) {
                args.push_back("--obfuscate");
                if (settings.build_obfuscate_strings) args.push_back("--obfuscate-strings");
                if (settings.build_flatten_control_flow) args.push_back("--flatten-control-flow");
            }
            if (settings.build_zero_disk) args.push_back("--zero-disk");
            if (settings.build_debug_unencrypted) args.push_back("--debug");

            // Mirrors build_command.cpp's directory-mode naming rule
            // exactly (default_name = --entry's stem, "packaged" if
            // that's somehow empty) -- so the success message below
            // shows the real path ava_cli build will have written to,
            // not just the --out folder.
            std::string entry_stem = fs::path(entry).stem().string();
            if (entry_stem.empty()) entry_stem = "packaged";
            fs::path expected_exe = out_dir / (entry_stem + AVASTUDIO_EXE_SUFFIX);

            StartBuild(state, std::move(args), ava_cli.string(), expected_exe.string());
        }

        if (!setup_error.empty()) {
            std::lock_guard<std::mutex> lock(state.mutex);
            state.log = "error: " + setup_error;
            state.log_forwarded_upto = 0;
            state.has_result = true;
            state.last_success = false;
            state.logged_to_output = false;
        }
    }
    ImGui::EndDisabled();
    if (building) {
        ImGui::SameLine();
        ImGui::TextColored(palette::FromHex(palette::kTextMuted),
                            "this can take a while the first time (compiling avapack_gen)...");
    }

    // --- Output log ------------------------------------------------
    ImGui::Dummy(ImVec2(0.0f, 10.0f));
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        // Live: forward whatever the build has printed so far every
        // frame, whether it's still running or already finished --
        // state.log itself is appended to live by StartBuild()'s worker
        // (via IProcessStream), so this just keeps Output in sync with
        // what's already visible in this panel's own log box below.
        FlushLogToOutput(state.log, state.log_forwarded_upto, state.has_result, "[build]   ", log_bridge);
        if (state.has_result) {
            if (state.last_success) {
                ImGui::TextColored(palette::FromHex(palette::kSuccess), "Build succeeded -> %s",
                                    state.result_path.c_str());
            } else {
                ImGui::TextColored(palette::FromHex(palette::kError), "Build failed -- see log below.");
            }

            // The pass/fail header line itself still only goes out once,
            // right after the build actually finishes (not live, since
            // we don't know success/failure until then).
            if (!state.logged_to_output) {
                log_bridge.Log(state.last_success ? "[build] succeeded -> " + state.result_path
                                                   : "[build] failed:");
                state.logged_to_output = true;
            }
        }
        ImGui::InputTextMultiline("##BuildLog", &state.log, ImVec2(-1.0f, -1.0f),
                                   ImGuiInputTextFlags_ReadOnly);
    }

    ImGui::End();
    return result;
}

} // namespace studio
