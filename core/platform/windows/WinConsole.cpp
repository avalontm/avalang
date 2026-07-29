#include "WinConsole.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace ava {
namespace platform {
namespace windows {

namespace {

WORD ToWinAttribute(ConsoleColor color, WORD default_attrs) {
    switch (color) {
        case ConsoleColor::Black:   return 0;
        case ConsoleColor::Red:     return FOREGROUND_RED | FOREGROUND_INTENSITY;
        case ConsoleColor::Green:   return FOREGROUND_GREEN | FOREGROUND_INTENSITY;
        case ConsoleColor::Yellow:  return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
        case ConsoleColor::Blue:    return FOREGROUND_BLUE | FOREGROUND_INTENSITY;
        case ConsoleColor::Magenta: return FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
        case ConsoleColor::Cyan:    return FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
        case ConsoleColor::White:   return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
        case ConsoleColor::Default:
        default:
            return default_attrs;
    }
}

WORD g_default_attrs = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;

void WriteRaw(HANDLE handle, const std::string& utf8_text) {
    if (utf8_text.empty()) {
        return;
    }
    DWORD written = 0;
    WriteFile(handle, utf8_text.data(), static_cast<DWORD>(utf8_text.size()), &written, nullptr);
}

} // namespace

WinConsole::WinConsole() {
    // UTF-8 in and out so callers can pass/receive UTF-8 std::string as-is.
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (out != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(out, &info)) {
        g_default_attrs = info.wAttributes;
    }
}

void WinConsole::Write(const std::string& utf8_text) {
    WriteRaw(GetStdHandle(STD_OUTPUT_HANDLE), utf8_text);
}

void WinConsole::WriteLine(const std::string& utf8_text) {
    Write(utf8_text);
    Write("\n");
}

void WinConsole::WriteError(const std::string& utf8_text) {
    WriteRaw(GetStdHandle(STD_ERROR_HANDLE), utf8_text);
    WriteRaw(GetStdHandle(STD_ERROR_HANDLE), "\n");
}

bool WinConsole::ReadLine(std::string& out_line) {
    out_line.clear();

    HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
    if (in == INVALID_HANDLE_VALUE) {
        return false;
    }

    char buffer[4096];
    for (;;) {
        DWORD read = 0;
        if (!ReadFile(in, buffer, sizeof(buffer), &read, nullptr) || read == 0) {
            return !out_line.empty();
        }

        for (DWORD i = 0; i < read; ++i) {
            if (buffer[i] == '\n') {
                if (!out_line.empty() && out_line.back() == '\r') {
                    out_line.pop_back();
                }
                return true;
            }
            out_line.push_back(buffer[i]);
        }
    }
}

void WinConsole::SetForegroundColor(ConsoleColor color) {
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (out == INVALID_HANDLE_VALUE) {
        return;
    }
    SetConsoleTextAttribute(out, ToWinAttribute(color, g_default_attrs));
}

void WinConsole::ResetColor() {
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (out == INVALID_HANDLE_VALUE) {
        return;
    }
    SetConsoleTextAttribute(out, g_default_attrs);
}

} // namespace windows
} // namespace platform
} // namespace ava
