#ifndef AVA_PLATFORM_LIN_CONSOLE_H
#define AVA_PLATFORM_LIN_CONSOLE_H

#include "../interfaces/IConsole.h"

namespace ava {
namespace platform {
namespace linux_ {

// STUB. TODO: back with stdio + ANSI escape codes for color.
class LinConsole : public IConsole {
public:
    LinConsole();

    void Write(const std::string& utf8_text) override;
    void WriteLine(const std::string& utf8_text) override;
    void WriteError(const std::string& utf8_text) override;

    bool ReadLine(std::string& out_line) override;

    void SetForegroundColor(ConsoleColor color) override;
    void ResetColor() override;
};

} // namespace linux_
} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_LIN_CONSOLE_H
