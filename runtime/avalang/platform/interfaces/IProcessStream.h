#ifndef AVA_PLATFORM_IPROCESS_STREAM_H
#define AVA_PLATFORM_IPROCESS_STREAM_H

// NOT part of the frozen PAL_ABI contract (see PAL_ABI.h) -- IProcess's
// own Execute() signature is stable and cannot change or gain a new
// virtual method. This is a purely additive second interface a backend
// can *also* implement (see WinProcess: public IProcess, public
// IProcessStream) instead of touching IProcess itself. Callers that want
// live output dynamic_cast<IProcessStream*> the IProcess& they already
// have from IPlatform::Process() and fall back to the old blocking
// Execute() if that comes back null (e.g. on the Linux/Mac stub
// backends, which don't implement this yet).

#include <functional>
#include <string>
#include <vector>

namespace ava {
namespace platform {

// Same job as IProcess::Execute, but invokes `on_output` with each chunk
// of stdout/stderr as the child process actually produces it, instead of
// silently buffering everything until the process exits and handing it
// all back at once. Built for callers that want to show a long-running
// command's output live (e.g. Ava Studio's Build panel and its "Install
// vcpkg" button) rather than as a single dump at the end.
//
// `on_output` is invoked from whatever thread ExecuteStreaming() itself
// runs on (i.e. the caller's own background worker, not a new thread the
// caller has to manage) -- implementations serialize calls into it (a
// single reader at a time), so it does not need to be reentrant, but it
// must still be safe to call from a non-UI thread (e.g. push behind a
// mutex) and must return quickly, or it will stall the pipe reader and
// the child process along with it if its output pipe fills up.
class IProcessStream {
public:
    virtual ~IProcessStream() = default;

    virtual bool ExecuteStreaming(const std::string& command, const std::vector<std::string>& args,
                                   const std::function<void(const std::string&)>& on_output,
                                   int& out_exit_code) = 0;
};

} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_IPROCESS_STREAM_H
