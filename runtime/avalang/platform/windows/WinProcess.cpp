#include "WinProcess.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <mutex>
#include <thread>

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

    // CREATE_NO_WINDOW: ava_cli.exe is a console app; without this flag
    // Windows allocates and shows a brand-new console for it when launched
    // from ava_studio.exe (a GUI app with no console of its own), even
    // though stdout/stderr are already redirected into our pipes above.
    // The pipes keep capturing everything either way -- this only
    // suppresses the extra visible window.
    BOOL created = CreateProcessA(
        nullptr,
        cmd_line.data(), // must be mutable buffer
        nullptr, nullptr,
        TRUE, // inherit handles
        CREATE_NO_WINDOW, nullptr, nullptr,
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

    // Fase 7 bugfix: draining stdout_read fully and THEN stderr_read
    // (the original sequential order here) deadlocks for real -- an
    // anonymous pipe's OS buffer is finite (a few KB to ~64KB); if the
    // child writes enough to stderr while this thread is still blocked
    // in ReadFile() on stdout, the child's own WriteFile() to stderr
    // blocks (nobody is draining it), and this thread never gets more
    // stdout to unblock its read either -- both sides wait forever.
    // ExecuteStreaming() below already avoids this correctly with one
    // reader thread per pipe; do the same here instead of reading them
    // one after the other.
    std::string stdout_output;
    std::string stderr_output;
    std::thread stdout_thread([&]() { ReadPipeFully(stdout_read, stdout_output); });
    std::thread stderr_thread([&]() { ReadPipeFully(stderr_read, stderr_output); });
    stdout_thread.join();
    stderr_thread.join();
    out_result.stdout_output = std::move(stdout_output);
    out_result.stderr_output = std::move(stderr_output);

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    out_result.exit_code = static_cast<int>(exit_code);

    CloseHandle(pi.hProcess);
    CloseHandle(stdout_read);
    CloseHandle(stderr_read);

    return true;
}

bool WinProcess::ExecuteStreaming(const std::string& command, const std::vector<std::string>& args,
                                   const std::function<void(const std::string&)>& on_output,
                                   int& out_exit_code) {
    out_exit_code = -1;

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

    // CREATE_NO_WINDOW: same reasoning as Execute() above -- ava_cli.exe
    // (and git/vcpkg/bootstrap-vcpkg.bat) are console apps that would
    // otherwise pop their own console window when launched from
    // ava_studio.exe.
    BOOL created = CreateProcessA(
        nullptr,
        cmd_line.data(), // must be mutable buffer
        nullptr, nullptr,
        TRUE, // inherit handles
        CREATE_NO_WINDOW, nullptr, nullptr,
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

    // One reader thread per pipe (stdout/stderr each block independently
    // on ReadFile, so a single thread can't service both live) -- each
    // pushes every chunk it reads straight into `on_output` as soon as
    // it arrives, which is the whole point of this function versus the
    // plain Execute() above. `output_mutex` only serializes the two
    // reader threads against each other; ReadFile returning 0 bytes (or
    // failing) means the child closed that end, which happens on its
    // own once the process exits -- no separate "keep reading until
    // WaitForSingleObject says so" logic needed.
    std::mutex output_mutex;
    auto pump_pipe = [&](HANDLE pipe) {
        char buffer[4096];
        for (;;) {
            DWORD bytes_read = 0;
            BOOL ok = ReadFile(pipe, buffer, sizeof(buffer), &bytes_read, nullptr);
            if (!ok || bytes_read == 0) {
                break;
            }
            std::lock_guard<std::mutex> lock(output_mutex);
            on_output(std::string(buffer, bytes_read));
        }
    };

    std::thread stdout_thread([&]() { pump_pipe(stdout_read); });
    std::thread stderr_thread([&]() { pump_pipe(stderr_read); });

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    out_exit_code = static_cast<int>(exit_code);
    CloseHandle(pi.hProcess);

    // The child exiting closes its inherited pipe-write handles, which is
    // what makes each pump_pipe()'s ReadFile finally return 0 and let
    // these threads fall out of their loop on their own -- so these
    // joins don't hang waiting on anything beyond that natural EOF.
    stdout_thread.join();
    stderr_thread.join();

    CloseHandle(stdout_read);
    CloseHandle(stderr_read);

    return true;
}

} // namespace windows
} // namespace platform
} // namespace ava
