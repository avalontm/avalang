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

private:
    // Fase 7 bugfix: ReadFile() crudo no bufferea internamente entre
    // llamadas como sí lo hace fgets() en libc (el que usa LinConsole).
    // Si una sola lectura trae mas de una linea, todo lo que quede
    // despues del primer '\n' tiene que sobrevivir hasta el proximo
    // ReadLine() en vez de perderse -- ver ReadLine() en el .cpp.
    std::string pending_input_;
};

} // namespace windows
} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_WIN_CONSOLE_H
