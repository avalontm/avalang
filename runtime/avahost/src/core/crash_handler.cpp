#include "core/crash_handler.h"

#include <csignal>
#include <cstdio>
#include <exception>
#include <sstream>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace avahost {

namespace {

// std::set_terminate's handler and (on Windows) the unhandled-exception
// filter both take no user-supplied context, so the logger to write to
// has to live in a global. Only ever set once, from InstallCrashHandlers,
// before any of these handlers can possibly fire.
Logger* g_crashLogger = nullptr;

void LogFatal(const std::string& message) {
    if (g_crashLogger) {
        g_crashLogger->Error("FATAL: " + message);
    } else {
        // Should not happen (InstallCrashHandlers always sets
        // g_crashLogger first) but never silently swallow a crash
        // reason if it somehow does.
        std::fprintf(stderr, "FATAL: %s\n", message.c_str());
        std::fflush(stderr);
    }
}

// Fires for any exception (std::exception or not) that escapes every
// try/catch on its way up -- most relevantly, one thrown on
// watcherThread_ (hot-reload polling) or during plugin/startup code,
// neither of which run under HttpServer::HandleConnection's try/catch or
// the SEH translator scoped to it. An uncaught exception on *any*
// thread calls std::terminate, which by default aborts with no message
// at all -- this replaces that default with one that at least says why.
[[noreturn]] void OnTerminate() {
    if (auto ex = std::current_exception()) {
        try {
            std::rethrow_exception(ex);
        } catch (const std::exception& e) {
            LogFatal(std::string("unhandled exception outside request handling: ") + e.what());
        } catch (...) {
            LogFatal("unhandled non-std exception outside request handling");
        }
    } else {
        LogFatal("std::terminate called with no active exception "
                 "(likely a noexcept violation, pure virtual call, or double-throw)");
    }
    std::abort();
}

#if defined(_WIN32)
std::string HexAddress(void* p) {
    std::ostringstream oss;
    oss << "0x" << std::hex << reinterpret_cast<uintptr_t>(p);
    return oss.str();
}

const char* ExceptionCodeName(DWORD code) {
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION:      return "ACCESS_VIOLATION";
        case EXCEPTION_STACK_OVERFLOW:        return "STACK_OVERFLOW";
        case EXCEPTION_INT_DIVIDE_BY_ZERO:    return "INT_DIVIDE_BY_ZERO";
        case EXCEPTION_ILLEGAL_INSTRUCTION:   return "ILLEGAL_INSTRUCTION";
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "ARRAY_BOUNDS_EXCEEDED";
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:    return "FLT_DIVIDE_BY_ZERO";
        default:                              return "UNKNOWN";
    }
}

// Last-resort catch-all: anything reaching here got past the SEH
// translator scoped to request handling (see seh_guard.h) too -- most
// likely a stack overflow (the translator's own frame may not have room
// to run) or a crash on a thread other than the request-handling one
// (e.g. watcherThread_). Logs what Windows knows about the fault --
// exception code, faulting address -- then lets the default handler run
// (EXCEPTION_EXECUTE_HANDLER), which is what actually terminates the
// process; this filter's only job is to get one line of context into
// the log first.
LONG WINAPI OnUnhandledException(EXCEPTION_POINTERS* info) {
    if (info && info->ExceptionRecord) {
        DWORD code = info->ExceptionRecord->ExceptionCode;
        std::ostringstream oss;
        oss << "unhandled native exception 0x" << std::hex << code
            << " (" << ExceptionCodeName(code) << ")"
            << " at address " << HexAddress(info->ExceptionRecord->ExceptionAddress);
        if (code == EXCEPTION_ACCESS_VIOLATION &&
            info->ExceptionRecord->NumberParameters >= 2) {
            bool isWrite = info->ExceptionRecord->ExceptionInformation[0] != 0;
            oss << " -- " << (isWrite ? "write" : "read") << " to "
                << HexAddress(reinterpret_cast<void*>(info->ExceptionRecord->ExceptionInformation[1]));
        }
        LogFatal(oss.str());
    } else {
        LogFatal("unhandled native exception (no exception record available)");
    }
    return EXCEPTION_EXECUTE_HANDLER;
}
#else
void OnFatalSignal(int sig) {
    // async-signal-safety is best-effort here: this is a last resort for
    // a build that's already crashing, not a hot path -- getting a log
    // line out is more valuable than being perfectly signal-safe.
    const char* name =
        sig == SIGSEGV ? "SIGSEGV (segmentation fault)" :
        sig == SIGABRT ? "SIGABRT" :
        sig == SIGFPE  ? "SIGFPE (arithmetic error)" :
        sig == SIGILL  ? "SIGILL (illegal instruction)" : "unknown signal";
    LogFatal(std::string("fatal signal: ") + name);
    std::signal(sig, SIG_DFL);
    std::raise(sig);
}
#endif

} // namespace

void InstallCrashHandlers(Logger& logger) {
    g_crashLogger = &logger;
    std::set_terminate(OnTerminate);
#if defined(_WIN32)
    SetUnhandledExceptionFilter(OnUnhandledException);
#else
    std::signal(SIGSEGV, OnFatalSignal);
    std::signal(SIGABRT, OnFatalSignal);
    std::signal(SIGFPE, OnFatalSignal);
    std::signal(SIGILL, OnFatalSignal);
#endif
}

} // namespace avahost
