#ifndef AVA_PLATFORM_WIN_CONSOLE_H
#define AVA_PLATFORM_WIN_CONSOLE_H

#include "../interfaces/IConsole.h"

namespace ava {
namespace platform {
namespace windows {

class WinConsole : public IConsole {
public:
    WinConsole();

    void Write(const std::string& utf8_text) override;
    void WriteLine(const std::string& utf8_text) override;
    void WriteError(const std::string& utf8_text) override;

    bool ReadLine(std::string& out_line) override;

    void SetForegroundColor(ConsoleColor color) override;
    void ResetColor() override;
};

} // namespace windows
} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_WIN_CONSOLE_H
