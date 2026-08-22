#ifndef AVA_PLATFORM_BAREKERNEL_CONSOLE_H
#define AVA_PLATFORM_BAREKERNEL_CONSOLE_H

#include "../interfaces/IConsole.h"
#include "../interfaces/PAL_ABI.h"
#include "stdcompat/ava_stdcompat.h"

namespace ava {
namespace platform {
namespace barekernel {

class BareKernelConsole : public IConsole {
public:
    void Write(const avastd::string& utf8_text) override;
    void WriteLine(const avastd::string& utf8_text) override;
    void WriteError(const avastd::string& utf8_text) override;
    bool ReadLine(avastd::string& out_line) override;
    void SetForegroundColor(ConsoleColor color) override;
    void ResetColor() override;

private:
    void EmitAnsi(const char* seq);
};

}
}
}

#endif
