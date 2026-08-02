#include "LinConsole.h"
#include <cstdio>

// STUB implementation. Write/WriteLine/WriteError use plain stdio so basic
// CLI output still works; color and ReadLine are not implemented yet.
// TODO(Phase 5): ANSI escape codes for color,
// proper UTF-8 handling, mirroring core/platform/windows/LinConsole.cpp.

namespace ava {
namespace platform {
namespace linux_ {

LinConsole::LinConsole() = default;

void LinConsole::Write(const std::string& utf8_text) {
    std::fwrite(utf8_text.data(), 1, utf8_text.size(), stdout);
}

void LinConsole::WriteLine(const std::string& utf8_text) {
    Write(utf8_text);
    std::fputc('\n', stdout);
}

void LinConsole::WriteError(const std::string& utf8_text) {
    std::fwrite(utf8_text.data(), 1, utf8_text.size(), stderr);
    std::fputc('\n', stderr);
}

bool LinConsole::ReadLine(std::string& /*out_line*/) {
    return false;
}

void LinConsole::SetForegroundColor(ConsoleColor /*color*/) {
    // Not implemented.
}

void LinConsole::ResetColor() {
    // Not implemented.
}

} // namespace linux_
} // namespace platform
} // namespace ava
