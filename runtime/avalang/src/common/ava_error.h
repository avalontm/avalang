#ifndef AVA_COMMON_AVA_ERROR_H
#define AVA_COMMON_AVA_ERROR_H

#include "../../platform/barekernel/stdcompat/ava_stdcompat.h"

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
struct AvaError : avastd::runtime_error {
    int line;
    int column;
    // Path of the source file the error originates from. Empty when the
    // caller doesn't know it yet (e.g. constructed before the frame's
    // Proto::source_name was propagated) or when the error has no
    // meaningful file association.
    avastd::string source;

    explicit AvaError(const avastd::string& message, int line = 0, int column = 0,
                       const avastd::string& source = "")
        : avastd::runtime_error(message), line(line), column(column), source(source) {}

#if !AVA_HAVE_EXCEPTIONS
    // Ver ava_type_tag() en ava_error.h (stdcompat) y AvaRaiseException en
    // vm_internal.h (tag 1). AvaError usa 2 para que callers sin RTTI
    // (public/src/c_api.cpp) puedan distinguir "es un AvaError con
    // line/column/source" de un avastd::runtime_error generico dentro de un
    // solo AVA_CATCH(avastd::exception, e).
    int ava_type_tag() const noexcept override { return 2; }
#endif
};

} // namespace ava

#endif // AVA_COMMON_AVA_ERROR_H
