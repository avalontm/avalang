#pragma once
// Windows-only. A bug inside avahost's rendering pipeline or inside the
// AvaLang VM itself (ava_compile/ava_run, called through a plain C API --
// see runtime/runtime_host.cpp) can fault with an actual access
// violation rather than throwing a C++ exception. A structured exception
// like that is *not* a std::exception and is invisible to an ordinary
// try/catch (const std::exception&) -- it unwinds straight past
// HttpServer::HandleConnection's existing try/catch (web/server/
// http_server.cpp) and takes the whole avahost.exe process down with it,
// with nothing logged, which matches "the site closes with no error in
// the log" exactly.
//
// InstallSehTranslator (called once, from the request-handling thread,
// before HttpServer::Run's accept loop starts) uses MSVC's
// _set_se_translator to convert a Win32 structured exception into a
// throw of SehException -- a real C++ exception the existing
// `catch (const std::exception&)` around `handler(request)` can catch
// like any other. That turns "the whole process vanishes on a bad
// request" into "this one request returns 500 and gets logged with
// which exception code/address caused it", while every other client
// connection keeps being served normally.
//
// Requires the translating call site to be compiled with /EHa (MSVC) --
// see CMakeLists.txt, which scopes that flag to
// src/web/server/http_server.cpp only, not the whole target (broader
// /EHa hides real logic bugs behind a "everything might secretly
// unwind" model, so it's kept to the one file that actually needs it).
#include <stdexcept>
#include <string>

namespace avahost {

#if defined(_WIN32)

class SehException : public std::runtime_error {
public:
    SehException(unsigned int code, void* address);
    unsigned int code() const { return code_; }

private:
    unsigned int code_;
};

// Installs the translator for the *calling thread only* (an MSVC
// _set_se_translator registration is per-thread, not process-wide).
// AvaHost's request-handling loop is single-threaded (HttpServer::Run's
// blocking accept loop), so one call from that thread before the loop
// starts covers every request.
void InstallSehTranslator();

#else

// Non-Windows builds have no SEH; give callers a harmless no-op so
// http_server.cpp doesn't need #ifdefs at every call site. (AvaHost's
// Linux/macOS path is currently paused/stub anyway -- see
// CMakeLists.txt.)
inline void InstallSehTranslator() {}

#endif

} // namespace avahost
