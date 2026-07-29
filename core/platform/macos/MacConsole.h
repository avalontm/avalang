#ifndef AVA_PLATFORM_MAC_CONSOLE_H
#define AVA_PLATFORM_MAC_CONSOLE_H

#include "../interfaces/IConsole.h"

namespace ava {
namespace platform {
namespace macos_ {

// STUB. TODO: back with stdio + ANSI escape codes for color.
class MacConsole : public IConsole {
public:
    MacConsole();

    void Write(const std::string& utf8_text) override;
    void WriteLine(const std::string& utf8_text) override;
    void WriteError(const std::string& utf8_text) override;

    bool ReadLine(std::string& out_line) override;

    void SetForegroundColor(ConsoleColor color) override;
    void ResetColor() override;
};

} // namespace macos_
} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_MAC_CONSOLE_H
