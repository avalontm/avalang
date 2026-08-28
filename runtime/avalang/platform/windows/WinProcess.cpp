#include "WinProcess.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

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

std::string BuildCommandLine(const std::string& command, const std::vector<std::string>& args) {
    std::string cmd_line = QuoteArgIfNeeded(command);
    for (const auto& arg : args) {
        cmd_line += " ";
        cmd_line += QuoteArgIfNeeded(arg);
    }
    return cmd_line;
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

// Bugfix (handle-inheritance hang): plain CreateProcessA(..., TRUE, ...) doesn't only
// hand the child the two pipe ends we put in STARTUPINFO -- it duplicates *every*
// inheritable handle currently open in this process into the child. Execute()/
// ExecuteStreaming() sit at every hop of ava_studio.exe -> ava_cli.exe -> cmake.exe ->
// ninja.exe -> cl.exe/i686-elf-gcc.exe, so that uncontrolled inheritance compounds at
// each level: a background helper several levels down (MSVC's mspdbsrv.exe is the
// textbook case, but any lingering compiler worker qualifies) can end up holding a live
// duplicate of the *original* stdout_write/stderr_write handle from the very first
// CreatePipe() call at the top of the chain. An anonymous pipe only reaches EOF once
// every duplicate of its write end, in every process, is closed -- so as long as that
// helper is alive, ReadFile() in our reader threads blocks forever even though the
// child we actually launched (ava_cli.exe/cmake.exe) is long gone, and the caller (e.g.
// BuildPanelState::worker) never comes back from stdout_thread.join()/stderr_thread.join()
// -- which is what surfaces to the user as AvaStudio "never finishing" a build.
//
// PROC_THREAD_ATTRIBUTE_HANDLE_LIST restricts inheritance to exactly the handles listed
// here (stdout_write/stderr_write, plus a real console stdin if one exists), cutting off
// that propagation at *this* hop. It doesn't stop a build tool's own legitimate
// grandchildren from doing the same thing internally (mspdbsrv.exe is spawned by
// cl.exe/link.exe on purpose) -- see the grace-period fallback below for that half of
// the fix.
struct RestrictedAttributeList {
    std::vector<uint8_t> buffer;
    LPPROC_THREAD_ATTRIBUTE_LIST list = nullptr;

    bool Init(std::vector<HANDLE>& handles) {
        SIZE_T size = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
        if (size == 0) return false;
        buffer.resize(size);
        list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(buffer.data());
        if (!InitializeProcThreadAttributeList(list, 1, 0, &size)) {
            list = nullptr;
            return false;
        }
        if (!UpdateProcThreadAttribute(list, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, handles.data(),
                                        handles.size() * sizeof(HANDLE), nullptr, nullptr)) {
            DeleteProcThreadAttributeList(list);
            list = nullptr;
            return false;
        }
        return true;
    }

    ~RestrictedAttributeList() {
        if (list) DeleteProcThreadAttributeList(list);
    }
};

// Launches `command`/`args` with stdout/stderr redirected to the given pipe write ends,
// restricting handle inheritance to just those (+ a real console stdin, if any) via
// RestrictedAttributeList above. Falls back to old-style unrestricted inheritance only
// if the attribute-list machinery itself fails to initialize (very old Windows or an
// OS-level allocation failure), so a launch never breaks outright over this -- it just
// loses the extra protection for that one call.
// `stdin_read`, if non-null, is a pipe read-end WE created (ExecuteStreaming's own
// stdin pipe, see below) that the child should inherit as its stdin instead of
// whatever real console handle ava_studio.exe itself has (almost always none -- it's
// a GUI app). Pass nullptr to fall back to the old behavior (Execute()'s case, which
// has no interactive-input caller and no pipe to offer).
bool LaunchRedirected(const std::string& command, const std::vector<std::string>& args, HANDLE stdout_write,
                       HANDLE stderr_write, HANDLE stdin_read, PROCESS_INFORMATION& pi) {
    std::string cmd_line = BuildCommandLine(command, args);

    HANDLE stdin_handle;
    bool stdin_valid;
    if (stdin_read != nullptr) {
        stdin_handle = stdin_read;
        stdin_valid = true;
    } else {
        // GetStdHandle(STD_INPUT_HANDLE) is NULL for ava_studio.exe in the normal case
        // (it's a GUI app with no console of its own) -- only wire it up, and only add
        // it to the inherit whitelist, when it's a real handle.
        stdin_handle = GetStdHandle(STD_INPUT_HANDLE);
        stdin_valid = (stdin_handle != nullptr && stdin_handle != INVALID_HANDLE_VALUE);
        if (stdin_valid) {
            SetHandleInformation(stdin_handle, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
        }
    }

    std::vector<HANDLE> inherit_list = {stdout_write, stderr_write};
    if (stdin_valid) inherit_list.push_back(stdin_handle);

    RestrictedAttributeList attrs;
    const bool restricted = attrs.Init(inherit_list);

    STARTUPINFOEXA si_ex{};
    si_ex.StartupInfo.cb = sizeof(si_ex.StartupInfo);
    si_ex.StartupInfo.dwFlags |= STARTF_USESTDHANDLES;
    si_ex.StartupInfo.hStdOutput = stdout_write;
    si_ex.StartupInfo.hStdError = stderr_write;
    si_ex.StartupInfo.hStdInput = stdin_valid ? stdin_handle : nullptr;
    if (restricted) si_ex.lpAttributeList = attrs.list;

    // CREATE_NO_WINDOW: ava_cli.exe (and git/vcpkg/bootstrap-vcpkg.bat) are console apps;
    // without this flag Windows allocates and shows a brand-new console for them when
    // launched from ava_studio.exe (a GUI app with no console of its own), even though
    // stdout/stderr are already redirected into our pipes. The pipes keep capturing
    // everything either way -- this only suppresses the extra visible window.
    DWORD creation_flags = CREATE_NO_WINDOW;
    if (restricted) creation_flags |= EXTENDED_STARTUPINFO_PRESENT;

    pi = PROCESS_INFORMATION{};
    BOOL created = CreateProcessA(nullptr,
                                   cmd_line.data(),  // must be mutable buffer
                                   nullptr, nullptr,
                                   TRUE,  // bInheritHandles -- with `restricted` set, the
                                          // PROC_THREAD_ATTRIBUTE_HANDLE_LIST above narrows
                                          // what that actually means to just inherit_list.
                                   creation_flags, nullptr, nullptr,
                                   reinterpret_cast<LPSTARTUPINFOA>(&si_ex), &pi);

    return created != FALSE;
}

// Grace period (handle-inheritance hang, part 2): restricting inheritance above stops
// *our own* process from leaking the pipe handle further down, but it can't reach into
// a build tool's own subprocess tree -- cl.exe/link.exe spawn mspdbsrv.exe on purpose
// and hand it their own (legitimately inherited) copy of the handle, and mspdbsrv.exe
// is designed to persist in the background to serve later builds. That's out of our
// control. So once the child we actually launched has exited, give the reader threads a
// short window to drain naturally; if they're still stuck after that, stop trusting
// ReadFile() to see EOF and force it to return via CancelIoEx() on the read handles
// instead. Worst case this drops the last few lines of output from an orphaned helper
// process -- far better than a build that "never finishes" from the user's side.
constexpr auto kReaderDrainGrace = std::chrono::milliseconds(3000);

struct ReaderSync {
    std::mutex mtx;
    std::condition_variable cv;
    bool stdout_done = false;
    bool stderr_done = false;
};

void MarkDone(ReaderSync& sync, bool ReaderSync::*flag) {
    {
        std::lock_guard<std::mutex> lock(sync.mtx);
        sync.*flag = true;
    }
    sync.cv.notify_all();
}

void JoinReadersWithGrace(ReaderSync& sync, std::thread& stdout_thread, std::thread& stderr_thread,
                           HANDLE stdout_read, HANDLE stderr_read) {
    std::unique_lock<std::mutex> lock(sync.mtx);
    const bool drained =
        sync.cv.wait_for(lock, kReaderDrainGrace, [&] { return sync.stdout_done && sync.stderr_done; });
    lock.unlock();

    if (!drained) {
        // Something downstream is still holding a duplicate of the write end open --
        // stop waiting on an EOF that may never come and force the pending ReadFile()
        // calls to return (with an error, which the reader loops already treat as "done").
        CancelIoEx(stdout_read, nullptr);
        CancelIoEx(stderr_read, nullptr);
    }
    stdout_thread.join();
    stderr_thread.join();
}

// Wraps the parent's end of a stdin pipe we created for the child in
// ExecuteStreaming(). Handed out via `on_started` as an IStdinWriter (see
// IProcessStream.h) so a caller on some *other* thread (Ava Studio's UI
// thread) can feed the child's stdin while ExecuteStreaming's own thread
// is still blocked waiting on the process. `mutex_` guards against a
// WriteLine() racing Close() -- called once the child has exited, from
// ExecuteStreaming's thread -- rather than against the two reader threads
// (stdout_read/stderr_read are a completely separate pipe/handle pair).
class WinStdinWriter : public ava::platform::IProcessStream::IStdinWriter {
public:
    explicit WinStdinWriter(HANDLE write_handle) : handle_(write_handle) {}

    bool WriteLine(const avastd::string& line) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (handle_ == nullptr) return false;  // already closed -- child is gone
        std::string buf = line;
        buf += '\n';
        DWORD written = 0;
        BOOL ok = WriteFile(handle_, buf.data(), static_cast<DWORD>(buf.size()), &written, nullptr);
        return ok != FALSE;
    }

    // Called once, from ExecuteStreaming's own thread, right after the
    // child has exited -- keeping the write handle open past that point
    // serves no purpose (nothing will ever read it again) and would leak
    // it if the caller drops its shared_ptr without another WriteLine to
    // notice the handle is stale.
    void Close() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (handle_ != nullptr) {
            CloseHandle(handle_);
            handle_ = nullptr;
        }
    }

private:
    std::mutex mutex_;
    HANDLE handle_;
};

}  // namespace

uint64_t WinProcess::CurrentProcessId() const { return static_cast<uint64_t>(GetCurrentProcessId()); }

bool WinProcess::Execute(const std::string& command, const std::vector<std::string>& args,
                          ProcessResult& out_result) {
    out_result = ProcessResult{};

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

    PROCESS_INFORMATION pi{};
    const bool created = LaunchRedirected(command, args, stdout_write, stderr_write, nullptr, pi);

    // Parent no longer needs the write ends once the child has inherited them.
    CloseHandle(stdout_write);
    CloseHandle(stderr_write);

    if (!created) {
        CloseHandle(stdout_read);
        CloseHandle(stderr_read);
        return false;
    }

    CloseHandle(pi.hThread);

    // One reader thread per pipe -- an anonymous pipe's OS buffer is finite (a few KB to
    // ~64KB); reading stdout_read fully and THEN stderr_read (sequentially) can deadlock
    // for real if the child writes enough to stderr while this thread is still blocked
    // reading stdout: the child's own WriteFile() to stderr blocks (nobody is draining
    // it), and this thread never gets more stdout to unblock its read either.
    std::string stdout_output;
    std::string stderr_output;
    ReaderSync sync;
    std::thread stdout_thread([&]() {
        ReadPipeFully(stdout_read, stdout_output);
        MarkDone(sync, &ReaderSync::stdout_done);
    });
    std::thread stderr_thread([&]() {
        ReadPipeFully(stderr_read, stderr_output);
        MarkDone(sync, &ReaderSync::stderr_done);
    });

    // Wait on the process we actually launched first -- this is the wait that matters,
    // and (unlike the reader threads) it's guaranteed to return once ava_cli.exe/cmake.exe
    // itself exits, regardless of what any grandchild does with our pipe handles.
    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    out_result.exit_code = static_cast<int>(exit_code);
    CloseHandle(pi.hProcess);

    JoinReadersWithGrace(sync, stdout_thread, stderr_thread, stdout_read, stderr_read);

    out_result.stdout_output = std::move(stdout_output);
    out_result.stderr_output = std::move(stderr_output);

    CloseHandle(stdout_read);
    CloseHandle(stderr_read);

    return true;
}

bool WinProcess::ExecuteStreaming(const std::string& command, const std::vector<std::string>& args,
                                   const std::function<void(const std::string&)>& on_output, int& out_exit_code,
                                   const std::function<void(avastd::shared_ptr<IStdinWriter>)>& on_started) {
    out_exit_code = -1;

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    HANDLE stdout_read = nullptr, stdout_write = nullptr;
    HANDLE stderr_read = nullptr, stderr_write = nullptr;
    HANDLE stdin_read = nullptr, stdin_write = nullptr;

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
    // Our own stdin pipe -- CreatePipe(sa) makes BOTH ends inheritable by default
    // (same `sa` as stdout/stderr above); stdin_read is meant to be inherited by the
    // child (added to inherit_list below) so that's fine as-is, but stdin_write is
    // ours to keep and write to later, so strip inheritance from it the same way
    // stdout_read/stderr_read (the ends WE read from) have theirs stripped above.
    if (!CreatePipe(&stdin_read, &stdin_write, &sa, 0) ||
        !SetHandleInformation(stdin_write, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(stdout_read);
        CloseHandle(stdout_write);
        CloseHandle(stderr_read);
        CloseHandle(stderr_write);
        return false;
    }

    PROCESS_INFORMATION pi{};
    const bool created = LaunchRedirected(command, args, stdout_write, stderr_write, stdin_read, pi);

    // Parent no longer needs the write ends -- or the child's stdin read end -- once
    // the child has inherited them.
    CloseHandle(stdout_write);
    CloseHandle(stderr_write);
    CloseHandle(stdin_read);

    if (!created) {
        CloseHandle(stdout_read);
        CloseHandle(stderr_read);
        CloseHandle(stdin_write);
        return false;
    }

    CloseHandle(pi.hThread);

    // Hand the caller a way to write to the child's stdin from any thread (typically
    // the UI thread, echoing what the user types into the console's input box) --
    // must happen before WaitForSingleObject below, since that's the whole point:
    // the child may block reading stdin (input()) long before it exits.
    auto stdin_writer = avastd::make_shared<WinStdinWriter>(stdin_write);
    if (on_started) on_started(stdin_writer);

    // One reader thread per pipe (stdout/stderr each block independently on ReadFile, so
    // a single thread can't service both live) -- each pushes every chunk it reads
    // straight into `on_output` as soon as it arrives, which is the whole point of this
    // function versus the plain Execute() above. `output_mutex` only serializes the two
    // reader threads against each other.
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

    ReaderSync sync;
    std::thread stdout_thread([&]() {
        pump_pipe(stdout_read);
        MarkDone(sync, &ReaderSync::stdout_done);
    });
    std::thread stderr_thread([&]() {
        pump_pipe(stderr_read);
        MarkDone(sync, &ReaderSync::stderr_done);
    });

    // Wait on the process we actually launched first -- see the comment in Execute()
    // above; ReadFile() seeing EOF on our pipes is a separate, weaker guarantee that a
    // lingering grandchild (mspdbsrv.exe & co.) can delay indefinitely, which is exactly
    // what JoinReadersWithGrace below is for.
    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    out_exit_code = static_cast<int>(exit_code);
    CloseHandle(pi.hProcess);

    // The child is gone -- nothing will ever read stdin_write again, and a
    // WriteLine() arriving after this point (e.g. the user hits Enter a moment too
    // late) must fail cleanly instead of writing into a handle that could, once
    // closed and its value reused by something unrelated, mean something else.
    stdin_writer->Close();

    JoinReadersWithGrace(sync, stdout_thread, stderr_thread, stdout_read, stderr_read);

    CloseHandle(stdout_read);
    CloseHandle(stderr_read);

    return true;
}

}  // namespace windows
}  // namespace platform
}  // namespace ava
