#ifndef AVA_FRONTEND_FRONTEND_H
#define AVA_FRONTEND_FRONTEND_H

#include "../../platform/barekernel/stdcompat/ava_stdcompat.h"
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
avastd::shared_ptr<Proto> CompileSource(const avastd::string& source, const avastd::string& source_name);

// Compiles an AvaLang source file into a top-level Proto ready for VM::Run.
// Reads the file and calls CompileSource.
avastd::shared_ptr<Proto> CompileFile(const avastd::string& file_path);

} // namespace ava

#endif // AVA_FRONTEND_FRONTEND_H
