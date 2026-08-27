#include "panels/build_panel.h"

#include <filesystem>
#include <vector>

#include "imgui.h"
#include "imgui_stdlib.h"
#include "palette.h"
#include "platform/Platform.h"
#include "platform/interfaces/IProcessStream.h"
#include "util/ava_cli_locator.h"
#include "util/i18n.h"
#include "util/ui_widgets.h"
#include "util/process_log.h"
#include "util/project_utils.h"

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

std::string TrFormat(const std::string& key, std::initializer_list<std::string> args) {
    std::string result = util::Tr(key);
    for (const std::string& arg : args) {
        const size_t pos = result.find("%s");
        if (pos == std::string::npos) break;
        result = result.substr(0, pos) + arg + result.substr(pos + 2);
    }
    return result;
}

std::string TrFormat(const std::string& key, const std::string& arg) { return TrFormat(key, {arg}); }

bool LooksLikeRepoRoot(const fs::path& dir) {
    std::error_code ec;
    return fs::exists(dir / "CMakeLists.txt", ec) &&
           fs::exists(dir / "runtime" / "avapack" / "CMakeLists.txt", ec);
}

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

bool LooksLikeVcpkgRoot(const fs::path& dir) {
    std::error_code ec;
#if defined(_WIN32)
    return fs::exists(dir / "vcpkg.exe", ec);
#else
    return fs::exists(dir / "vcpkg", ec);
#endif
}

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

void StartBuild(BuildPanelState& state, std::vector<std::string> args, std::string ava_cli_path,
                 std::string expected_result_path) {
    if (state.building.load()) return;
    if (state.worker.joinable()) state.worker.join();

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

}  // close the anonymous namespace early so StartVcpkgInstall (below) gets
   // external linkage -- it's called directly from main.cpp's Command
   // Palette wiring (Fase 3), not just from the button click further down in
   // this file. DetectRepoRoot/DetectVcpkgRoot/etc. above stay internal
   // (only used inside this TU); the `fs` alias declared inside that
   // namespace is still visible down here and for the rest of the file,
   // same as any other anonymous-namespace member.

void StartVcpkgInstall(BuildPanelState& state, std::string target_dir, std::string triplet) {
    if (state.installing_vcpkg.load()) return;
    if (state.vcpkg_worker.joinable()) state.vcpkg_worker.join();

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

        auto* streaming = dynamic_cast<ava::platform::IProcessStream*>(&process);
        bool ok = true;

        auto append = [&](const std::string& text) {
            if (text.empty()) return;
            std::lock_guard<std::mutex> lock(state.vcpkg_mutex);
            state.vcpkg_log += text;
        };
        auto run_step = [&](const std::string& what, const std::string& cmd,
                             const std::vector<std::string>& args) -> bool {
            if (!ok) return false;
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

// Same computation the "Install vcpkg" button used to do inline (settings.build_vcpkg_root,
// falling back to auto-detecting a vcpkg checkout next to the repo root or the executable).
// Public (declared in build_panel.h) so the Command Palette's "Install vcpkg" entry can call
// it without duplicating the fallback chain -- see DrawBuildPanel's own button below, which
// now calls this too instead of repeating the ternary chain inline.
std::string ResolveVcpkgInstallTarget(const StudioSettings& settings) {
    const fs::path repo_root_for_vcpkg =
        settings.build_repo_root.empty() ? DetectRepoRoot(SelfExecutableDir()) : fs::path(settings.build_repo_root);
    const fs::path detected_vcpkg = DetectVcpkgRoot(repo_root_for_vcpkg);
    const fs::path install_target = settings.build_vcpkg_root.empty()
                                         ? (detected_vcpkg.empty()
                                                ? (repo_root_for_vcpkg.empty() ? SelfExecutableDir() / "vcpkg"
                                                                                : repo_root_for_vcpkg / "vcpkg")
                                                : detected_vcpkg)
                                         : fs::path(settings.build_vcpkg_root);
    return install_target.string();
}

std::string ResolveBuildProjectDir(const StudioSettings& settings, const std::string& explorer_root_dir) {
    return settings.build_project_dir.empty() ? explorer_root_dir : settings.build_project_dir;
}

void PollBuild(BuildPanelState& state, LogBridge& log_bridge) {
    std::lock_guard<std::mutex> lock(state.mutex);
    FlushLogToOutput(state.log, state.log_forwarded_upto, state.has_result, "[build]   ", log_bridge);
    if (state.has_result && !state.logged_to_output) {
        log_bridge.Log(state.last_success ? "[build] succeeded -> " + state.result_path : "[build] failed:");
        state.logged_to_output = true;
    }
}

void TriggerBuild(BuildPanelState& state, const StudioSettings& settings, const std::string& explorer_root_dir,
                   LogBridge& log_bridge) {
    if (state.building.load()) return;

    const fs::path project_dir(ResolveBuildProjectDir(settings, explorer_root_dir));
    const bool is_barekernel = (settings.build_target == "barekernel");

    fs::path ava_cli =
        settings.build_ava_cli_path.empty() ? DetectAvaCliPath() : fs::path(settings.build_ava_cli_path);
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
        setup_error = util::Tr("build.error_ava_cli_not_found");
    } else if (repo_root.empty() || !LooksLikeRepoRoot(repo_root)) {
        setup_error = util::Tr("build.error_repo_root_not_found");
    } else if (!fs::exists(project_dir, ec) || !fs::is_directory(project_dir, ec)) {
        setup_error = util::Tr("build.error_project_dir_missing");
    } else if (entry.empty()) {
        setup_error = util::Tr("build.error_entry_file_missing");
    } else if (is_barekernel && (settings.build_compiler_path_barekernel.empty() ||
                                  !fs::exists(settings.build_compiler_path_barekernel, ec))) {
        setup_error = util::Tr("build.error_toolchain_dir_missing");
    } else {
        fs::path vcpkg_root =
            settings.build_vcpkg_root.empty() ? DetectVcpkgRoot(repo_root) : fs::path(settings.build_vcpkg_root);
        if (!vcpkg_root.empty()) {
            auto env_platform = ava::platform::Platform::Create();
            if (env_platform) {
                env_platform->Environment().SetEnvVar("VCPKG_ROOT", vcpkg_root.string());
                env_platform->Environment().SetEnvVar("AVA_VCPKG_TRIPLET", "x64-windows-static-md");
            }
        }

        std::string out_arg = out_dir.string();
        if (out_arg.empty() || (out_arg.back() != '/' && out_arg.back() != '\\')) out_arg += "/";

        std::vector<std::string> args = {
            "build",
            "--project", project_dir.string(),
            "--entry", entry,
            "--out", out_arg,
            "--repo-root", repo_root.string(),
            "--target", is_barekernel ? "barekernel" : "desktop",
        };
        const std::string& active_compiler_path =
            is_barekernel ? settings.build_compiler_path_barekernel : settings.build_compiler_path_desktop;
        if (!active_compiler_path.empty()) {
            args.push_back("--compiler-path");
            args.push_back(active_compiler_path);
        }
        if (!is_barekernel) {
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
        }

        std::string entry_stem = fs::path(entry).stem().string();
        if (entry_stem.empty()) entry_stem = "packaged";

        const std::string expected_suffix = is_barekernel ? ".exe" : AVASTUDIO_EXE_SUFFIX;
        fs::path expected_exe = out_dir / (entry_stem + expected_suffix);

        StartBuild(state, std::move(args), ava_cli.string(), expected_exe.string());
    }

    if (!setup_error.empty()) {
        std::lock_guard<std::mutex> lock(state.mutex);
        state.log = "error: " + setup_error;
        state.log_forwarded_upto = 0;
        state.has_result = true;
        state.last_success = false;
        state.logged_to_output = false;
        log_bridge.Log("[build] error: " + setup_error);
        state.logged_to_output = true;
    }
}

// TriggerBuild/PollBuild/ResolveBuildProjectDir above get external linkage the same way
// StartVcpkgInstall/ResolveVcpkgInstallTarget already did (this point in the file is already
// past the anonymous namespace's early close above, in plain `studio::` scope) -- they're
// called directly from main.cpp (the Run menu's Build action and the frame-polled log flush)
// instead of only being reachable from inside DrawBuildPanel further down in this file.

namespace {

const char* DetectedPlatformName() {
#if defined(_WIN32)
    return "Windows";
#elif defined(__APPLE__)
    return "macOS";
#elif defined(__linux__)
    return "Linux";
#else
    return "Unknown";
#endif
}

bool DrawPathRow(const char* label, const char* hint, std::string& value, const char* browse_id,
                  BuildBrowseField field, BuildPanelResult& result) {
    ImGui::TextColored(palette::FromHex(palette::kTextMuted), "%s", label);
    ImGui::SetNextItemWidth(-90.0f);
    bool edited = ImGui::InputTextWithHint((std::string("##") + label).c_str(), hint, &value);
    bool committed = ImGui::IsItemDeactivatedAfterEdit();
    ImGui::SameLine();

    if (ImGui::Button((std::string(browse_id) + "##" + label).c_str(), ImVec2(80.0f, 0.0f))) {
        result.browse_requested = field;
    }
    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    (void)edited;
    return committed;
}

}

BuildPanelResult DrawBuildPanel(BuildPanelState& state, StudioSettings& settings,
                                 const std::string& explorer_root_dir, BuildBrowseField browsed_field,
                                 const std::string& browsed_value, LogBridge& log_bridge, bool* p_open) {
    BuildPanelResult result;

    if (browsed_field != BuildBrowseField::kNone && !browsed_value.empty()) {
        switch (browsed_field) {
            case BuildBrowseField::kProjectDir:  settings.build_project_dir  = browsed_value; break;
            case BuildBrowseField::kOutputDir:   settings.build_out_dir      = browsed_value; break;
            case BuildBrowseField::kAvaCliPath:  settings.build_ava_cli_path = browsed_value; break;
            case BuildBrowseField::kKeyFile:     settings.build_key_file     = browsed_value; break;
            case BuildBrowseField::kVcpkgRoot:   settings.build_vcpkg_root   = browsed_value; break;
            case BuildBrowseField::kCompilerPathDesktop:
                settings.build_compiler_path_desktop = browsed_value; break;
            case BuildBrowseField::kCompilerPathBarekernel:
                settings.build_compiler_path_barekernel = browsed_value; break;
            case BuildBrowseField::kEntryFile: {

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

    const std::string title = util::Tr("panel.build.title") + "###build";
    ImGui::Begin(title.c_str(), p_open);

    ImGui::TextWrapped("%s", util::Tr("build.intro").c_str());
    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    ImGui::TextColored(palette::FromHex(palette::kInfo), "%s", util::Tr("build.section_target").c_str());
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    ImGui::TextColored(palette::FromHex(palette::kTextMuted), "%s", util::Tr("build.platform_label").c_str());
    const std::string platform_value = TrFormat("build.platform_value", DetectedPlatformName());
    ImGui::Text("%s", platform_value.c_str());
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    const bool is_barekernel = (settings.build_target == "barekernel");
    int target_index = is_barekernel ? 1 : 0;
    ImGui::TextColored(palette::FromHex(palette::kTextMuted), "%s", util::Tr("build.target_label").c_str());
    ImGui::SetNextItemWidth(-1.0f);
    const std::string target_desktop_label = util::Tr("build.target_desktop");
    const std::string target_barekernel_label = util::Tr("build.target_barekernel");
    const char* target_items[] = {target_desktop_label.c_str(), target_barekernel_label.c_str()};
    if (ImGui::Combo("##BuildTarget", &target_index, target_items, 2)) {
        settings.build_target = (target_index == 1) ? "barekernel" : "desktop";
        result.settings_dirty = true;
    }

    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    // One visible row, but it auto-switches which underlying setting it
    // edits based on the Target combo above -- pick "desktop" here, type a
    // path, flip to "barekernel", type a different path: both are kept
    // separately (build_compiler_path_desktop / build_compiler_path_barekernel)
    // and whichever matches the current target is what actually gets sent to
    // ava_cli build --compiler-path below.
    if (is_barekernel) {
        if (DrawPathRow(util::Tr("build.compiler_path_label").c_str(),
                         util::Tr("build.compiler_path_hint_barekernel").c_str(),
                         settings.build_compiler_path_barekernel, util::Tr("common.browse").c_str(),
                         BuildBrowseField::kCompilerPathBarekernel, result)) {
            result.settings_dirty = true;
        }
        ImGui::TextWrapped("%s", util::Tr("build.barekernel_note").c_str());
    } else {
        if (DrawPathRow(util::Tr("build.compiler_path_label").c_str(),
                         util::Tr("build.compiler_path_hint_desktop").c_str(),
                         settings.build_compiler_path_desktop, util::Tr("common.browse").c_str(),
                         BuildBrowseField::kCompilerPathDesktop, result)) {
            result.settings_dirty = true;
        }
    }
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    ImGui::TextColored(palette::FromHex(palette::kInfo), "%s", util::Tr("build.section_project").c_str());
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    const std::string project_hint = TrFormat("build.default_path_hint", explorer_root_dir);
    if (DrawPathRow(util::Tr("build.project_folder_label").c_str(), project_hint.c_str(), settings.build_project_dir,
                     util::Tr("common.browse").c_str(), BuildBrowseField::kProjectDir, result)) {
        result.settings_dirty = true;
    }
    const fs::path project_dir(ResolveBuildProjectDir(settings, explorer_root_dir));

    ImGui::TextColored(palette::FromHex(palette::kTextMuted), "%s", util::Tr("build.entry_script_label").c_str());
    ImGui::SetNextItemWidth(-180.0f);
    bool entry_edited = ImGui::InputTextWithHint("##EntryScript", util::Tr("build.entry_script_hint").c_str(),
                                                  &settings.build_entry_file);
    if (ImGui::IsItemDeactivatedAfterEdit()) result.settings_dirty = true;
    (void)entry_edited;
    ImGui::SameLine();
    const std::string detect_label = util::Tr("build.detect_button");
    if (ImGui::Button(detect_label.c_str(), util::AutoButtonSize(detect_label.c_str(), 70.0f))) {
        std::error_code ec;
        if (fs::exists(project_dir, ec)) {
            settings.build_entry_file = DetectEntryFile(project_dir);
            result.settings_dirty = true;
        }
    }
    ImGui::SameLine();
    const std::string browse_entry_id = util::Tr("common.browse") + "##EntryScript";
    if (ImGui::Button(browse_entry_id.c_str(), ImVec2(80.0f, 0.0f))) {
        result.browse_requested = BuildBrowseField::kEntryFile;
    }
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    const std::string out_hint = TrFormat("build.default_path_hint", (project_dir / "dist").string());
    if (DrawPathRow(util::Tr("build.output_folder_label").c_str(), out_hint.c_str(), settings.build_out_dir,
                     util::Tr("common.browse").c_str(), BuildBrowseField::kOutputDir, result)) {
        result.settings_dirty = true;
    }

    if (!is_barekernel) {
    ImGui::Dummy(ImVec2(0.0f, 4.0f));
    ImGui::TextColored(palette::FromHex(palette::kInfo), "%s", util::Tr("build.section_options").c_str());
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    if (ImGui::Checkbox(util::Tr("build.obfuscate_label").c_str(), &settings.build_obfuscate)) result.settings_dirty = true;
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", util::Tr("build.obfuscate_tooltip").c_str());
    }
    if (settings.build_obfuscate) {
        ImGui::Indent();
        if (ImGui::Checkbox(util::Tr("build.obfuscate_strings_label").c_str(), &settings.build_obfuscate_strings))
            result.settings_dirty = true;
        if (ImGui::Checkbox(util::Tr("build.flatten_control_flow_label").c_str(),
                             &settings.build_flatten_control_flow))
            result.settings_dirty = true;
        ImGui::Unindent();
    }

    if (ImGui::Checkbox(util::Tr("build.zero_disk_label").c_str(), &settings.build_zero_disk)) result.settings_dirty = true;
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", util::Tr("build.zero_disk_tooltip").c_str());
    }

    if (ImGui::Checkbox(util::Tr("build.debug_unencrypted_label").c_str(), &settings.build_debug_unencrypted))
        result.settings_dirty = true;
    if (settings.build_debug_unencrypted) {
        ImGui::SameLine();
        ImGui::TextColored(palette::FromHex(palette::kWarning), "%s",
                            util::Tr("build.debug_unencrypted_warning").c_str());
    }
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    if (DrawPathRow(util::Tr("build.key_file_label").c_str(), util::Tr("build.key_file_hint").c_str(),
                     settings.build_key_file, util::Tr("common.browse").c_str(), BuildBrowseField::kKeyFile, result)) {
        result.settings_dirty = true;
    }
    }

    ImGui::Dummy(ImVec2(0.0f, 4.0f));
    if (ImGui::CollapsingHeader(util::Tr("build.section_advanced").c_str())) {
        ImGui::Indent();
        // Repo Root used to be a manual override here, but DetectRepoRoot(SelfExecutableDir())
        // already finds it reliably (it just walks up looking for CMakeLists.txt +
        // runtime/avapack/CMakeLists.txt) -- exposing a field for something that's never
        // actually needed by hand was clutter, so it's gone from the UI. TriggerBuild/
        // StartVcpkgInstall still fall back to settings.build_repo_root if it's non-empty (old
        // settings.json from before this still work), they just auto-detect otherwise.
        if (DrawPathRow(util::Tr("build.ava_cli_path_label").c_str(), util::Tr("build.ava_cli_path_hint").c_str(),
                         settings.build_ava_cli_path, util::Tr("common.browse").c_str(),
                         BuildBrowseField::kAvaCliPath, result)) {
            result.settings_dirty = true;
        }
        ImGui::SameLine();
        const std::string auto_detect_avacli_id = util::Tr("build.auto_detect_button") + "##AvaCli";
        if (ImGui::Button(auto_detect_avacli_id.c_str())) {
            fs::path detected = DetectAvaCliPath();
            if (!detected.empty()) {
                settings.build_ava_cli_path = detected.string();
                result.settings_dirty = true;
            }
        }

        const fs::path repo_root_for_vcpkg =
            settings.build_repo_root.empty() ? DetectRepoRoot(SelfExecutableDir()) : fs::path(settings.build_repo_root);
        const fs::path detected_vcpkg = DetectVcpkgRoot(repo_root_for_vcpkg);
        const std::string vcpkg_hint =
            detected_vcpkg.empty() ? util::Tr("build.vcpkg_not_found_hint")
                                    : TrFormat("build.default_path_hint", detected_vcpkg.string());
        if (DrawPathRow(util::Tr("build.vcpkg_root_label").c_str(), vcpkg_hint.c_str(), settings.build_vcpkg_root,
                         util::Tr("common.browse").c_str(), BuildBrowseField::kVcpkgRoot, result)) {
            result.settings_dirty = true;
        }

        const bool vcpkg_installing = state.installing_vcpkg.load();
        ImGui::BeginDisabled(vcpkg_installing || state.building.load());
        if (ImGui::Button(vcpkg_installing ? util::Tr("build.installing_vcpkg_button").c_str()
                                            : util::Tr("build.install_vcpkg_button").c_str())) {
            StartVcpkgInstall(state, ResolveVcpkgInstallTarget(settings), "x64-windows-static-md");
        }
        ImGui::EndDisabled();
        if (vcpkg_installing) {
            ImGui::SameLine();
            ImGui::TextColored(palette::FromHex(palette::kTextMuted), "%s",
                                util::Tr("build.vcpkg_installing_status").c_str());
        }
        {
            std::lock_guard<std::mutex> vcpkg_lock(state.vcpkg_mutex);

            // This panel is config-only (see comment below) -- FlushLogToOutput forwards the
            // vcpkg install log to the Output panel, which is the ONLY place it's shown. No
            // "vcpkg ready"/"vcpkg failed" text is drawn here anymore; that was a second,
            // redundant echo of the exact one-line result Output already gets right below via
            // log_bridge.Log(...). Auto-filling build_vcpkg_root on success is real config
            // behavior (not a log), so that part stays.
            FlushLogToOutput(state.vcpkg_log, state.vcpkg_log_forwarded_upto, state.vcpkg_has_result,
                              "[vcpkg]   ", log_bridge);
            if (state.vcpkg_has_result) {
                if (state.vcpkg_last_success && settings.build_vcpkg_root.empty()) {
                    settings.build_vcpkg_root = state.vcpkg_installed_dir;
                    result.settings_dirty = true;
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

    // This panel only configures build paths/settings -- it doesn't show build progress,
    // results, or any other log-like text. Starting a build (StartBuild, args resolution,
    // setup-error checks) lives in TriggerBuild above, reachable from Run > Build / Ctrl+B /
    // the Command Palette (main.cpp), not a button here. Everything about what a build actually
    // did -- its full log, "succeeded -> path", "failed" -- goes ONLY to the Output panel
    // (PollBuild, polled every frame regardless of whether this panel is open) so there's a
    // single place to look, instead of this panel echoing a second copy of the same status.
    ImGui::End();
    return result;
}

}
