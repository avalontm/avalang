#pragma once

#include <optional>
#include <string>
#include <vector>

#include "engine/engine_bridge.h"

namespace studio {

struct ProblemEntry {
    std::string source_label;
    std::string file;
    int line = 0;
    int column = 0;
    std::string message;
};

struct ProblemsState {
    std::vector<ProblemEntry> entries;

    int selection_anchor = -1;
    int selection_cursor = -1;
};

// Single sink for diagnostics: both "Check" and "Run" call this with their
// result so Problems never diverges from what Terminal already knows about.
// A successful result clears whatever this same source_label last reported;
// a failing one replaces it with the new diagnostic. fallback_file is used
// when the engine didn't attach a source file to the error.
void UpdateProblemsFromResult(ProblemsState& state, const std::string& source_label, const RunResult& result,
                               const std::string& fallback_file);

struct ProblemsFileClickRequest {
    std::string file_path;
    int line = 0;
    int column = 0;
    std::string message;
};

std::optional<ProblemsFileClickRequest> DrawProblemsPanel(ProblemsState& state, bool* p_open = nullptr);

}
