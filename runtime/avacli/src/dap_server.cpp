#include "dap_server.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <unordered_map>
#include <utility>

namespace ava {
namespace dap {

namespace {

constexpr int kMainThreadId = 1;

JsonValue Obj() { return JsonValue::MakeObject(); }
JsonValue Arr() { return JsonValue::MakeArray(); }

std::string AbsolutePath(const std::string& path) {
    if (path.empty()) return path;
    std::error_code ec;
    std::filesystem::path absolute = std::filesystem::absolute(std::filesystem::path(path), ec);
    return ec ? path : absolute.lexically_normal().string();
}

JsonValue MakeSource(const std::string& raw) {
    JsonValue source = Obj();
    source.set("name", JsonValue(std::filesystem::path(raw).filename().string()));
    source.set("path", JsonValue(AbsolutePath(raw)));
    return source;
}

const char* ReasonName(DapStopReason reason) {
    switch (reason) {
        case DapStopReason::Breakpoint: return "breakpoint";
        case DapStopReason::Step: return "step";
        case DapStopReason::Pause: return "pause";
        case DapStopReason::Entry: return "entry";
    }
    return "pause";
}

}  // namespace

DapServer::DapServer(ScriptRunner runner, std::string default_program)
    : runner_(std::move(runner)), default_program_(std::move(default_program)) {}

DapServer::~DapServer() {
    if (debuggee_.joinable() && !running_) debuggee_.join();
}

int DapServer::Serve(int port) {
    std::string error;
    if (!transport_.Listen(port, error)) {
        std::fprintf(stderr, "error: cannot listen on port %d: %s\n", port, error.c_str());
        return 1;
    }
    std::printf("DAP server listening on 127.0.0.1:%d\n", transport_.BoundPort());
    std::fflush(stdout);

    if (!transport_.AcceptClient(error)) {
        std::fprintf(stderr, "error: accept failed: %s\n", error.c_str());
        return 1;
    }

    std::string buffer;
    char chunk[4096];
    while (!done_) {
        long received = transport_.Receive(chunk, sizeof(chunk));
        if (received <= 0) break;
        buffer.append(chunk, static_cast<size_t>(received));

        while (!done_) {
            std::optional<JsonValue> message;
            try {
                message = TryExtractMessage(buffer);
            } catch (const JsonParseError& e) {
                std::fprintf(stderr, "dap: malformed message: %s\n", e.what());
                continue;
            }
            if (!message) break;
            if (IsRequest(*message)) Dispatch(ParseRequest(*message));
        }
    }

    Shutdown();
    return 0;
}

void DapServer::Shutdown() {
    if (running_) {
        transport_.Close();
        std::fflush(stdout);
        std::fflush(stderr);
        std::_Exit(0);
    }
    if (debuggee_.joinable()) debuggee_.join();
    transport_.Close();
}

void DapServer::Respond(const DapRequest& request, JsonValue body) {
    Emit([&](SeqCounter& seq) { return MakeResponse(seq, request, std::move(body)); });
}

void DapServer::Fail(const DapRequest& request, const std::string& message) {
    Emit([&](SeqCounter& seq) { return MakeErrorResponse(seq, request, message); });
}

void DapServer::SendEvent(const std::string& name, JsonValue body) {
    Emit([&](SeqCounter& seq) { return MakeEvent(seq, name, std::move(body)); });
}

void DapServer::SendOutput(const std::string& category, const std::string& text) {
    JsonValue body = Obj();
    body.set("category", JsonValue(category));
    body.set("output", JsonValue(text));
    SendEvent("output", std::move(body));
}

int DapServer::ToClientLine(int line) const {
    return lines_start_at_1_ ? line : line - 1;
}

int DapServer::FromClientLine(int line) const {
    return lines_start_at_1_ ? line : line + 1;
}

int DapServer::ToClientColumn(int column) const {
    int one_based = std::max(column, 1);
    return columns_start_at_1_ ? one_based : one_based - 1;
}

void DapServer::Dispatch(const DapRequest& request) {
    static const std::unordered_map<std::string, Handler> handlers = {
        {"initialize", &DapServer::HandleInitialize},
        {"launch", &DapServer::HandleLaunch},
        {"configurationDone", &DapServer::HandleConfigurationDone},
        {"setBreakpoints", &DapServer::HandleSetBreakpoints},
        {"setExceptionBreakpoints", &DapServer::HandleSetExceptionBreakpoints},
        {"threads", &DapServer::HandleThreads},
        {"stackTrace", &DapServer::HandleStackTrace},
        {"scopes", &DapServer::HandleScopes},
        {"variables", &DapServer::HandleVariables},
        {"continue", &DapServer::HandleContinue},
        {"next", &DapServer::HandleNext},
        {"stepIn", &DapServer::HandleStepIn},
        {"stepOut", &DapServer::HandleStepOut},
        {"pause", &DapServer::HandlePause},
        {"evaluate", &DapServer::HandleEvaluate},
        {"terminate", &DapServer::HandleTerminate},
        {"disconnect", &DapServer::HandleDisconnect},
    };

    auto it = handlers.find(request.command);
    if (it == handlers.end()) {
        Fail(request, "unsupported request: " + request.command);
        return;
    }
    (this->*(it->second))(request);
}

void DapServer::HandleInitialize(const DapRequest& request) {
    lines_start_at_1_ = request.arguments.get("linesStartAt1").as_bool(true);
    columns_start_at_1_ = request.arguments.get("columnsStartAt1").as_bool(true);

    JsonValue capabilities = Obj();
    capabilities.set("supportsConfigurationDoneRequest", JsonValue(true));
    capabilities.set("supportsTerminateRequest", JsonValue(true));
    capabilities.set("supportsEvaluateForHovers", JsonValue(false));
    capabilities.set("supportsStepBack", JsonValue(false));
    capabilities.set("supportsSetVariable", JsonValue(false));
    Respond(request, std::move(capabilities));
    SendEvent("initialized");
}

void DapServer::HandleLaunch(const DapRequest& request) {
    const JsonValue& arguments = request.arguments;

    LaunchRequest launch;
    launch.program = arguments.get("program").as_string();
    if (launch.program.empty()) launch.program = default_program_;
    if (launch.program.empty()) {
        Fail(request, "launch requires a program");
        return;
    }
    for (const JsonValue& item : arguments.get("args").as_array()) {
        launch.args.push_back(item.as_string());
    }
    launch.stop_on_entry = arguments.get("stopOnEntry").as_bool(false);
    launch.no_debug = arguments.get("noDebug").as_bool(false);

    launch_ = std::move(launch);
    launched_ = true;
    Respond(request);
    MaybeStart();
}

void DapServer::HandleConfigurationDone(const DapRequest& request) {
    configured_ = true;
    Respond(request);
    MaybeStart();
}

void DapServer::MaybeStart() {
    if (!launched_ || !configured_ || started_) return;
    started_ = true;
    running_ = true;
    debuggee_ = std::thread([this] { RunDebuggee(); });
}

void DapServer::RunDebuggee() {
    ScriptHooks hooks;
    hooks.on_ready = [this](ava::VM* vm) { AttachVm(vm); };
    hooks.on_closing = [this] { DetachVm(); };
    hooks.on_error = [this](const std::string& message) { SendOutput("stderr", message + "\n"); };

    int exit_code = runner_(launch_, hooks);
    running_ = false;

    JsonValue exited = Obj();
    exited.set("exitCode", JsonValue(exit_code));
    SendEvent("exited", std::move(exited));
    SendEvent("terminated");
}

void DapServer::AttachVm(ava::VM* vm) {
    vm->SetDapHooksEnabled(!launch_.no_debug);
    vm->SetPrintSink([this](const avastd::string& text) { SendOutput("stdout", text); });
    vm->SetDapStoppedSink([this](const DapStopEvent& event) { OnStopped(event); });

    std::lock_guard<std::mutex> lock(state_mutex_);
    vm_ = vm;
    for (const auto& entry : breakpoints_) {
        vm->DapSetBreakpoints(entry.first, entry.second);
    }
    if (launch_.stop_on_entry && !launch_.no_debug) {
        entry_pending_ = true;
        vm->DapRequestPause();
    }
}

void DapServer::DetachVm() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    vm_ = nullptr;
    stopped_ = false;
}

void DapServer::OnStopped(const DapStopEvent& event) {
    stopped_ = true;
    const char* reason = ReasonName(event.reason);
    if (event.reason == DapStopReason::Pause && entry_pending_.exchange(false)) reason = "entry";

    JsonValue body = Obj();
    body.set("reason", JsonValue(reason));
    body.set("threadId", JsonValue(kMainThreadId));
    body.set("allThreadsStopped", JsonValue(true));
    SendEvent("stopped", std::move(body));
}

void DapServer::HandleSetBreakpoints(const DapRequest& request) {
    const JsonValue& arguments = request.arguments;
    std::string path = arguments.get("source").get("path").as_string();
    if (path.empty()) {
        Fail(request, "setBreakpoints requires source.path");
        return;
    }

    std::vector<int> lines;
    if (arguments.get("breakpoints").is_array()) {
        for (const JsonValue& item : arguments.get("breakpoints").as_array()) {
            lines.push_back(FromClientLine(static_cast<int>(item.get("line").as_int())));
        }
    } else {
        for (const JsonValue& item : arguments.get("lines").as_array()) {
            lines.push_back(FromClientLine(static_cast<int>(item.as_int())));
        }
    }

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        breakpoints_[path] = lines;
        if (vm_) vm_->DapSetBreakpoints(path, lines);
    }

    JsonValue verified = Arr();
    for (int line : lines) {
        JsonValue entry = Obj();
        entry.set("verified", JsonValue(true));
        entry.set("line", JsonValue(ToClientLine(line)));
        verified.push_back(std::move(entry));
    }
    JsonValue body = Obj();
    body.set("breakpoints", std::move(verified));
    Respond(request, std::move(body));
}

void DapServer::HandleSetExceptionBreakpoints(const DapRequest& request) {
    Respond(request);
}

void DapServer::HandleThreads(const DapRequest& request) {
    JsonValue thread = Obj();
    thread.set("id", JsonValue(kMainThreadId));
    thread.set("name", JsonValue("main"));
    JsonValue threads = Arr();
    threads.push_back(std::move(thread));
    JsonValue body = Obj();
    body.set("threads", std::move(threads));
    Respond(request, std::move(body));
}

void DapServer::HandleStackTrace(const DapRequest& request) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (!vm_ || !stopped_) {
        Fail(request, "debuggee is not stopped");
        return;
    }

    JsonValue frames = Arr();
    for (const DapFrameInfo& info : vm_->DapCaptureFrames()) {
        JsonValue frame = Obj();
        frame.set("id", JsonValue(static_cast<int64_t>(info.frame_index)));
        frame.set("name", JsonValue(info.function_name.empty() ? std::string("<main>") : std::string(info.function_name)));
        if (!info.source.empty()) frame.set("source", MakeSource(info.source));
        frame.set("line", JsonValue(ToClientLine(info.line)));
        frame.set("column", JsonValue(ToClientColumn(info.column)));
        frames.push_back(std::move(frame));
    }

    JsonValue body = Obj();
    body.set("totalFrames", JsonValue(static_cast<int64_t>(frames.as_array().size())));
    body.set("stackFrames", std::move(frames));
    Respond(request, std::move(body));
}

void DapServer::HandleScopes(const DapRequest& request) {
    int64_t frame_id = request.arguments.get("frameId").as_int(-1);
    if (frame_id < 0) {
        Fail(request, "scopes requires frameId");
        return;
    }

    JsonValue locals = Obj();
    locals.set("name", JsonValue("Locals"));
    locals.set("variablesReference", JsonValue(frame_id + 1));
    locals.set("expensive", JsonValue(false));
    JsonValue scopes = Arr();
    scopes.push_back(std::move(locals));
    JsonValue body = Obj();
    body.set("scopes", std::move(scopes));
    Respond(request, std::move(body));
}

void DapServer::HandleVariables(const DapRequest& request) {
    int64_t reference = request.arguments.get("variablesReference").as_int(0);
    if (reference <= 0) {
        Fail(request, "invalid variablesReference");
        return;
    }

    std::lock_guard<std::mutex> lock(state_mutex_);
    if (!vm_ || !stopped_) {
        Fail(request, "debuggee is not stopped");
        return;
    }

    JsonValue variables = Arr();
    for (const DapVariable& item : vm_->DapCaptureLocals(static_cast<size_t>(reference - 1))) {
        JsonValue variable = Obj();
        variable.set("name", JsonValue(std::string(item.name)));
        variable.set("value", JsonValue(std::string(item.value)));
        variable.set("type", JsonValue(std::string(item.type)));
        variable.set("variablesReference", JsonValue(0));
        variables.push_back(std::move(variable));
    }
    JsonValue body = Obj();
    body.set("variables", std::move(variables));
    Respond(request, std::move(body));
}

void DapServer::Resume(const DapRequest& request, void (ava::VM::*action)(), JsonValue body) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (!vm_ || !stopped_) {
        Fail(request, "debuggee is not stopped");
        return;
    }
    Respond(request, std::move(body));
    stopped_ = false;
    (vm_->*action)();
}

void DapServer::HandleContinue(const DapRequest& request) {
    JsonValue body = Obj();
    body.set("allThreadsContinued", JsonValue(true));
    Resume(request, &ava::VM::DapContinue, std::move(body));
}

void DapServer::HandleNext(const DapRequest& request) {
    Resume(request, &ava::VM::DapStepOver, JsonValue());
}

void DapServer::HandleStepIn(const DapRequest& request) {
    Resume(request, &ava::VM::DapStepInto, JsonValue());
}

void DapServer::HandleStepOut(const DapRequest& request) {
    Resume(request, &ava::VM::DapStepOut, JsonValue());
}

void DapServer::HandlePause(const DapRequest& request) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (!vm_) {
        Fail(request, "debuggee is not running");
        return;
    }
    Respond(request);
    vm_->DapRequestPause();
}

void DapServer::HandleEvaluate(const DapRequest& request) {
    Fail(request, "evaluate is not supported yet");
}

void DapServer::HandleTerminate(const DapRequest& request) {
    Respond(request);
    if (running_) SendEvent("terminated");
    done_ = true;
}

void DapServer::HandleDisconnect(const DapRequest& request) {
    Respond(request);
    done_ = true;
}

}  // namespace dap
}  // namespace ava
