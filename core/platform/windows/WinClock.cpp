#include "WinClock.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace ava {
namespace platform {
namespace windows {

namespace {
// 100ns intervals between 1601-01-01 (FILETIME epoch) and 1970-01-01 (Unix epoch).
constexpr int64_t kFileTimeToUnixEpochOffset100Ns = 116444736000000000LL;
}

int64_t WinClock::NowMs() const {
    FILETIME ft{};
    GetSystemTimeAsFileTime(&ft);

    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;

    int64_t hundred_ns_since_unix_epoch =
        static_cast<int64_t>(uli.QuadPart) - kFileTimeToUnixEpochOffset100Ns;

    return hundred_ns_since_unix_epoch / 10000; // 100ns -> ms
}

int64_t WinClock::HighResNowNs() const {
    static LARGE_INTEGER frequency = [] {
        LARGE_INTEGER f{};
        QueryPerformanceFrequency(&f);
        return f;
    }();

    LARGE_INTEGER counter{};
    QueryPerformanceCounter(&counter);

    // Avoid overflow: split into whole seconds + remainder before scaling to ns.
    int64_t seconds = counter.QuadPart / frequency.QuadPart;
    int64_t remainder = counter.QuadPart % frequency.QuadPart;

    return seconds * 1000000000LL + (remainder * 1000000000LL) / frequency.QuadPart;
}

void WinClock::SleepMs(uint32_t milliseconds) {
    ::Sleep(static_cast<DWORD>(milliseconds));
}

} // namespace windows
} // namespace platform
} // namespace ava
