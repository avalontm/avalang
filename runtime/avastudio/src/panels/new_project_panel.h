#pragma once

#include <optional>
#include <string>

namespace studio {

// kConsole: plain main.ava, no UI files (previously "Empty").
// kUi: main.ava + app.ava (font/style declarations, same convention already
// used by AvaHost projects, see samples/web/testproj/app.ava) + styles.ava +
// a views/ folder with one starter screen (views/Home.avaui, built from the
// same BuildScaffoldContent(kScreen, ...) the Explorer's "Generate > Screen"
// action already uses) -- previously "With example screen", which only
// dropped a single screen.avaui at the project root with no app.ava/styles.
enum class NewProjectTemplateKind { kConsole, kUi };

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
