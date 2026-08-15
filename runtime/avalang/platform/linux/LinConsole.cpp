#include "LinConsole.h"
#include <cstdio>

namespace ava {
namespace platform {
namespace linux_ {

LinConsole::LinConsole() = default;

void LinConsole::Write(const std::string& utf8_text) {
    std::fwrite(utf8_text.data(), 1, utf8_text.size(), stdout);
    std::fflush(stdout);
}

void LinConsole::WriteLine(const std::string& utf8_text) {
    Write(utf8_text);
    std::fputc('\n', stdout);
    std::fflush(stdout);
}

void LinConsole::WriteError(const std::string& utf8_text) {
    std::fwrite(utf8_text.data(), 1, utf8_text.size(), stderr);
    std::fputc('\n', stderr);
    std::fflush(stderr);
}

bool LinConsole::ReadLine(std::string& out_line) {
    out_line.clear();
    char buf[4096];
    if (!std::fgets(buf, sizeof(buf), stdin)) {
        return !out_line.empty();
    }
    out_line = buf;
    if (!out_line.empty() && out_line.back() == '\n') out_line.pop_back();
    if (!out_line.empty() && out_line.back() == '\r') out_line.pop_back();
    return true;
}

static const char* ColorToAnsi(ConsoleColor color) {
    switch (color) {
        case ConsoleColor::Black:   return "\033[30m";
        case ConsoleColor::Red:     return "\033[31m";
        case ConsoleColor::Green:    return "\033[32m";
        case ConsoleColor::Yellow:   return "\033[33m";
        case ConsoleColor::Blue:     return "\033[34m";
        case ConsoleColor::Magenta:  return "\033[35m";
        case ConsoleColor::Cyan:     return "\033[36m";
        case ConsoleColor::White:    return "\033[37m";
        case ConsoleColor::Default:
        default:                     return "\033[0m";
    }
}

void LinConsole::SetForegroundColor(ConsoleColor color) {
    std::fputs(ColorToAnsi(color), stdout);
    std::fflush(stdout);
}

void LinConsole::ResetColor() {
    std::fputs("\033[0m", stdout);
    std::fflush(stdout);
}

} // namespace linux_
} // namespace platform
} // namespace ava
