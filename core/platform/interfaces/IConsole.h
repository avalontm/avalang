#ifndef AVA_PLATFORM_ICONSOLE_H
#define AVA_PLATFORM_ICONSOLE_H

#include <string>

namespace ava {
namespace platform {

enum class ConsoleColor {
    Default,
    Black,
    Red,
    Green,
    Yellow,
    Blue,
    Magenta,
    Cyan,
    White
};

class IConsole {
public:
    virtual ~IConsole() = default;

    // Writes UTF-8 text as-is; implementations own any codepage/encoding
    // setup needed on the host OS.
    virtual void Write(const std::string& utf8_text) = 0;
    virtual void WriteLine(const std::string& utf8_text) = 0;
    virtual void WriteError(const std::string& utf8_text) = 0;

    // Reads a single line from stdin. Returns false on EOF.
    virtual bool ReadLine(std::string& out_line) = 0;

    virtual void SetForegroundColor(ConsoleColor color) = 0;
    virtual void ResetColor() = 0;
};

} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_ICONSOLE_H
