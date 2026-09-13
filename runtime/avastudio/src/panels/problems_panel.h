#pragma once

#include <optional>
#include <string>
#include <vector>

#include "engine/engine_bridge.h"
#include "languages/diagnostics_engine.h"

namespace studio {

struct ProblemEntry {
    std::string source_label;
    std::string file;
    int line = 0;
    int column = 0;
    std::string message;
    diagnostics::Severity severity = diagnostics::Severity::Error;
};

struct ProblemsState {
    std::vector<ProblemEntry> entries;

    int selection_anchor = -1;
    int selection_cursor = -1;
};

void UpdateProblemsFromResult(ProblemsState& state, const std::string& source_label, const RunResult& result,
                               const std::string& fallback_file);

void UpdateProblemsFromDiagnostics(ProblemsState& state, const std::string& source_label,
                                    std::vector<ProblemEntry> fresh_entries);

struct ProblemsFileClickRequest {
    std::string file_path;
    int line = 0;
    int column = 0;
    std::string message;
};

std::optional<ProblemsFileClickRequest> DrawProblemsPanel(ProblemsState& state, bool* p_open = nullptr);

}