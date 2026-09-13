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

    // Fix: true cuando el .avaproj existe en disco pero no se pudo parsear
    // (XML corrupto/truncado, por ejemplo por un guardado anterior que
    // fallo a mitad de camino) -- antes esto caia en silencio a un
    // AvaProjFile{} default (todo Exe/bin/etc.), indistinguible de "recien
    // creado", y el usuario veia sus opciones "resetearse" sin ningun
    // aviso. El caller (main.cpp) puede usar este flag para loguear una
    // advertencia clara en vez de mostrar los defaults sin mas.
    bool load_parse_failed = false;
};

ProjectConfig LoadProjectConfig(const std::string& dir);

// Fix: antes devolvia void y el resultado de SaveAvaProjFile/
// SaveAvaProjUserFile se descartaba -- si el guardado fallaba (archivo
// bloqueado por OneDrive/antivirus, permisos, disco lleno, etc.) la
// seleccion del usuario vivia bien en memoria durante la sesion pero
// nunca llegaba al disco, y recien se notaba al reabrir el proyecto,
// sin ningun error en el medio. Ahora devuelve false si cualquiera de
// los dos guardados fallo, para que el caller pueda avisar.
bool SaveProjectConfig(const ProjectConfig& config);

bool EnsureGitignoreEntry(const std::string& dir, const std::string& pattern);

}
