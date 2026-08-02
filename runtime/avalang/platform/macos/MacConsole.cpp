#include "MacConsole.h"
#include <cstdio>

// STUB implementation. Write/WriteLine/WriteError use plain stdio so basic
// CLI output still works; color and ReadLine are not implemented yet.
// TODO(Phase 6): ANSI escape codes for color,
// proper UTF-8 handling, mirroring core/platform/windows/MacConsole.cpp.

namespace ava {
namespace platform {
namespace macos_ {

MacConsole::MacConsole() = default;

void MacConsole::Write(const std::string& utf8_text) {
    std::fwrite(utf8_text.data(), 1, utf8_text.size(), stdout);
}

void MacConsole::WriteLine(const std::string& utf8_text) {
    Write(utf8_text);
    std::fputc('\n', stdout);
}

void MacConsole::WriteError(const std::string& utf8_text) {
    std::fwrite(utf8_text.data(), 1, utf8_text.size(), stderr);
    std::fputc('\n', stderr);
}

bool MacConsole::ReadLine(std::string& /*out_line*/) {
    return false;
}

void MacConsole::SetForegroundColor(ConsoleColor /*color*/) {
    // Not implemented.
}

void MacConsole::ResetColor() {
    // Not implemented.
}

} // namespace macos_
} // namespace platform
} // namespace ava
