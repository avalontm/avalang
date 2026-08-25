#pragma once

#include <optional>
#include <string>
#include <vector>

namespace studio {

struct FindInProjectMatch {
    std::string file;
    int line = 0;
    // 1-based, half-open [column_start, column_end), same convention as the
    // rest of the codebase's line/column reporting (RunResult, ProblemEntry).
    int column_start = 0;
    int column_end = 0;
    std::string line_text;
};

struct FindInProjectState {
    std::string query;
    bool case_sensitive = false;

    // Grouped by file already (search walks files in sorted order and
    // appends in-order), so DrawFindInProjectPanel doesn't need to re-sort.
    std::vector<FindInProjectMatch> matches;

    bool searched = false;
    int files_scanned = 0;
    // Set when the scan hit kMaxMatches and stopped early, so the panel can
    // say "showing the first N" instead of silently truncating.
    bool truncated = false;

    int selected_index = -1;

    // Set by callers (e.g. the Ctrl+Shift+F handler / Edit menu item) to ask
    // the panel to grab keyboard focus on its query field next frame.
    bool focus_query_field = false;
};

// Recursively scans every ".ava"/".avaui" file under project_root for
// `state.query` (plain substring match, no regex -- matches the scope of
// §5.2 in the plan) and replaces `state.matches`. No-op (clears matches) if
// the query is empty. Caps at a fixed number of matches so a very broad
// query on a large project can't make the panel unusable.
void RunFindInProject(FindInProjectState& state, const std::string& project_root);

struct FindInProjectClickRequest {
    std::string file_path;
    int line = 0;
    int column_start = 0;
    int column_end = 0;
};

// Draws the query field + case-sensitivity toggle + results grouped by file.
// Re-runs RunFindInProject(state, project_root) itself when the query is
// submitted/the Search button is clicked/the case-sensitivity toggle changes
// after a search already happened -- the panel owns when to search, so
// callers don't need to inspect its widgets to know when to invoke it.
// Returns a click request when the user clicks a result line; the caller is
// expected to open the file (OpenFileInTab) and then call
// SelectMatchInEditor -- same two-step pattern already used for
// Terminal/Problems file clicks in main.cpp.
std::optional<FindInProjectClickRequest> DrawFindInProjectPanel(FindInProjectState& state,
                                                                 const std::string& project_root,
                                                                 bool* p_open = nullptr);

}
