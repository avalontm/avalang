#include "context_builder.h"

#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

bool ShouldSkipDir(const std::string& name) {
    static const std::vector<std::string> skip = {".git",     "build",     "out",       "vcpkg",
                                                    "node_modules", ".vs", ".idea", "cmake-build-debug"};
    for (const auto& s : skip) {
        if (name == s) return true;
    }
    return false;
}

// Shared walk used by both BuildFileTree (Fase 3, text for the prompt)
// and ListProjectFiles (Fase 4, structured list for the list_project_files
// tool) -- one truncation/skip policy instead of two that could drift.
std::vector<std::string> WalkProjectFiles(const fs::path& root, size_t max_files) {
    std::vector<std::string> result;
    std::error_code ec;
    if (!fs::exists(root, ec)) return result;

    fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec);
    fs::recursive_directory_iterator end;
    for (; it != end && result.size() < max_files; it.increment(ec)) {
        if (ec) break;
        const auto& entry = *it;

        if (entry.is_directory(ec)) {
            if (ShouldSkipDir(entry.path().filename().string())) {
                it.disable_recursion_pending();
            }
            continue;
        }
        if (!entry.is_regular_file(ec)) continue;

        std::string rel = fs::relative(entry.path(), root, ec).generic_string();
        if (ec) continue;
        result.push_back(std::move(rel));
    }
    return result;
}

std::string BuildFileTree(const fs::path& root, size_t max_chars, size_t max_files) {
    std::vector<std::string> files = WalkProjectFiles(root, max_files + 1); // +1 so we can detect "more remain"

    std::ostringstream out;
    bool truncated = files.size() > max_files;
    if (truncated) files.resize(max_files);

    for (const auto& rel : files) {
        std::string line = rel + "\n";
        if (static_cast<size_t>(out.tellp()) + line.size() > max_chars) {
            truncated = true;
            break;
        }
        out << line;
    }
    if (truncated) out << "... (arbol truncado)\n";
    return out.str();
}

} // namespace

std::vector<std::string> ListProjectFiles(const std::string& root, size_t max_files) {
    if (root.empty()) return {};
    return WalkProjectFiles(fs::path(root), max_files);
}

std::string BuildContextMessage(AvaStudioHost* host, size_t max_chars) {
    if (!host) return "";

    std::ostringstream out;
    out << "[Contexto automatico del proyecto -- generado por Ava Studio, no escrito por el usuario]\n\n";

    const char* out_path = nullptr;
    const char* out_content = nullptr;
    int sel_start = -1, sel_end = -1;
    bool has_active = host->services.get_active_file(host, &out_path, &out_content, &sel_start, &sel_end);

    if (has_active) {
        std::string path = out_path ? out_path : "";
        std::string content = out_content ? out_content : "";

        out << "Archivo abierto: " << (path.empty() ? "(sin guardar)" : path) << "\n";
        if (sel_start >= 0 && sel_end >= 0 && sel_end > sel_start) {
            out << "Seleccion activa: bytes " << sel_start << "-" << sel_end << "\n";
        }

        const size_t kMaxFileChars = 6000;
        if (content.size() > kMaxFileChars) {
            content = content.substr(0, kMaxFileChars) + "\n... [contenido truncado]";
        }
        out << "--- contenido ---\n" << content << "\n--- fin contenido ---\n\n";
    } else {
        out << "No hay ningun archivo abierto en el editor.\n\n";
    }

    const char* run_text = nullptr;
    bool had_error = false;
    if (host->services.get_last_run_output(host, &run_text, &had_error)) {
        out << "Ultimo resultado de compilacion/ejecucion (" << (had_error ? "con error" : "OK") << "):\n";
        out << (run_text ? run_text : "") << "\n\n";
    }

    const char* root = host->services.get_project_root(host);
    std::string project_root = (root && root[0] != '\0') ? root : "";
    if (!project_root.empty()) {
        size_t used = out.str().size();
        size_t remaining = (used < max_chars) ? (max_chars - used) : 0;
        if (remaining > 200) {
            std::string tree = BuildFileTree(project_root, remaining - 100, 400);
            if (!tree.empty()) {
                out << "Arbol de archivos del proyecto (rutas relativas a la raiz):\n" << tree << "\n";
            }
        }
    }

    std::string result = out.str();
    if (result.size() > max_chars) {
        result = result.substr(0, max_chars) + "\n... [contexto truncado]";
    }
    return result;
}
