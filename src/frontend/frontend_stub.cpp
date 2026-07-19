// Placeholder implementation compiled ONLY when AVA_HAVE_ANTLR is NOT
// defined (see CMakeLists.txt). This lets the VM, C API and CLI build and
// link into a working library/binary before the ANTLR4 C++ runtime and
// generated parser are wired in -- useful for exercising the VM directly
// with hand-built Proto objects while the frontend is still in progress.
//
// Once antlr4-runtime is available, this file is excluded from the build
// and frontend_antlr.cpp (AST builder + compiler over the generated
// AvaLangParser) is compiled instead.
#include "frontend.h"

namespace ava {

std::shared_ptr<Proto> CompileSource(const std::string&, const std::string&) {
    throw CompileError(
        "AvaLang frontend not built: antlr4-runtime was not found by CMake. "
        "Install the ANTLR4 C++ runtime and reconfigure to enable "
        "ava_compile(); see DESIGN.md build order, steps 2-6."
    );
}

} // namespace ava
