#include "project/project_config.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#include "util/project_utils.h"

namespace studio {

namespace fs = std::filesystem;

namespace {

std::string UserFilePathFor(const std::string& avaproj_path) {
    return avaproj_path + ".user";
}

std::string DefaultAvaProjPathFor(const std::string& dir) {
    fs::path dir_path(dir);
    std::string name = dir_path.filename().string();
    if (name.empty()) name = "project";
    return (dir_path / (name + ".avaproj")).string();
}

}  // namespace

ProjectConfig LoadProjectConfig(const std::string& dir) {
    ProjectConfig config;
    config.dir = dir;

    const std::vector<std::string> candidates = FindAllAvaProjInDir(dir);
    if (candidates.size() > 1) {
        // Ambiguo: no inventamos un proyecto default ni elegimos uno al
        // azar -- se marca ambiguous_avaproj y se deja que el caller
        // (TriggerBuild en build_panel.cpp) rechace el build con un
        // mensaje claro listando los candidatos, en vez de compilar
        // silenciosamente el .ava incorrecto.
        config.ambiguous_avaproj = true;
        config.avaproj_candidates = candidates;
        config.avaproj_path = candidates.front();
    } else {
        config.avaproj_path = candidates.empty() ? DefaultAvaProjPathFor(dir) : candidates.front();
    }

    if (auto loaded = LoadAvaProjFile(config.avaproj_path)) {
        config.proj = *loaded;
    } else {
        std::error_code exists_ec;
        config.load_parse_failed = fs::exists(config.avaproj_path, exists_ec);
        config.proj = AvaProjFile{};
        config.proj.project_name = fs::path(dir).filename().string();
        config.proj.uses_ui = DetectUsesUi(dir);
    }

    if (auto loaded_user = LoadAvaProjUserFile(UserFilePathFor(config.avaproj_path))) {
        config.user = *loaded_user;
    } else {
        config.user = AvaProjUserFile{};
    }

    return config;
}

bool SaveProjectConfig(const ProjectConfig& config) {
    if (config.avaproj_path.empty()) return false;

    std::error_code ec;
    fs::create_directories(fs::path(config.avaproj_path).parent_path(), ec);

    const bool proj_ok = SaveAvaProjFile(config.avaproj_path, config.proj);
    const bool user_ok = SaveAvaProjUserFile(UserFilePathFor(config.avaproj_path), config.user);
    EnsureGitignoreEntry(config.dir, "*.avaproj.user");
    return proj_ok && user_ok;
}

bool EnsureGitignoreEntry(const std::string& dir, const std::string& pattern) {
    const fs::path gitignore_path = fs::path(dir) / ".gitignore";

    std::ifstream in(gitignore_path);
    std::string line;
    while (std::getline(in, line)) {
        if (line == pattern) return true;
    }
    in.close();

    std::ofstream out(gitignore_path, std::ios::app);
    if (!out) return false;
    out << pattern << "\n";
    return true;
}

}  // namespace studio
