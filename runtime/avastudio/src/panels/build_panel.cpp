#include "panels/build_panel.h"

#include <cfloat>
#include <chrono>
#include <filesystem>
#include <vector>

#include "imgui.h"
#include "imgui_stdlib.h"
#include "palette.h"
#include "platform/Platform.h"
#include "platform/interfaces/IProcessStream.h"
#include "project/output_type_labels.h"
#include "util/ava_cli_locator.h"
#include "util/host_platform.h"
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

// Deriva una etapa legible + una fraccion (0..1, solo para la barra) a
// partir de los marcadores que `ava_cli build` ya imprime por stdout
// (ver build_command.cpp: "build: configuring", "build: compiling",
// "build: usando herramientas prebuilt", "signing", "build: done ->",
// y el equivalente en español para el target barekernel). No es una
// medicion real del progreso (ava_cli no reporta porcentaje), pero le
// da al usuario una nocion concreta de en que paso esta el build en
// vez de solo un spinner indefinido -- se busca el ULTIMO marcador que
// aparece en el log acumulado hasta ahora, ya que los pasos se
// imprimen en orden y no se repiten.
struct BuildStageInfo {
    std::string label_key;
    float fraction;
};

BuildStageInfo DeriveBuildStage(const std::string& log) {
    struct Marker {
        const char* needle;
        const char* label_key;
        float fraction;
    };
    // Orden = orden en que ava_cli los imprime; se evalua de atras para
    // adelante asi el marcador mas reciente (el que realmente describe
    // donde esta el build ahora) gana sobre uno mas viejo que tambien
    // aparezca en el log.
    static const Marker kMarkers[] = {
        {"build: done ->", "build.progress_stage_done", 1.0f},
        {"listo ->", "build.progress_stage_done", 1.0f},
        {"signing", "build.progress_stage_signing", 0.85f},
        {"escribiendo AppHeader", "build.progress_stage_packaging", 0.85f},
        {"[info] copied", "build.progress_stage_packaging", 0.8f},
        {"usando herramientas prebuilt", "build.progress_stage_packaging", 0.6f},
        {"build: compiling", "build.progress_stage_compiling", 0.55f},
        {"compilando (cruzado", "build.progress_stage_compiling", 0.55f},
        {"compilando herramientas de host", "build.progress_stage_compiling", 0.45f},
        {"linkeando", "build.progress_stage_compiling", 0.7f},
        {"generando embedded_avb.cpp", "build.progress_stage_compiling", 0.4f},
        {"build: configuring", "build.progress_stage_configuring", 0.15f},
        {"configurando build cruzado", "build.progress_stage_configuring", 0.15f},
        {"compilando" /* barekernel entry, catch-all */, "build.progress_stage_compiling", 0.3f},
    };
    for (size_t i = sizeof(kMarkers) / sizeof(kMarkers[0]); i-- > 0;) {
        if (log.find(kMarkers[i].needle) != std::string::npos) {
            return {kMarkers[i].label_key, kMarkers[i].fraction};
        }
    }
    return {"build.progress_stage_starting", 0.05f};
}

std::string LastNonEmptyLine(const std::string& log) {
    size_t end = log.find_last_not_of("\n\r");
    if (end == std::string::npos) return "";
    size_t start = log.find_last_of('\n', end);
    return log.substr(start == std::string::npos ? 0 : start + 1, end - (start == std::string::npos ? 0 : start));
}

std::string FormatSeconds(double seconds) {
    int total = static_cast<int>(seconds);
    if (total < 60) return std::to_string(total) + "s";
    return std::to_string(total / 60) + "m " + std::to_string(total % 60) + "s";
}

std::string TrFormat(const std::string& key, const std::string& arg) { return util::TrFormat(key, {arg}); }

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
    state.show_result_dialog = false;
    state.building = true;
    state.build_started_at = std::chrono::steady_clock::now();

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

}  // namespace

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

std::string ResolveVcpkgInstallTarget(const AvaProjUserFile& user) {
    const fs::path repo_root_for_vcpkg =
        user.repo_root.empty() ? DetectRepoRoot(SelfExecutableDir()) : fs::path(user.repo_root);
    const fs::path detected_vcpkg = DetectVcpkgRoot(repo_root_for_vcpkg);
    const fs::path install_target = user.vcpkg_root.empty()
                                         ? (detected_vcpkg.empty()
                                                ? (repo_root_for_vcpkg.empty() ? SelfExecutableDir() / "vcpkg"
                                                                                : repo_root_for_vcpkg / "vcpkg")
                                                : detected_vcpkg)
                                         : fs::path(user.vcpkg_root);
    return install_target.string();
}

std::string NormalizeEntryFilePath(const std::string& project_dir, const std::string& picked_path) {
    std::error_code ec;
    fs::path rel = fs::relative(fs::path(picked_path), fs::path(project_dir), ec);
    return (!ec && !rel.empty() && rel.native().rfind(fs::path("..").native(), 0) != 0) ? rel.generic_string()
                                                                                         : picked_path;
}

void PollBuild(BuildPanelState& state, LogBridge& log_bridge) {
    std::lock_guard<std::mutex> lock(state.mutex);
    FlushLogToOutput(state.log, state.log_forwarded_upto, state.has_result, "[build]   ", log_bridge);
    if (state.has_result && !state.logged_to_output) {
        log_bridge.Log(state.last_success ? "[build] succeeded -> " + state.result_path : "[build] failed:");
        state.logged_to_output = true;
        state.dialog_success = state.last_success;
        state.dialog_result_path = state.result_path;
        state.show_result_dialog = true;
    }
}

TriggerBuildOutcome TriggerBuild(BuildPanelState& state, const AvaProjFile& proj, const AvaProjUserFile& user,
                                  const std::string& explorer_root_dir, LogBridge& log_bridge,
                                  bool project_ambiguous, const std::vector<std::string>& avaproj_candidates) {
    TriggerBuildOutcome outcome;
    outcome.project_dir = explorer_root_dir;
    if (state.building.load()) return outcome;

    // Fix: si la carpeta abierta tiene mas de un .avaproj, no hay forma de
    // saber cual de los proyectos quiere compilar el usuario -- antes esto
    // se resolvia mezclando archivos de cualquiera de ellos (ver
    // LoadProjectConfig / DetectEntryFile). Ahora se corta aca, listando
    // los candidatos, para que el usuario abra la subcarpeta especifica
    // (Open Folder) del proyecto que quiere buildear.
    if (project_ambiguous) {
        std::string list;
        for (const std::string& c : avaproj_candidates) {
            if (!list.empty()) list += ", ";
            list += c;
        }
        std::lock_guard<std::mutex> lock(state.mutex);
        state.log = "error: se encontraron varios .avaproj en " + explorer_root_dir +
                     " (" + list +
                     ") -- Ava Studio no puede saber cual proyecto queres compilar. "
                     "Abri la subcarpeta del proyecto especifico (File > Open Folder) "
                     "en vez de la carpeta que los contiene a todos.\n";
        state.log_forwarded_upto = 0;
        state.has_result = true;
        state.last_success = false;
        state.logged_to_output = false;
        log_bridge.Log("[build] error: multiples .avaproj encontrados -- ver detalle en Build.");
        state.logged_to_output = true;
        return outcome;
    }

    const fs::path project_dir(outcome.project_dir);
    const bool is_barekernel = (proj.target == AvaProjTarget::kBareKernel);
    const bool is_library = (proj.output_type == AvaProjOutputType::kLibrary);

    fs::path ava_cli = user.ava_cli_path.empty() ? DetectAvaCliPath() : fs::path(user.ava_cli_path);
    fs::path repo_root = user.repo_root.empty()
                              ? [&]() {
                                    fs::path d = DetectRepoRoot(SelfExecutableDir());
                                    return d.empty() ? DetectRepoRoot(project_dir) : d;
                                }()
                              : fs::path(user.repo_root);
    std::string entry = proj.entry_file.empty() ? DetectEntryFile(project_dir) : proj.entry_file;
    fs::path out_dir_raw = proj.out_dir.empty() ? fs::path("bin") : fs::path(proj.out_dir);
    fs::path out_dir = out_dir_raw.is_absolute() ? out_dir_raw : (project_dir / out_dir_raw);

    std::error_code ec;
    std::string setup_error;
    if (is_library && is_barekernel) {
        setup_error = util::Tr("build.error_library_barekernel_not_supported");
    } else if (ava_cli.empty() || !fs::exists(ava_cli, ec)) {
        setup_error = util::Tr("build.error_ava_cli_not_found");
    } else if (repo_root.empty() || !LooksLikeRepoRoot(repo_root)) {
        setup_error = util::Tr("build.error_repo_root_not_found");
    } else if (!fs::exists(project_dir, ec) || !fs::is_directory(project_dir, ec)) {
        setup_error = util::Tr("build.error_project_dir_missing");
    } else if (entry.empty()) {
        setup_error = util::Tr("build.error_entry_file_missing");
    } else if (!fs::exists(project_dir / entry, ec)) {
        outcome.entry_file_missing = true;
        setup_error = TrFormat("build.error_entry_file_not_found", (project_dir / entry).string());
    } else if (is_barekernel && (user.compiler_path_barekernel.empty() ||
                                  !fs::exists(user.compiler_path_barekernel, ec))) {
        setup_error = util::Tr("build.error_toolchain_dir_missing");
    } else {
        fs::path vcpkg_root = user.vcpkg_root.empty() ? DetectVcpkgRoot(repo_root) : fs::path(user.vcpkg_root);
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
        if (!is_barekernel) {
            args.push_back("--output-kind");
            args.push_back(is_library ? "library" : "exe");
        }
        const std::string& active_compiler_path =
            is_barekernel ? user.compiler_path_barekernel : user.compiler_path_desktop;
        if (!active_compiler_path.empty()) {
            args.push_back("--compiler-path");
            args.push_back(active_compiler_path);
        }
        if (is_barekernel) {
            if (user.force_so) args.push_back("--force-so");
            if (user.force_runtime) args.push_back("--force-runtime");
        }
        // Fix: las <Reference Include="..."> del .avaproj (AvaProjReference,
        // ver project/avaproj_file.h) se guardaban y leian del XML pero
        // nunca llegaban a la build real -- cualquier import a una carpeta
        // fuera de project_dir (p.ej. una carpeta compartida hermana del
        // proyecto dentro del workspace) nunca se empaquetaba, y el .exe
        // final fallaba en runtime con "could not find module: X". Cada
        // referencia se resuelve relativa a project_dir (si no es absoluta)
        // y se pasa como --extra-modules-dir; las que no existan se
        // ignoran (avapack_gen ya avisa si termina faltando un modulo).
        for (const AvaProjReference& reference : proj.references) {
            if (reference.include.empty()) continue;
            fs::path ref_path(reference.include);
            fs::path ref_abs = ref_path.is_absolute() ? ref_path : (project_dir / ref_path);
            std::error_code ref_ec;
            if (!fs::exists(ref_abs, ref_ec) || !fs::is_directory(ref_abs, ref_ec)) continue;
            args.push_back("--extra-modules-dir");
            args.push_back(ref_abs.string());
        }

        if (!is_barekernel) {
            if (!user.key_file.empty()) {
                args.push_back("--key-file");
                args.push_back(user.key_file);
            }
            if (proj.obfuscate) {
                args.push_back("--obfuscate");
                if (proj.obfuscate_strings) args.push_back("--obfuscate-strings");
                if (proj.flatten_control_flow) args.push_back("--flatten-control-flow");
            }
            if (proj.zero_disk && !is_library) args.push_back("--zero-disk");
            if (proj.debug_unencrypted) args.push_back("--debug");
        }

        std::string entry_stem = fs::path(entry).stem().string();
        if (entry_stem.empty()) entry_stem = "packaged";

        std::string expected_suffix;
        if (is_barekernel) {
            expected_suffix = ".exe";
        } else if (is_library) {
            expected_suffix = util::HostLibraryExtension(util::DetectedHostPlatform());
        } else {
            expected_suffix = AVASTUDIO_EXE_SUFFIX;
        }
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

    return outcome;
}

namespace {

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

}  // namespace

BuildPanelResult DrawBuildPanel(BuildPanelState& state, AvaProjFile& proj, AvaProjUserFile& user,
                                 const std::string& explorer_root_dir, BuildBrowseField browsed_field,
                                 const std::string& browsed_value, LogBridge& log_bridge, bool* p_open) {
    BuildPanelResult result;

    if (browsed_field != BuildBrowseField::kNone && !browsed_value.empty()) {
        switch (browsed_field) {
            case BuildBrowseField::kAvaCliPath:  user.ava_cli_path           = browsed_value; break;
            case BuildBrowseField::kKeyFile:     user.key_file               = browsed_value; break;
            case BuildBrowseField::kVcpkgRoot:   user.vcpkg_root             = browsed_value; break;
            case BuildBrowseField::kCompilerPathDesktop:
                user.compiler_path_desktop = browsed_value; break;
            case BuildBrowseField::kCompilerPathBarekernel:
                user.compiler_path_barekernel = browsed_value; break;
            case BuildBrowseField::kNone: break;
        }
        result.dirty = true;
    }

    const std::string title = util::Tr("panel.build.title") + "###build";
    ImGui::Begin(title.c_str(), p_open);

    ImGui::TextWrapped("%s", util::Tr("build.intro").c_str());
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    const std::string open_properties_label = util::Tr("build.open_project_properties_button");
    if (ImGui::Button(open_properties_label.c_str())) {
        result.open_project_properties = true;
    }
    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    if (state.building.load()) {
        std::string log_snapshot;
        {
            std::lock_guard<std::mutex> lock(state.mutex);
            log_snapshot = state.log;
        }
        const BuildStageInfo stage = DeriveBuildStage(log_snapshot);
        const std::string last_line = LastNonEmptyLine(log_snapshot);
        const double elapsed_s = std::chrono::duration<double>(
                                      std::chrono::steady_clock::now() - state.build_started_at)
                                      .count();

        ImGui::Separator();

        // Spinner de 8 frames Braille, gira con el reloj de ImGui -- no
        // depende de ningun evento nuevo del proceso hijo, asi que se ve
        // vivo aunque `ava_cli` este en silencio un rato (ej. durante el
        // configure de CMake).
        static const char* kSpinnerFrames[] = {"\xE2\xA0\x8B", "\xE2\xA0\x99", "\xE2\xA0\xB9", "\xE2\xA0\xB8",
                                                 "\xE2\xA0\xBC", "\xE2\xA0\xB4", "\xE2\xA0\xA6", "\xE2\xA0\xA7"};
        const int frame = static_cast<int>(ImGui::GetTime() * 8.0) %
                           static_cast<int>(sizeof(kSpinnerFrames) / sizeof(kSpinnerFrames[0]));
        ImGui::TextColored(palette::FromHex(palette::kInfo), "%s", kSpinnerFrames[frame]);
        ImGui::SameLine();
        ImGui::Text("%s", util::Tr(stage.label_key).c_str());
        ImGui::SameLine();
        ImGui::TextColored(palette::FromHex(palette::kTextMuted), "%s",
                            util::TrFormat("build.progress_elapsed", {FormatSeconds(elapsed_s)}).c_str());

        ImGui::ProgressBar(stage.fraction, ImVec2(-FLT_MIN, 0.0f));

        if (!last_line.empty()) {
            ImGui::TextColored(palette::FromHex(palette::kTextMuted), "%s", last_line.c_str());
        }

        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
    }

    ImGui::TextColored(palette::FromHex(palette::kInfo), "%s", util::Tr("build.section_target").c_str());
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    const util::HostPlatform host_platform = util::DetectedHostPlatform();

    ImGui::TextColored(palette::FromHex(palette::kTextMuted), "%s", util::Tr("build.platform_label").c_str());
    const std::string platform_value = TrFormat("build.platform_value", util::HostPlatformName(host_platform));
    ImGui::Text("%s", platform_value.c_str());
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    const bool is_barekernel = (proj.target == AvaProjTarget::kBareKernel);
    const bool is_library = (proj.output_type == AvaProjOutputType::kLibrary);
    const std::string target_desktop_label = util::Tr("build.target_desktop");
    const std::string target_barekernel_label = util::Tr("build.target_barekernel");
    const std::string output_exe_label = DescribeOutputType(host_platform, is_barekernel, AvaProjOutputType::kExe);
    const std::string output_library_label =
        DescribeOutputType(host_platform, is_barekernel, AvaProjOutputType::kLibrary);
    ImGui::TextColored(palette::FromHex(palette::kTextMuted), "%s", util::Tr("build.target_label").c_str());
    ImGui::Text("%s", (is_barekernel ? target_barekernel_label : target_desktop_label).c_str());
    ImGui::TextColored(palette::FromHex(palette::kTextMuted), "%s", util::Tr("build.output_type_label").c_str());
    ImGui::Text("%s", (is_library ? output_library_label : output_exe_label).c_str());
    ImGui::TextDisabled("%s", util::Tr("build.target_readonly_note").c_str());

    if (is_library && is_barekernel) {
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        ImGui::TextColored(palette::FromHex(palette::kWarning), "%s",
                            util::Tr("build.error_library_barekernel_not_supported").c_str());
    }

    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    ImGui::TextColored(palette::FromHex(palette::kInfo), "%s", util::Tr("build.section_machine_paths").c_str());
    ImGui::Separator();
    ImGui::TextDisabled("%s", util::Tr("build.section_machine_paths_note").c_str());
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    if (is_barekernel) {
        if (DrawPathRow(util::Tr("build.compiler_path_label").c_str(),
                         util::Tr("build.compiler_path_hint_barekernel").c_str(),
                         user.compiler_path_barekernel, util::Tr("common.browse").c_str(),
                         BuildBrowseField::kCompilerPathBarekernel, result)) {
            result.dirty = true;
        }
        ImGui::TextWrapped("%s", util::Tr("build.barekernel_note").c_str());
        ImGui::Dummy(ImVec2(0.0f, 6.0f));

        if (ImGui::Checkbox(util::Tr("build.force_so_label").c_str(), &user.force_so))
            result.dirty = true;
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", util::Tr("build.force_so_tooltip").c_str());
        }
        if (ImGui::Checkbox(util::Tr("build.force_runtime_label").c_str(), &user.force_runtime))
            result.dirty = true;
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", util::Tr("build.force_runtime_tooltip").c_str());
        }
        if (user.force_so || user.force_runtime) {
            ImGui::TextColored(palette::FromHex(palette::kWarning), "%s",
                                util::Tr("build.force_rebuild_warning").c_str());
        }
    } else {
        if (DrawPathRow(util::Tr("build.compiler_path_label").c_str(),
                         util::Tr("build.compiler_path_hint_desktop").c_str(),
                         user.compiler_path_desktop, util::Tr("common.browse").c_str(),
                         BuildBrowseField::kCompilerPathDesktop, result)) {
            result.dirty = true;
        }
        if (DrawPathRow(util::Tr("build.key_file_label").c_str(), util::Tr("build.key_file_hint").c_str(),
                         user.key_file, util::Tr("common.browse").c_str(), BuildBrowseField::kKeyFile, result)) {
            result.dirty = true;
        }
    }

    ImGui::Dummy(ImVec2(0.0f, 4.0f));
    if (ImGui::CollapsingHeader(util::Tr("build.section_advanced").c_str())) {
        ImGui::Indent();
        if (DrawPathRow(util::Tr("build.ava_cli_path_label").c_str(), util::Tr("build.ava_cli_path_hint").c_str(),
                         user.ava_cli_path, util::Tr("common.browse").c_str(),
                         BuildBrowseField::kAvaCliPath, result)) {
            result.dirty = true;
        }
        ImGui::SameLine();
        const std::string auto_detect_avacli_id = util::Tr("build.auto_detect_button") + "##AvaCli";
        if (ImGui::Button(auto_detect_avacli_id.c_str())) {
            fs::path detected = DetectAvaCliPath();
            if (!detected.empty()) {
                user.ava_cli_path = detected.string();
                result.dirty = true;
            }
        }

        const fs::path repo_root_for_vcpkg =
            user.repo_root.empty() ? DetectRepoRoot(SelfExecutableDir()) : fs::path(user.repo_root);
        const fs::path detected_vcpkg = DetectVcpkgRoot(repo_root_for_vcpkg);
        const std::string vcpkg_hint =
            detected_vcpkg.empty() ? util::Tr("build.vcpkg_not_found_hint")
                                    : TrFormat("build.default_path_hint", detected_vcpkg.string());
        if (DrawPathRow(util::Tr("build.vcpkg_root_label").c_str(), vcpkg_hint.c_str(), user.vcpkg_root,
                         util::Tr("common.browse").c_str(), BuildBrowseField::kVcpkgRoot, result)) {
            result.dirty = true;
        }

        const bool vcpkg_installing = state.installing_vcpkg.load();
        ImGui::BeginDisabled(vcpkg_installing || state.building.load());
        if (ImGui::Button(vcpkg_installing ? util::Tr("build.installing_vcpkg_button").c_str()
                                            : util::Tr("build.install_vcpkg_button").c_str())) {
            StartVcpkgInstall(state, ResolveVcpkgInstallTarget(user), "x64-windows-static-md");
        }
        ImGui::EndDisabled();
        if (vcpkg_installing) {
            ImGui::SameLine();
            ImGui::TextColored(palette::FromHex(palette::kTextMuted), "%s",
                                util::Tr("build.vcpkg_installing_status").c_str());
        }
        {
            std::lock_guard<std::mutex> vcpkg_lock(state.vcpkg_mutex);
            FlushLogToOutput(state.vcpkg_log, state.vcpkg_log_forwarded_upto, state.vcpkg_has_result,
                              "[vcpkg]   ", log_bridge);
            if (state.vcpkg_has_result) {
                if (state.vcpkg_last_success && user.vcpkg_root.empty()) {
                    user.vcpkg_root = state.vcpkg_installed_dir;
                    result.dirty = true;
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

    (void)explorer_root_dir;
    ImGui::End();
    return result;
}

}

