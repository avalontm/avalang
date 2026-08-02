#include "WinProcess.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace ava {
namespace platform {
namespace windows {

namespace {

std::string QuoteArgIfNeeded(const std::string& arg) {
    if (arg.find(' ') == std::string::npos && !arg.empty()) {
        return arg;
    }
    std::string quoted = "\"";
    quoted += arg;
    quoted += "\"";
    return quoted;
}

void ReadPipeFully(HANDLE pipe_read, std::string& out) {
    char buffer[4096];
    for (;;) {
        DWORD bytes_read = 0;
        BOOL ok = ReadFile(pipe_read, buffer, sizeof(buffer), &bytes_read, nullptr);
        if (!ok || bytes_read == 0) {
            break;
        }
        out.append(buffer, bytes_read);
    }
}

} // namespace

uint64_t WinProcess::CurrentProcessId() const {
    return static_cast<uint64_t>(GetCurrentProcessId());
}

bool WinProcess::Execute(const std::string& command,
                          const std::vector<std::string>& args,
                          ProcessResult& out_result) {
    out_result = ProcessResult{};

    std::string cmd_line = QuoteArgIfNeeded(command);
    for (const auto& arg : args) {
        cmd_line += " ";
        cmd_line += QuoteArgIfNeeded(arg);
    }

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    HANDLE stdout_read = nullptr, stdout_write = nullptr;
    HANDLE stderr_read = nullptr, stderr_write = nullptr;

    if (!CreatePipe(&stdout_read, &stdout_write, &sa, 0) ||
        !SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0)) {
        return false;
    }
    if (!CreatePipe(&stderr_read, &stderr_write, &sa, 0) ||
        !SetHandleInformation(stderr_read, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(stdout_read);
        CloseHandle(stdout_write);
        return false;
    }

    STARTUPINFOA si{};
    si.cb = sizeof(STARTUPINFOA);
    si.dwFlags |= STARTF_USESTDHANDLES;
    si.hStdOutput = stdout_write;
    si.hStdError = stderr_write;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi{};

    BOOL created = CreateProcessA(
        nullptr,
        cmd_line.data(), // must be mutable buffer
        nullptr, nullptr,
        TRUE, // inherit handles
        0, nullptr, nullptr,
        &si, &pi);

    // Parent no longer needs the write ends once the child has inherited them.
    CloseHandle(stdout_write);
    CloseHandle(stderr_write);

    if (!created) {
        CloseHandle(stdout_read);
        CloseHandle(stderr_read);
        return false;
    }

    CloseHandle(pi.hThread);

    ReadPipeFully(stdout_read, out_result.stdout_output);
    ReadPipeFully(stderr_read, out_result.stderr_output);

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    out_result.exit_code = static_cast<int>(exit_code);

    CloseHandle(pi.hProcess);
    CloseHandle(stdout_read);
    CloseHandle(stderr_read);

    return true;
}

} // namespace windows
} // namespace platform
} // namespace ava
