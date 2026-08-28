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

#include "../barekernel/stdcompat/ava_stdcompat.h"

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

    // Lets a caller feed a running child's stdin -- e.g. Ava Studio's
    // Terminal panel forwarding whatever the user types into its console
    // input box while a script (ava_cli) is blocked inside an `input()`
    // call. Handed to the caller via ExecuteStreaming's `on_started`
    // below once the child is actually running.
    class IStdinWriter {
    public:
        virtual ~IStdinWriter() = default;

        // Writes `line` + '\n' to the child's stdin. Unlike `on_output`
        // above, this is meant to be called from a DIFFERENT thread than
        // the one blocked inside ExecuteStreaming (typically the UI
        // thread, in response to the user pressing Enter) -- 
        // implementations must make that safe on their own (e.g. the
        // write handle/fd is independent of the ones the reader threads
        // use, so no shared state needs locking beyond guarding against a
        // write racing the child's exit). Returns false if the write
        // failed (most commonly: the child already exited and closed its
        // end) -- callers should treat that as "nobody is listening"
        // rather than surface it as an error.
        virtual bool WriteLine(const avastd::string& line) = 0;
    };

    // `on_started`, if non-null, is invoked once (synchronously, from
    // ExecuteStreaming's own thread) right after the child process has
    // launched, with a stdin writer the caller can stash and call later
    // -- from any thread -- for as long as the child may still be
    // reading its stdin. Pass nullptr (the default) when the caller has
    // no interactive input to send; implementations that can't offer a
    // stdin pipe for some reason may also call it with nullptr, which
    // callers must treat the same as "not offered" rather than a bug.
    virtual bool ExecuteStreaming(const avastd::string& command, const avastd::vector<avastd::string>& args,
                                   const avastd::function<void(const avastd::string&)>& on_output,
                                   int& out_exit_code,
                                   const avastd::function<void(avastd::shared_ptr<IStdinWriter>)>& on_started = nullptr) = 0;
};

} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_IPROCESS_STREAM_H
