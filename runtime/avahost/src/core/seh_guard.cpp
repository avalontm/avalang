#include "core/seh_guard.h"

#if defined(_WIN32)
#include <windows.h>
#include <eh.h>
#include <sstream>

namespace avahost {

namespace {

// Resolves the loaded module (exe/dll) an address falls in, plus its
// offset from that module's base -- lets the log say e.g.
// "avalang_ui.dll+0x1a6f1" instead of a raw absolute pointer, so a bug
// can be triaged (UI engine vs VM vs host) without attaching a debugger.
bool DescribeModuleAndOffset(void* address, std::string& outModule, uintptr_t& outOffset) {
    if (!address) return false;
    MEMORY_BASIC_INFORMATION mbi{};
    if (VirtualQuery(address, &mbi, sizeof(mbi)) == 0 || mbi.AllocationBase == nullptr) {
        return false;
    }
    HMODULE hModule = reinterpret_cast<HMODULE>(mbi.AllocationBase);
    char path[MAX_PATH]{};
    DWORD len = GetModuleFileNameA(hModule, path, MAX_PATH);
    if (len == 0) return false;

    std::string fullPath(path, len);
    std::size_t sep = fullPath.find_last_of("\\/");
    outModule = (sep == std::string::npos) ? fullPath : fullPath.substr(sep + 1);
    outOffset = reinterpret_cast<uintptr_t>(address) - reinterpret_cast<uintptr_t>(hModule);
    return true;
}

std::string DescribeSehCode(unsigned int code, void* address) {
    const char* name = "unknown structured exception";
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION:      name = "access violation"; break;
        case EXCEPTION_STACK_OVERFLOW:        name = "stack overflow"; break;
        case EXCEPTION_INT_DIVIDE_BY_ZERO:    name = "integer divide by zero"; break;
        case EXCEPTION_ILLEGAL_INSTRUCTION:   name = "illegal instruction"; break;
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: name = "array bounds exceeded"; break;
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:    name = "floating point divide by zero"; break;
        default: break;
    }
    std::ostringstream oss;
    oss << "native crash (" << name << ", code 0x" << std::hex << code
        << ") at address 0x" << reinterpret_cast<uintptr_t>(address);

    std::string moduleName;
    uintptr_t offset = 0;
    if (DescribeModuleAndOffset(address, moduleName, offset)) {
        oss << " in " << moduleName << "+0x" << std::hex << offset;
    } else {
        oss << " (module unresolved)";
    }
    return oss.str();
}

// _set_se_translator's callback signature -- fires while the SEH is
// still active, so it can build the exception to throw using details
// from `info` (exception code, faulting address) before the stack
// unwinds.
void SehTranslatorCallback(unsigned int code, EXCEPTION_POINTERS* info) {
    void* address = (info && info->ExceptionRecord)
        ? info->ExceptionRecord->ExceptionAddress
        : nullptr;
    // A stack overflow leaves too little stack space to safely throw
    // from here -- let it fall through to InstallCrashHandlers'
    // SetUnhandledExceptionFilter (core/crash_handler.cpp) instead of
    // risking a second fault while unwinding.
    if (code == EXCEPTION_STACK_OVERFLOW) {
        return;
    }
    throw SehException(code, address);
}
} // namespace

SehException::SehException(unsigned int code, void* address)
    : std::runtime_error(DescribeSehCode(code, address)), code_(code) {}

void InstallSehTranslator() {
    _set_se_translator(SehTranslatorCallback);
}

} // namespace avahost

#endif
