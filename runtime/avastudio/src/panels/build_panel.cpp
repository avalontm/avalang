#include "panels/build_panel.h"

#include <cfloat>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
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

#include "diagnostics/crash_handler.h"

#if defined(_WIN32)
    #define AVASTUDIO_EXE_SUFFIX ".exe"
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

std::string ExtractHexField(const std::string& text, const std::string& label) {
    auto pos = text.find(label);
    if (pos == std::string::npos) return {};
    pos += label.size();
    auto end = text.find_first_of("\r\n", pos);
    if (end == std::string::npos) end = text.size();
    return text.substr(pos, end - pos);
}

std::string FindNewestCrashDumpSince(std::chrono::system_clock::time_point since) {
    std::error_code ec;
    fs::path dir(ava::diag::CrashDumpDirectory());
    if (!fs::exists(dir, ec)) return {};

    std::string newest_path;
    fs::file_time_type newest_time{};
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (ec) break;
        if (entry.path().extension() != ".dmp") continue;
        auto ftime = fs::last_write_time(entry, ec);
        if (ec) continue;
        auto sys_time = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
        if (sys_time < since) continue;
        if (newest_path.empty() || ftime > newest_time) {
            newest_time = ftime;
            newest_path = entry.path().string();
        }
    }
    return newest_path;
}

// Nombre simbolico de un codigo de salida NTSTATUS (lo que Windows devuelve
// como exit code cuando un proceso muere por una excepcion). nullptr si no
// es uno conocido.
const char* NtStatusName(unsigned int code) {
    switch (code) {
        case 0xC0000005u: return "ACCESS_VIOLATION";
        case 0xC00000FDu: return "STACK_OVERFLOW";
        case 0xC0000094u: return "INT_DIVIDE_BY_ZERO";
        case 0xC000001Du: return "ILLEGAL_INSTRUCTION";
        case 0xC0000374u: return "HEAP_CORRUPTION";
        case 0xC0000409u: return "STACK_BUFFER_OVERRUN / fail-fast";
        case 0xC0000135u: return "DLL_NOT_FOUND (falta una DLL)";
        case 0xC0000142u: return "DLL_INIT_FAILED (fallo la inicializacion de una DLL)";
        case 0xC0000139u: return "ENTRYPOINT_NOT_FOUND (DLL de version distinta)";
        case 0xC000007Bu: return "INVALID_IMAGE_FORMAT";
        case 0xC06D007Eu: return "DELAYLOAD: modulo no encontrado";
        case 0xC06D007Fu: return "DELAYLOAD: funcion no encontrada";
        case 0xC000013Au: return "CONTROL_C_EXIT";
        case 0x80000003u: return "BREAKPOINT";
        default: return nullptr;
    }
}

// Un exit code "de excepcion" (NTSTATUS de error) -- a diferencia de un
// exit(1) normal de la app, que no significa que el proceso haya crasheado.
bool LooksLikeNativeCrash(int exit_code) {
    const unsigned int code = static_cast<unsigned int>(exit_code);
    return (code & 0xF0000000u) == 0xC0000000u || code == 0x80000003u;
}

std::string FormatExitCode(int exit_code) {
    const unsigned int code = static_cast<unsigned int>(exit_code);
    char hex[16];
    std::snprintf(hex, sizeof(hex), "0x%08X", code);
    std::string out = hex;
    if (const char* name = NtStatusName(code)) {
        out += " = ";
        out += name;
    }
    return out;
}

std::string DescribeFileAge(const fs::path& path) {
    std::error_code ec;
    const auto mtime = fs::last_write_time(path, ec);
    if (ec) return "fecha desconocida";
    const long long minutes = std::chrono::duration_cast<std::chrono::minutes>(
                                   fs::file_time_type::clock::now() - mtime)
                                   .count();
    if (minutes < 1) return "hace menos de 1 minuto";
    if (minutes < 120) return "hace " + std::to_string(minutes) + " min";
    if (minutes < 60 * 48) return "hace " + std::to_string(minutes / 60) + " h";
    return "hace " + std::to_string(minutes / (60 * 24)) + " dias";
}

// Texto para el panel de Logs cuando el .exe murio con un codigo de crash y
// NO llego ninguna linea @@AVA_ERROR@@ (es decir, el handler de crash del
// .exe no llego a ejecutarse). Cada linea termina en '\n' para que
// FlushLogToOutput la publique por separado.
std::string DescribeUnexpectedExit(const std::string& exe_path, int exit_code) {
    std::error_code ec;
    std::string out = "error: el proceso '" + exe_path + "' termino inesperadamente (codigo " +
                       FormatExitCode(exit_code) + ")\n";
    if (!LooksLikeNativeCrash(exit_code)) return out;

    const fs::path crash_dir(ava::diag::CrashDumpDirectory());
    const bool crash_dir_exists = fs::exists(crash_dir, ec);
    out += "  No llego ninguna linea @@AVA_ERROR@@ ni se genero un .dmp/.txt nuevo: el handler de crash del "
           ".exe no llego a ejecutarse.\n";
    out += "  Carpeta de crashes: " + crash_dir.string() +
           (crash_dir_exists ? " (existe, sin archivos nuevos)"
                              : " (no existe: ningun .exe de Avalang ha escrito nunca un informe aqui)") +
           "\n";
    out += "  Binario: " + exe_path + " (modificado " + DescribeFileAge(exe_path) + ")\n";
    out += "  Causas probables: (1) el .exe es de una version anterior sin handler, o desincronizado con sus "
           "DLL (avalang.dll / avalang_ui.dll / avalang_ui_win.dll en su misma carpeta); (2) el crash ocurrio "
           "antes de main(), durante la inicializacion de una DLL.\n";
    out += "  Que probar: borrar la carpeta build_avastudio_run\\dist y volver a ejecutar (fuerza la "
           "recompilacion), o ejecutar el .exe desde una consola o bajo el depurador de Visual Studio.\n";
    return out;
}

bool IsRuntimeSourceFile(const fs::path& path) {
    const std::string ext = path.extension().string();
    return ext == ".cpp" || ext == ".cc" || ext == ".c" || ext == ".h" || ext == ".hpp" || ext == ".g4" ||
           ext == ".cmake" || path.filename() == "CMakeLists.txt";
}

// Fecha del fuente mas reciente de los que se compila avanative (runtime
// nativo + UI + host + comun + CMake). `out_newest_file` recibe cual es.
fs::file_time_type NewestRuntimeSourceTime(const fs::path& repo_root, fs::path& out_newest_file) {
    fs::file_time_type newest = (fs::file_time_type::min)();
    std::error_code ec;

    auto consider = [&](const fs::path& file, const fs::file_time_type& when) {
        if (when > newest) {
            newest = when;
            out_newest_file = file;
        }
    };

    const fs::path root_cmake = repo_root / "CMakeLists.txt";
    const auto root_time = fs::last_write_time(root_cmake, ec);
    if (!ec) consider(root_cmake, root_time);
    ec.clear();

    static const char* const kSourceDirs[] = {"runtime/avalang", "runtime/avaui", "runtime/avahost",
                                               "runtime/common", "cmake"};
    for (const char* rel : kSourceDirs) {
        const fs::path dir = repo_root / rel;
        if (!fs::is_directory(dir, ec)) {
            ec.clear();
            continue;
        }
        fs::recursive_directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec);
        fs::recursive_directory_iterator end;
        for (; !ec && it != end; it.increment(ec)) {
            const fs::directory_entry& entry = *it;
            std::error_code entry_ec;
            if (!entry.is_regular_file(entry_ec) || entry_ec) continue;
            if (!IsRuntimeSourceFile(entry.path())) continue;
            const auto when = entry.last_write_time(entry_ec);
            if (entry_ec) continue;
            consider(entry.path(), when);
        }
        ec.clear();
    }
    return newest;
}

// El binario de Run (avanative) esta desactualizado si no hay sello de "se
// compilo bien" o si algun fuente del runtime es mas nuevo que ese sello.
bool RunBuildIsStale(const fs::path& stamp_path, const fs::path& repo_root, std::string& out_reason) {
    std::error_code ec;
    if (!fs::exists(stamp_path, ec)) {
        out_reason = "no hay constancia de que se compilara desde los fuentes actuales";
        return true;
    }
    const auto stamp_time = fs::last_write_time(stamp_path, ec);
    if (ec) {
        out_reason = "no se pudo leer la fecha del ultimo build";
        return true;
    }
    fs::path newest_file;
    const auto newest = NewestRuntimeSourceTime(repo_root, newest_file);
    if (newest > stamp_time) {
        out_reason = newest_file.generic_string() + " es mas nuevo que el ultimo build";
        return true;
    }
    return false;
}

void WriteRunStamp(const std::string& path) {
    std::error_code ec;
    fs::create_directories(fs::path(path).parent_path(), ec);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (out) out << "avanative run build ok\n";
}

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

}  // namespace

void StartMultiStepBuild(BuildPanelState& state, std::vector<BuildStep> steps, std::string expected_result_path) {
    if (state.building.load()) return;
    if (state.worker.joinable()) state.worker.join();

    {
        std::lock_guard<std::mutex> lock(state.mutex);
        state.log.clear();
        state.log_forwarded_upto = 0;
        state.has_result = false;
        state.crash_dump_path.clear();
        state.crash_code.clear();
        state.crash_address.clear();
        state.failure_summary.clear();
        state.running_process.reset();
        state.last_output_at = std::chrono::steady_clock::now();
    }
    state.logged_to_output = false;
    state.show_result_dialog = false;
    state.pending_run_stamp.clear();
    state.building = true;
    state.build_started_at = std::chrono::steady_clock::now();

    state.worker = std::thread([&state, steps = std::move(steps),
                                 expected_result_path = std::move(expected_result_path)]() {
        auto platform = ava::platform::Platform::Create();
        ava::platform::IProcess& process = platform->Process();
        auto* streaming = dynamic_cast<ava::platform::IProcessStream*>(&process);

        bool all_succeeded = true;

        for (const BuildStep& step : steps) {
            if (!step.step_label.empty()) {
                std::lock_guard<std::mutex> lock(state.mutex);
                state.log += "$ " + step.step_label + "\n";
            }

            bool launched = false;
            int exit_code = -1;
            bool saw_structured_error = false;
            bool saw_native_crash = false;
            auto step_started_at = std::chrono::system_clock::now();

            auto note_chunk = [&saw_structured_error, &saw_native_crash](const std::string& chunk) {
                if (chunk.find("@@AVA_ERROR@@") != std::string::npos) saw_structured_error = true;
                if (chunk.find("\"kind\":\"native_crash\"") != std::string::npos) saw_native_crash = true;
            };

            if (streaming) {
                const bool is_run_step = step.step_label.empty();
                launched = streaming->ExecuteStreaming(
                    step.exe_path, step.args,
                    [&state, &note_chunk](const std::string& chunk) {
                        note_chunk(chunk);
                        std::lock_guard<std::mutex> lock(state.mutex);
                        state.log += chunk;
                        state.last_output_at = std::chrono::steady_clock::now();
                    },
                    exit_code,
                    [&state, is_run_step](avastd::shared_ptr<ava::platform::IProcessStream::IStdinWriter> writer) {
                        if (!is_run_step) return;
                        std::lock_guard<std::mutex> lock(state.mutex);
                        state.running_process = std::move(writer);
                    });
                if (is_run_step) {
                    std::lock_guard<std::mutex> lock(state.mutex);
                    state.running_process.reset();
                }
            } else {
                ava::platform::ProcessResult result;
                launched = process.Execute(step.exe_path, step.args, result);
                if (launched) {
                    note_chunk(result.stdout_output);
                    note_chunk(result.stderr_output);
                    std::lock_guard<std::mutex> lock(state.mutex);
                    state.log += result.stdout_output;
                    if (!result.stderr_output.empty()) {
                        if (!state.log.empty()) state.log += "\n";
                        state.log += result.stderr_output;
                    }
                    exit_code = result.exit_code;
                }
            }

            if (!launched) {
                std::lock_guard<std::mutex> lock(state.mutex);
                state.log += "error: could not run '" + step.exe_path + "'\n";
                state.failure_summary = "no se pudo lanzar '" + step.exe_path + "'";
                all_succeeded = false;
                break;
            }
            if (exit_code != 0) {
                all_succeeded = false;
                {
                    std::lock_guard<std::mutex> lock(state.mutex);
                    state.failure_summary = "codigo de salida " + FormatExitCode(exit_code);
                }
                if (step.step_label.empty() && !saw_structured_error) {
                    std::lock_guard<std::mutex> lock(state.mutex);
                    state.log += DescribeUnexpectedExit(step.exe_path, exit_code);
                }
                if (saw_native_crash || step.step_label.empty()) {
                    std::string dump_path = FindNewestCrashDumpSince(step_started_at);
                    if (!dump_path.empty()) {
                        std::lock_guard<std::mutex> lock(state.mutex);
                        state.crash_dump_path = dump_path;
                        state.crash_code = ExtractHexField(state.log, "Codigo: ");
                        state.crash_address = ExtractHexField(state.log, "Direccion: ");
                    }
                }
                break;
            }
        }

        std::lock_guard<std::mutex> lock(state.mutex);
        state.last_success = all_succeeded;
        if (all_succeeded) state.result_path = expected_result_path;
        state.has_result = true;
        state.building = false;
    });
}

namespace {

void StartBuild(BuildPanelState& state, std::vector<std::string> args, std::string ava_cli_path,
                 std::string expected_result_path) {
    StartMultiStepBuild(state, {BuildStep{std::move(ava_cli_path), std::move(args), ""}},
                        std::move(expected_result_path));
}

void TriggerRun(BuildPanelState& state, const std::string& exe_path,
                 const std::vector<std::string>& extra_args = {}) {
    StartMultiStepBuild(state, {BuildStep{exe_path, extra_args, ""}}, exe_path);
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

void TerminateRunningProcess(BuildPanelState& state) {
    avastd::shared_ptr<ava::platform::IProcessStream::IStdinWriter> writer;
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        writer = state.running_process;
    }
    if (writer) writer->Terminate();
}

void PollBuild(BuildPanelState& state, LogBridge& log_bridge) {
    bool should_launch = false;
    std::string launch_result_path;
    std::vector<std::string> launch_extra_args;
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        FlushLogToOutput(state.log, state.log_forwarded_upto, state.has_result, "[build]   ", log_bridge);
        if (state.has_result && !state.logged_to_output) {
            std::string failed_line = "[build] failed";
            failed_line += state.failure_summary.empty() ? " -- ver el detalle en las lineas anteriores"
                                                         : " -- " + state.failure_summary;
            log_bridge.Log(state.last_success ? "[build] succeeded -> " + state.result_path : failed_line);
            state.logged_to_output = true;

            // Sello de "el binario de Run se compilo bien con los fuentes de este momento".
            if (state.last_success && !state.pending_run_stamp.empty()) WriteRunStamp(state.pending_run_stamp);
            state.pending_run_stamp.clear();
            // Failures (build errors and crashes of the launched app) are not
            // shown as a modal: the Logs panel already has the full detail
            // (file:line, stack, dump path), so a dialog would be redundant.
            // Only a successful build still offers the "Open Folder" dialog.
            state.dialog_success = state.last_success;
            state.dialog_result_path = state.result_path;
            state.show_result_dialog = state.last_success;

            if (state.launch_on_success && state.last_success) {
                should_launch = true;
                launch_result_path = state.result_path;
                launch_extra_args = state.launch_extra_args;
            }
            state.launch_on_success = false;
        }
    }
    if (should_launch) TriggerRun(state, launch_result_path, launch_extra_args);
}

TriggerBuildOutcome TriggerBuild(BuildPanelState& state, const AvaProjFile& proj, const AvaProjUserFile& user,
                                  const std::string& explorer_root_dir, LogBridge& log_bridge,
                                  bool project_ambiguous, const std::vector<std::string>& avaproj_candidates,
                                  std::optional<AvaProjOutputType> force_output_type, bool force_no_ui,
                                  bool force_debug_symbols) {
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
    const bool is_library = force_output_type.has_value() ? (*force_output_type == AvaProjOutputType::kLibrary)
                                                            : (proj.output_type == AvaProjOutputType::kLibrary);

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
            if (force_debug_symbols) args.push_back("--debug-symbols");
            if (proj.uses_ui && !force_no_ui) args.push_back("--with-ui");
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

void ReportTriggerSetupError(BuildPanelState& state, LogBridge& log_bridge, const std::string& setup_error) {
    std::lock_guard<std::mutex> lock(state.mutex);
    state.log = "error: " + setup_error;
    state.log_forwarded_upto = 0;
    state.has_result = true;
    state.last_success = false;
    state.logged_to_output = false;
    log_bridge.Log("[build] error: " + setup_error);
    state.logged_to_output = true;
}

}  // namespace

TriggerBuildOutcome TriggerDesktopUiRunBuild(BuildPanelState& state, const AvaProjFile& proj,
                                              const AvaProjUserFile& user, const std::string& explorer_root_dir,
                                              LogBridge& log_bridge, bool project_ambiguous,
                                              const std::vector<std::string>& avaproj_candidates) {
    TriggerBuildOutcome outcome;
    outcome.project_dir = explorer_root_dir;
    if (state.building.load()) return outcome;

    if (project_ambiguous) {
        std::string list;
        for (const std::string& candidate : avaproj_candidates) {
            if (!list.empty()) list += ", ";
            list += candidate;
        }
        ReportTriggerSetupError(state, log_bridge,
                                 "se encontraron varios .avaproj en " + explorer_root_dir + " (" + list +
                                     ") -- Ava Studio no puede saber cual proyecto queres compilar. Abri la "
                                     "subcarpeta del proyecto especifico (File > Open Folder) en vez de la "
                                     "carpeta que los contiene a todos.");
        return outcome;
    }

    if (util::DetectedHostPlatform() != util::HostPlatform::kWindows) {
        ReportTriggerSetupError(state, log_bridge, util::Tr("build.error_desktop_ui_windows_only"));
        return outcome;
    }

    const fs::path project_dir(outcome.project_dir);
    fs::path repo_root = user.repo_root.empty()
                              ? [&]() {
                                    fs::path detected = DetectRepoRoot(SelfExecutableDir());
                                    return detected.empty() ? DetectRepoRoot(project_dir) : detected;
                                }()
                              : fs::path(user.repo_root);

    std::error_code ec;
    std::string setup_error;
    std::string entry = proj.entry_file.empty() ? DetectEntryFile(project_dir) : proj.entry_file;
    if (repo_root.empty() || !LooksLikeRepoRoot(repo_root)) {
        setup_error = util::Tr("build.error_repo_root_not_found");
    } else if (!fs::exists(project_dir, ec) || !fs::is_directory(project_dir, ec)) {
        setup_error = util::Tr("build.error_project_dir_missing");
    } else if (entry.empty()) {
        outcome.entry_file_missing = true;
        setup_error = util::Tr("build.error_entry_file_missing");
    } else if (!fs::exists(project_dir / entry, ec)) {
        outcome.entry_file_missing = true;
        setup_error = TrFormat("build.error_entry_file_not_found", (project_dir / entry).string());
    }

    if (!setup_error.empty()) {
        ReportTriggerSetupError(state, log_bridge, setup_error);
        return outcome;
    }

    const fs::path build_dir = repo_root / "build_avastudio_run";
    const fs::path dist_dir = build_dir / "dist" / "win" / "avanative";
    const fs::path expected_result_path = dist_dir / "avanative.exe";

    state.launch_extra_args = {project_dir.string(), "--entry", entry};

    std::error_code dist_ec;
    const bool avanative_dlls_present = fs::exists(dist_dir / "avalang.dll", dist_ec) &&
                                         fs::exists(dist_dir / "avalang_ui.dll", dist_ec) &&
                                         fs::exists(dist_dir / "avalang_ui_win.dll", dist_ec);
    const bool cached_binary_present = fs::exists(expected_result_path, dist_ec) && avanative_dlls_present;

    // Antes se reutilizaba SIEMPRE el avanative.exe cacheado: si los fuentes
    // del runtime cambiaban (o el binario venia de una version anterior), Run
    // ejecutaba un .exe viejo -- con DLL desincronizadas y sin el handler de
    // crash actual -- y podia morir con un access violation sin dejar rastro. Ahora
    // solo se reutiliza si esta al dia respecto de los fuentes; set
    // AVA_STUDIO_SKIP_STALE_CHECK=1 para volver al comportamiento anterior.
    const fs::path run_stamp_path = build_dir / ".studio_run_stamp";
    std::string stale_reason;
    const bool skip_stale_check = std::getenv("AVA_STUDIO_SKIP_STALE_CHECK") != nullptr;
    if (cached_binary_present && (skip_stale_check || !RunBuildIsStale(run_stamp_path, repo_root, stale_reason))) {
        StartMultiStepBuild(state, {}, expected_result_path.string());
        return outcome;
    }

    if (cached_binary_present) {
        log_bridge.Log("[build] avanative.exe desactualizado (" + stale_reason + ") -- recompilando el runtime de Run en " +
                       build_dir.string() + " (incremental; puede tardar si cambio el runtime).");
    } else {
        log_bridge.Log("[build] " + util::Tr("build.first_run_build_notice"));
    }

    BuildStep configure_step{"cmake",
                              {"-S", repo_root.string(), "-B", build_dir.string(), "-DAVA_BUILD_SHARED=ON",
                               "-DAVA_BUILD_UI=ON", "-DAVA_BUILD_UI_BACKEND_WIN=ON", "-DAVA_BUILD_AVAHOST=ON",
                               "-DAVA_PACKAGE_DIST=ON"},
                              "cmake configure"};
    BuildStep build_step{"cmake",
                         {"--build", build_dir.string(), "--target", "avanative", "--config", "Release", "--parallel"},
                         "cmake build"};

    // Si el arbol de build ya esta configurado y solo se trata de ponerlo al
    // dia, alcanza con el build incremental (cmake --build reconfigura solo
    // si hace falta).
    std::error_code cache_ec;
    const bool skip_configure = cached_binary_present && fs::exists(build_dir / "CMakeCache.txt", cache_ec);
    std::vector<BuildStep> steps;
    if (!skip_configure) steps.push_back(configure_step);
    steps.push_back(build_step);

    StartMultiStepBuild(state, std::move(steps), expected_result_path.string());
    state.pending_run_stamp = run_stamp_path.string();

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

RunTarget PreselectRunTarget(const AvaProjFile& proj, util::HostPlatform host_platform) {
    if (proj.output_type == AvaProjOutputType::kLibrary) return RunTarget::kLibrary;
    if (proj.uses_ui && host_platform == util::HostPlatform::kWindows) return RunTarget::kDesktopUi;
    return RunTarget::kConsole;
}

const char* RunTargetLabelKey(RunTarget target) {
    switch (target) {
        case RunTarget::kConsole: return "build.run_target_console";
        case RunTarget::kDesktopUi: return "build.run_target_desktop_ui";
        case RunTarget::kLibrary: return "build.run_target_library";
    }
    return "build.run_target_console";
}

}  // namespace

TriggerBuildOutcome DispatchBuildAndRun(BuildPanelState& state, RunTarget target, const AvaProjFile& proj,
                                         const AvaProjUserFile& user, const std::string& explorer_root_dir,
                                         LogBridge& log_bridge, bool project_ambiguous,
                                         const std::vector<std::string>& avaproj_candidates) {
    state.last_built_target = target;
    switch (target) {
        case RunTarget::kConsole:
            return TriggerBuild(state, proj, user, explorer_root_dir, log_bridge, project_ambiguous,
                                 avaproj_candidates, AvaProjOutputType::kExe, true, state.debug_mode);
        case RunTarget::kDesktopUi:
            return TriggerDesktopUiRunBuild(state, proj, user, explorer_root_dir, log_bridge, project_ambiguous,
                                             avaproj_candidates);
        case RunTarget::kLibrary:
            return TriggerBuild(state, proj, user, explorer_root_dir, log_bridge, project_ambiguous,
                                 avaproj_candidates, AvaProjOutputType::kLibrary, false, state.debug_mode);
    }
    return {};
}

BuildPanelResult DrawBuildPanel(BuildPanelState& state, AvaProjFile& proj, AvaProjUserFile& user,
                                 const std::string& explorer_root_dir, BuildBrowseField browsed_field,
                                 const std::string& browsed_value, LogBridge& log_bridge, bool project_ambiguous,
                                 const std::vector<std::string>& avaproj_candidates, bool* p_open) {
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

    const util::HostPlatform run_host_platform = util::DetectedHostPlatform();
    if (!state.run_target_preselected) {
        state.selected_run_target = PreselectRunTarget(proj, run_host_platform);
        state.run_target_preselected = true;
    }

    ImGui::TextColored(palette::FromHex(palette::kInfo), "%s", util::Tr("build.section_run_target").c_str());
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    ImGui::TextColored(palette::FromHex(palette::kTextMuted), "%s", util::Tr("build.run_target_label").c_str());
    ImGui::SetNextItemWidth(-1.0f);
    const bool desktop_ui_disabled = run_host_platform != util::HostPlatform::kWindows;
    if (ImGui::BeginCombo("##run_target", util::Tr(RunTargetLabelKey(state.selected_run_target)).c_str())) {
        for (RunTarget candidate : {RunTarget::kConsole, RunTarget::kDesktopUi, RunTarget::kLibrary}) {
            const bool disabled = candidate == RunTarget::kDesktopUi && desktop_ui_disabled;
            ImGui::BeginDisabled(disabled);
            if (ImGui::Selectable(util::Tr(RunTargetLabelKey(candidate)).c_str(),
                                   state.selected_run_target == candidate)) {
                state.selected_run_target = candidate;
            }
            ImGui::EndDisabled();
            if (disabled && ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", util::Tr("build.run_target_desktop_ui_unsupported_tooltip").c_str());
            }
        }
        ImGui::EndCombo();
    }
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    ImGui::Checkbox(util::Tr("build.debug_mode_checkbox").c_str(), &state.debug_mode);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", util::Tr("build.debug_mode_checkbox_tooltip").c_str());
    }
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    const bool is_library_target = state.selected_run_target == RunTarget::kLibrary;
    const std::string dispatch_label =
        util::Tr(is_library_target ? "build.build_only_button" : "build.build_and_run_button");
    ImGui::BeginDisabled(state.building.load());
    if (ImGui::Button(dispatch_label.c_str())) {
        DispatchBuildAndRun(state, state.selected_run_target, proj, user, explorer_root_dir, log_bridge,
                             project_ambiguous, avaproj_candidates);
    }
    ImGui::EndDisabled();

    const bool can_run =
        state.has_result && state.last_success && !is_library_target && state.last_built_target == state.selected_run_target;
    if (can_run) {
        ImGui::SameLine();
        ImGui::BeginDisabled(state.building.load());
        if (ImGui::Button(util::Tr("build.run_button").c_str())) {
            if (state.selected_run_target == RunTarget::kDesktopUi) {
                TriggerRun(state, state.result_path, state.launch_extra_args);
            } else {
                std::vector<std::string> run_args;
                if (state.debug_mode) run_args.push_back("--debug-runtime");
                TriggerRun(state, state.result_path, run_args);
            }
        }
        ImGui::EndDisabled();
    }

    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    if (state.building.load()) {
        std::string log_snapshot;
        std::chrono::steady_clock::time_point last_output_at;
        {
            std::lock_guard<std::mutex> lock(state.mutex);
            log_snapshot = state.log;
            last_output_at = state.last_output_at;
        }
        const BuildStageInfo stage = DeriveBuildStage(log_snapshot);
        const std::string last_line = LastNonEmptyLine(log_snapshot);
        const double elapsed_s = std::chrono::duration<double>(
                                      std::chrono::steady_clock::now() - state.build_started_at)
                                      .count();
        const double silent_s =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - last_output_at).count();

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

        bool has_running_process = false;
        {
            std::lock_guard<std::mutex> lock(state.mutex);
            has_running_process = state.running_process != nullptr;
        }
        if (has_running_process) {
            constexpr double kHungWarningSeconds = 10.0;
            if (silent_s >= kHungWarningSeconds) {
                ImGui::Dummy(ImVec2(0.0f, 4.0f));
                ImGui::TextColored(palette::FromHex(palette::kWarning), "%s",
                                    util::TrFormat("build.hung_warning", {FormatSeconds(silent_s)}).c_str());
            }
            ImGui::Dummy(ImVec2(0.0f, 4.0f));
            ImGui::PushStyleColor(ImGuiCol_Button, palette::FromHex(palette::kError));
            if (ImGui::Button(util::Tr("build.stop_button").c_str())) {
                TerminateRunningProcess(state);
            }
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", util::Tr("build.stop_button_tooltip").c_str());
            }
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

