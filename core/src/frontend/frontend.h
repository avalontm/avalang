#ifndef AVA_FRONTEND_FRONTEND_H
#define AVA_FRONTEND_FRONTEND_H

#include <memory>
#include <stdexcept>
#include <string>
#include "../common/ava_error.h"
#include "../vm/proto.h"

namespace ava {

struct CompileError : AvaError {
    using AvaError::AvaError;
};

// Compiles AvaLang source text into a top-level Proto ready for VM::Run.
// Throws CompileError with a human-readable message on any lex/parse/
// compile failure. Implemented in frontend_antlr.cpp when ANTLR4 is
// available, or frontend_stub.cpp otherwise (see CMakeLists.txt).
std::shared_ptr<Proto> CompileSource(const std::string& source, const std::string& source_name);

} // namespace ava

#endif // AVA_FRONTEND_FRONTEND_H
