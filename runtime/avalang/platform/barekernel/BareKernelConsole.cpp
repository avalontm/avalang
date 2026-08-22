#include "BareKernelConsole.h"
#include "ckm_contract.h"
#include "BareKernelCaps.h"

namespace ava {
namespace platform {
namespace barekernel {

void BareKernelConsole::EmitAnsi(const char* seq) {
#if CKM_CAP_COLOR
    ckm_write(CKM_STDOUT, seq, (long)avastd::strlen(seq));
#else
    (void)seq;
#endif
}

void BareKernelConsole::Write(const avastd::string& utf8_text) {
    ckm_write(CKM_STDOUT, utf8_text.data(), (long)utf8_text.size());
}

void BareKernelConsole::WriteLine(const avastd::string& utf8_text) {
    Write(utf8_text);
    ckm_write(CKM_STDOUT, "\n", 1);
}

void BareKernelConsole::WriteError(const avastd::string& utf8_text) {
    ckm_write(CKM_STDERR, utf8_text.data(), (long)utf8_text.size());
}

bool BareKernelConsole::ReadLine(avastd::string& out_line) {
    out_line.clear();
    char c;
    while (true) {
        long n = ckm_read(CKM_STDIN, &c, 1);
        if (n <= 0) return !out_line.empty();
        if (c == '\n') break;
        if (c != '\r') out_line.push_back(c);
    }
    return true;
}

void BareKernelConsole::SetForegroundColor(ConsoleColor color) {
#if CKM_CAP_COLOR
    static const char* codes[] = {
        "\033[0m", "\033[30m", "\033[31m", "\033[32m",
        "\033[33m", "\033[34m", "\033[35m", "\033[36m", "\033[37m"
    };
    EmitAnsi(codes[static_cast<int>(color)]);
#else
    (void)color;
#endif
}

void BareKernelConsole::ResetColor() {
    EmitAnsi("\033[0m");
}

}
}
}
