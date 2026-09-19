#pragma once

#include <atomic>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "dap/dap_protocol.h"
#include "dap_transport.h"
#include "vm/vm.h"

namespace ava {
namespace dap {

struct LaunchRequest {
    std::string program;
    std::vector<std::string> args;
    bool stop_on_entry = false;
    bool no_debug = false;
};

struct ScriptHooks {
    std::function<void(ava::VM*)> on_ready;
    std::function<void()> on_closing;
    std::function<void(const std::string&)> on_error;
};

using ScriptRunner = std::function<int(const LaunchRequest&, const ScriptHooks&)>;

class DapServer {
public:
    DapServer(ScriptRunner runner, std::string default_program);
    ~DapServer();

    int Serve(int port);

private:
    using Handler = void (DapServer::*)(const DapRequest&);

    template <typename Builder>
    void Emit(Builder&& build) {
        std::lock_guard<std::mutex> lock(out_mutex_);
        transport_.Send(EncodeMessage(build(seq_)));
    }

    void Respond(const DapRequest& request, JsonValue body = JsonValue());
    void Fail(const DapRequest& request, const std::string& message);
    void SendEvent(const std::string& name, JsonValue body = JsonValue());
    void SendOutput(const std::string& category, const std::string& text);

    void Dispatch(const DapRequest& request);
    void HandleInitialize(const DapRequest& request);
    void HandleLaunch(const DapRequest& request);
    void HandleConfigurationDone(const DapRequest& request);
    void HandleSetBreakpoints(const DapRequest& request);
    void HandleSetExceptionBreakpoints(const DapRequest& request);
    void HandleThreads(const DapRequest& request);
    void HandleStackTrace(const DapRequest& request);
    void HandleScopes(const DapRequest& request);
    void HandleVariables(const DapRequest& request);
    void HandleContinue(const DapRequest& request);
    void HandleNext(const DapRequest& request);
    void HandleStepIn(const DapRequest& request);
    void HandleStepOut(const DapRequest& request);
    void HandlePause(const DapRequest& request);
    void HandleEvaluate(const DapRequest& request);
    void HandleTerminate(const DapRequest& request);
    void HandleDisconnect(const DapRequest& request);

    void Resume(const DapRequest& request, void (ava::VM::*action)(), JsonValue body);
    void MaybeStart();
    void RunDebuggee();
    void AttachVm(ava::VM* vm);
    void DetachVm();
    void OnStopped(const DapStopEvent& event);
    void Shutdown();

    int ToClientLine(int line) const;
    int FromClientLine(int line) const;
    int ToClientColumn(int column) const;

    ScriptRunner runner_;
    std::string default_program_;
    TcpServer transport_;

    std::mutex out_mutex_;
    SeqCounter seq_;

    std::mutex state_mutex_;
    ava::VM* vm_ = nullptr;
    std::map<std::string, std::vector<int>> breakpoints_;

    LaunchRequest launch_;
    bool launched_ = false;
    bool configured_ = false;
    bool started_ = false;
    bool done_ = false;
    bool lines_start_at_1_ = true;
    bool columns_start_at_1_ = true;

    std::atomic<bool> stopped_{false};
    std::atomic<bool> entry_pending_{false};
    std::atomic<bool> running_{false};
    std::thread debuggee_;
};

}  // namespace dap
}  // namespace ava
