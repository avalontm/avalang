#pragma once

#include <optional>
#include <string>

#include "project/avaproj_file.h"

namespace studio {

// kConsole: plain main.ava, no UI files (previously "Empty"). Saved as
// OutputType=Exe, UsesUi=false.
// kDesktopUi: main.ava + app.ava (font/style declarations, same convention
// already used by AvaHost projects, see samples/web/testproj/app.ava) +
// styles.ava + a views/ folder with one starter screen (views/Home.avaui,
// built from the same BuildScaffoldContent(kScreen, ...) the Explorer's
// "Generate > Screen" action already uses). Saved as OutputType=Exe,
// UsesUi=true so the project starts ready for the Desktop (UI) run target
// in the Build panel (see build_panel.cpp).
// kLibrary: plain main.ava, no UI files. Saved as OutputType=Library,
// UsesUi=false. BareKernel isn't offered here -- it stays a
// Properties-panel-only choice made after creation, since it also needs a
// toolchain path configured, not something a brand new project has yet.
enum class NewProjectTemplateKind { kConsole, kDesktopUi, kLibrary };

struct NewProjectState {
    std::string name;
    std::string destination;

    NewProjectTemplateKind template_kind = NewProjectTemplateKind::kConsole;

    // Visual Studio-style template browser: free-text search plus a
    // category filter, both applied against the data-driven template list
    // in new_project_panel.cpp. Empty search/category means "show all".
    std::string template_search;
    std::string template_category;

    std::string error_key;

    bool focus_name_field = false;
};

struct NewProjectResult {
    std::string project_dir;
    std::string entry_file;
};

struct NewProjectDrawResult {
    std::optional<NewProjectResult> created;

    bool browse_destination_requested = false;
};

void OpenNewProjectDialog(NewProjectState& state, const std::string& default_destination);

NewProjectDrawResult DrawNewProjectDialog(NewProjectState& state);

}
