#ifndef AVA_COMMON_AVA_ERROR_H
#define AVA_COMMON_AVA_ERROR_H

#include <stdexcept>
#include <string>

namespace ava {

// Base error type carrying a structured source position alongside the
// human-readable message every std::runtime_error already has. line/column
// are 1-based; 0 means "unknown" (e.g. an error with no meaningful source
// position, or a Proto compiled before line tracking existed).
//
// CompileError (frontend.h) inherits from this so both compile-time and
// run-time failures can be caught uniformly as AvaError by callers that
// want to show the user exactly where things went wrong (see
// public/src/c_api.cpp and core/src/vm/vm.cpp).
struct AvaError : std::runtime_error {
    int line;
    int column;
    // Path of the source file the error originates from. Empty when the
    // caller doesn't know it yet (e.g. constructed before the frame's
    // Proto::source_name was propagated) or when the error has no
    // meaningful file association.
    std::string source;

    explicit AvaError(const std::string& message, int line = 0, int column = 0,
                       const std::string& source = "")
        : std::runtime_error(message), line(line), column(column), source(source) {}
};

} // namespace ava

#endif // AVA_COMMON_AVA_ERROR_H
