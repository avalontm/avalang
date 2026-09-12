#pragma once

#include <string>

#include "project/avaproj_file.h"
#include "project/avaproj_user_file.h"

namespace studio {

struct ProjectPropertiesState {
    bool open = false;
    int active_tab = 0;
};

enum class ProjectPropertiesBrowseField {
    kNone,
    kIcon,
    kAvaCliPath,
    kKeyFile,
    kVcpkgRoot,
    kCompilerPathDesktop,
    kCompilerPathBarekernel,
    kReferenceFile,
};

struct ProjectPropertiesResult {
    ProjectPropertiesBrowseField browse_requested = ProjectPropertiesBrowseField::kNone;

    bool dirty = false;
};

ProjectPropertiesResult DrawProjectPropertiesPanel(ProjectPropertiesState& state, AvaProjFile& proj,
                                                     AvaProjUserFile& user, const std::string& project_dir,
                                                     ProjectPropertiesBrowseField browsed_field,
                                                     const std::string& browsed_value);

}
