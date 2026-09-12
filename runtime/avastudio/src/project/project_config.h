#pragma once

#include <string>
#include <vector>

#include "project/avaproj_file.h"
#include "project/avaproj_user_file.h"

namespace studio {

struct ProjectConfig {
    std::string dir;
    std::string avaproj_path;
    AvaProjFile proj;
    AvaProjUserFile user;

    // Fix: true cuando `dir` contiene mas de un .avaproj y por lo tanto no
    // se puede saber cual es "el" proyecto -- antes esto se resolvia en
    // silencio inventando un AvaProjFile default (ver LoadProjectConfig),
    // lo que hacia que build tomara cualquier .ava al azar de cualquiera
    // de los proyectos presentes. Con este flag, TriggerBuild (build_panel)
    // puede negarse a compilar y explicar el problema en vez de adivinar.
    bool ambiguous_avaproj = false;
    std::vector<std::string> avaproj_candidates;
};

ProjectConfig LoadProjectConfig(const std::string& dir);

void SaveProjectConfig(const ProjectConfig& config);

bool EnsureGitignoreEntry(const std::string& dir, const std::string& pattern);

}
