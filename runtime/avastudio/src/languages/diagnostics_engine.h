#pragma once

#include <string>
#include <vector>

#include "languages/class_index.h"
#include "languages/function_index.h"
#include "languages/member_access_resolver.h"

namespace studio::diagnostics {

enum class Severity { Error, Warning, Info, Hint };

enum class Kind {
    UnresolvedSymbol,
    MissingImport,
    UnresolvedImportPath,
    UnusedVariable,
    UnusedImport,
    UnusedPrivateMember,
    UnreachableCode,
};

struct Diagnostic {
    Severity severity = Severity::Warning;
    Kind kind = Kind::UnresolvedSymbol;
    int line = 0;
    int column_start = 0;
    int column_end = 0;
    std::string message;
    std::string symbol;
    std::vector<std::string> import_segments;
};

struct InlayHint {
    int line = 0;
    int column = 0;
    std::string text;
};

std::vector<Diagnostic> ComputeDiagnostics(const std::string& text, const std::string& current_file_dir,
                                            const std::string& stdlib_dir, const std::string& project_root,
                                            const ClassIndex& class_index,
                                            const FunctionIndex& function_index,
                                            const ClassIndex* workspace_classes,
                                            const FunctionIndex* workspace_functions);

std::vector<InlayHint> ComputeInlayHints(const std::string& text, const VariableTypeIndex& variable_types,
                                          const FunctionIndex& function_index,
                                          const ClassIndex* workspace_classes,
                                          const FunctionIndex* workspace_functions);

}
