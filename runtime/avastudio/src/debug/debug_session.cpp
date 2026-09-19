#include "debug/debug_session.h"

#include <cctype>
#include <cstdlib>
#include <utility>

#include "platform/Platform.h"
#include "platform/interfaces/IProcessStream.h"

namespace studio {
namespace debug {

using ava::dap::JsonValue;

namespace {

constexpr int kMainThreadId = 1;
constexpr int kConnectTimeoutMs = 3000;
constexpr int kReleaseTimeoutMs = 300;
const char kListeningMarker[] = "DAP server listening on 127.0.0.1:";

JsonValue Obj() { return JsonValue::MakeObject(); }
JsonValue Arr() { return JsonValue::MakeArray(); }

JsonValue ThreadArgs() {
    JsonValue args = Obj();
    args.set("threadId", JsonValue(kMainThreadId));
    return args;
}

int ParseListeningPort(const std::string& text) {
    size_t marker = text.find(kListeningMarker);
    if (marker == std::string::npos) return 0;
    size_t start = marker + sizeof(kListeningMarker) - 1;
    size_t end = start;
    while (end < text.size() && std::isdigit(static_cast<unsigned char>(text[end]))) ++end;
    if (end == start || end == text.size()) return 0;
    return std::atoi(text.substr(start, end - start).c_str());
}

}  // namespace

DebugSession::DebugSession() {
    client_.SetEventHandler([this](const std::string& name, const JsonValue& body) { OnEvent(name, body); });
    client_.SetClosedHandler([this] { OnClosed(); });
}

DebugSession::~DebugSession() {
    Stop();
    if (connect_thread_.joinable()) connect_thread_.join();
    if (process_thread_.joinable()) process_thread_.join();
    client_.Disconnect();
}

bool DebugSession::Start(const DebugLaunchOptions& options, std::string& error) {
    if (IsActive()) {
        error = "a debug session is already running";
        return false;
    }
    if (options.ava_cli_path.empty()) {
        error = "ava_cli was not found";
        return false;
    }
    if (options.program.empty()) {
        error = "no program to debug";
        return false;
    }

    Reset();
    options_ = options;
    phase_ = DebugPhase::Starting;
    process_thread_ = std::thread([this] { RunProcess(); });
    return true;
}

void DebugSession::Reset() {
    if (connect_thread_.joinable()) connect_thread_.join();
    if (process_thread_.joinable()) process_thread_.join();
    client_.Disconnect();

    phase_ = DebugPhase::Idle;
    error_.clear();
    log_.clear();
    log_taken_ = 0;
    exit_code_ = 0;
    stop_reason_.clear();
    frames_.clear();
    selected_frame_ = 0;
    variables_.clear();
    ++generation_;
    configured_ = false;
    finishing_ = false;
    server_released_ = false;
    connected_once_ = false;
    process_done_ = false;
    port_ = 0;
    connect_result_ = 0;
    connect_error_.clear();
    connect_started_ = false;
    connect_resolved_ = false;
    {
        std::lock_guard<std::mutex> lock(output_mutex_);
        process_output_.clear();
    }
}

void DebugSession::RunProcess() {
    auto platform = ava::platform::Platform::Create();
    ava::platform::IProcess& process = platform->Process();
    auto* streaming = dynamic_cast<ava::platform::IProcessStream*>(&process);

    if (!streaming) {
        AppendProcessOutput("error: process streaming is not available on this platform\n");
        process_done_ = true;
        return;
    }

    std::vector<std::string> args = {"--dap-server", "0"};
    if (!options_.modules_path.empty()) {
        args.push_back("--modules");
        args.push_back(options_.modules_path);
    }
    args.push_back(options_.program);

    std::string tail;
    int exit_code = -1;
    bool launched = streaming->ExecuteStreaming(
        options_.ava_cli_path, args,
        [this, &tail](const std::string& chunk) {
            AppendProcessOutput(chunk);
            if (port_ == 0) {
                tail += chunk;
                int port = ParseListeningPort(tail);
                if (port > 0) port_ = port;
            }
        },
        exit_code);

    if (!launched) AppendProcessOutput("error: could not run " + options_.ava_cli_path + "\n");
    process_done_ = true;
}

void DebugSession::AppendProcessOutput(const std::string& chunk) {
    std::lock_guard<std::mutex> lock(output_mutex_);
    process_output_ += chunk;
}

void DebugSession::DrainProcessOutput() {
    std::string chunk;
    {
        std::lock_guard<std::mutex> lock(output_mutex_);
        chunk.swap(process_output_);
    }
    if (!chunk.empty()) log_ += chunk;
}

void DebugSession::AppendLog(const std::string& text) {
    log_ += text;
}

std::string DebugSession::TakeNewLog() {
    std::string fresh = log_.substr(log_taken_);
    log_taken_ = log_.size();
    return fresh;
}

void DebugSession::Update() {
    DrainProcessOutput();

    if (phase_ == DebugPhase::Starting) {
        if (!connect_started_ && port_ > 0) StartConnect();
        if (connect_started_ && !connect_resolved_) ResolveConnect();
        if (phase_ == DebugPhase::Starting && !connect_started_ && process_done_ && port_ == 0) {
            Fail("ava_cli exited before the debug server started");
        }
    }

    if (client_.IsConnected()) client_.Poll();

    if (finishing_) {
        finishing_ = false;
        ReleaseClient();
    }

    if (process_done_ && process_thread_.joinable()) process_thread_.join();
}

void DebugSession::StartConnect() {
    connect_started_ = true;
    int port = port_;
    connect_thread_ = std::thread([this, port] {
        std::string error;
        bool ok = client_.Connect(port, kConnectTimeoutMs, error);
        connect_error_ = error;
        connect_result_ = ok ? 1 : -1;
    });
}

void DebugSession::ResolveConnect() {
    int result = connect_result_;
    if (result == 0) return;

    connect_thread_.join();
    connect_resolved_ = true;
    if (result > 0) {
        connected_once_ = true;
        BeginHandshake();
    } else {
        Fail("could not connect to the debug server: " + connect_error_);
    }
}

void DebugSession::BeginHandshake() {
    JsonValue args = Obj();
    args.set("clientID", JsonValue("avastudio"));
    args.set("clientName", JsonValue("AvaStudio"));
    args.set("adapterID", JsonValue("avalang"));
    args.set("linesStartAt1", JsonValue(true));
    args.set("columnsStartAt1", JsonValue(true));
    args.set("pathFormat", JsonValue("path"));

    client_.SendRequest("initialize", std::move(args), [this](const JsonValue& response) {
        if (!DapClient::Succeeded(response)) {
            Fail("initialize failed: " + DapClient::ErrorMessage(response));
            return;
        }
        SendLaunch();
    });
}

void DebugSession::SendLaunch() {
    JsonValue args = Obj();
    args.set("program", JsonValue(options_.program));
    JsonValue script_args = Arr();
    for (const std::string& arg : options_.args) script_args.push_back(JsonValue(arg));
    args.set("args", std::move(script_args));
    args.set("stopOnEntry", JsonValue(options_.stop_on_entry));
    args.set("noDebug", JsonValue(false));

    client_.SendRequest("launch", std::move(args), [this](const JsonValue& response) {
        if (!DapClient::Succeeded(response)) Fail("launch failed: " + DapClient::ErrorMessage(response));
    });
}

void DebugSession::SendConfiguration() {
    for (const auto& entry : breakpoints_) SendSetBreakpoints(entry.first);
    configured_ = true;

    client_.SendRequest("configurationDone", JsonValue(), [this](const JsonValue& response) {
        if (!DapClient::Succeeded(response)) {
            Fail("configurationDone failed: " + DapClient::ErrorMessage(response));
            return;
        }
        if (phase_ == DebugPhase::Starting) phase_ = DebugPhase::Running;
    });
}

void DebugSession::SendSetBreakpoints(const std::string& file) {
    JsonValue source = Obj();
    source.set("path", JsonValue(file));

    JsonValue lines = Arr();
    auto it = breakpoints_.find(file);
    if (it != breakpoints_.end()) {
        for (int line : it->second) {
            JsonValue entry = Obj();
            entry.set("line", JsonValue(line));
            lines.push_back(std::move(entry));
        }
    }

    JsonValue args = Obj();
    args.set("source", std::move(source));
    args.set("breakpoints", std::move(lines));
    client_.SendRequest("setBreakpoints", std::move(args));
}

void DebugSession::OnEvent(const std::string& name, const JsonValue& body) {
    if (name == "initialized") {
        SendConfiguration();
    } else if (name == "stopped") {
        OnStopped(body);
    } else if (name == "output") {
        if (body.get("category").as_string() != "telemetry") AppendLog(body.get("output").as_string());
    } else if (name == "exited") {
        exit_code_ = static_cast<int>(body.get("exitCode").as_int());
    } else if (name == "terminated") {
        if (phase_ != DebugPhase::Ended) {
            phase_ = DebugPhase::Ended;
            ++generation_;
            frames_.clear();
            variables_.clear();
            finishing_ = true;
        }
    }
}

void DebugSession::OnStopped(const JsonValue& body) {
    phase_ = DebugPhase::Paused;
    stop_reason_ = body.get("reason").as_string();
    ++generation_;
    RefreshStack();
}

void DebugSession::OnClosed() {
    if (phase_ == DebugPhase::Ended) return;
    AppendLog("debug connection closed\n");
    phase_ = DebugPhase::Ended;
    ++generation_;
    frames_.clear();
    variables_.clear();
    finishing_ = true;
}

void DebugSession::RefreshStack() {
    uint64_t generation = generation_;
    client_.SendRequest("stackTrace", ThreadArgs(), [this, generation](const JsonValue& response) {
        if (generation != generation_ || !DapClient::Succeeded(response)) return;

        frames_.clear();
        for (const JsonValue& item : response.get("body").get("stackFrames").as_array()) {
            DebugFrame frame;
            frame.id = item.get("id").as_int();
            frame.name = item.get("name").as_string();
            frame.file = item.get("source").get("path").as_string();
            frame.line = static_cast<int>(item.get("line").as_int());
            frame.column = static_cast<int>(item.get("column").as_int());
            frames_.push_back(std::move(frame));
        }
        selected_frame_ = 0;
        variables_.clear();
        if (!frames_.empty()) LoadVariables(frames_[0].id);
    });
}

void DebugSession::LoadVariables(int64_t frame_id) {
    uint64_t generation = generation_;
    JsonValue args = Obj();
    args.set("frameId", JsonValue(frame_id));

    client_.SendRequest("scopes", std::move(args), [this, generation](const JsonValue& scopes_response) {
        if (generation != generation_ || !DapClient::Succeeded(scopes_response)) return;

        const auto& scopes = scopes_response.get("body").get("scopes").as_array();
        if (scopes.empty()) return;

        JsonValue variables_args = Obj();
        variables_args.set("variablesReference", JsonValue(scopes[0].get("variablesReference").as_int()));

        client_.SendRequest("variables", std::move(variables_args), [this, generation](const JsonValue& response) {
            if (generation != generation_ || !DapClient::Succeeded(response)) return;

            variables_.clear();
            for (const JsonValue& item : response.get("body").get("variables").as_array()) {
                DebugVariable variable;
                variable.name = item.get("name").as_string();
                variable.value = item.get("value").as_string();
                variable.type = item.get("type").as_string();
                variables_.push_back(std::move(variable));
            }
        });
    });
}

void DebugSession::SelectFrame(size_t index) {
    if (phase_ != DebugPhase::Paused || index >= frames_.size()) return;
    selected_frame_ = index;
    variables_.clear();
    LoadVariables(frames_[index].id);
}

void DebugSession::Resume(const char* command) {
    if (phase_ != DebugPhase::Paused) return;
    phase_ = DebugPhase::Running;
    ++generation_;
    frames_.clear();
    variables_.clear();
    stop_reason_.clear();

    std::string name = command;
    client_.SendRequest(command, ThreadArgs(), [this, name](const JsonValue& response) {
        if (!DapClient::Succeeded(response)) AppendLog(name + " failed: " + DapClient::ErrorMessage(response) + "\n");
    });
}

void DebugSession::Continue() { Resume("continue"); }
void DebugSession::StepOver() { Resume("next"); }
void DebugSession::StepInto() { Resume("stepIn"); }
void DebugSession::StepOut() { Resume("stepOut"); }

void DebugSession::Pause() {
    if (phase_ != DebugPhase::Running) return;
    client_.SendRequest("pause", ThreadArgs());
}

bool DebugSession::ToggleBreakpoint(const std::string& file, int line) {
    std::set<int>& lines = breakpoints_[file];
    bool now_set = lines.insert(line).second;
    if (!now_set) lines.erase(line);
    if (lines.empty()) {
        if (configured_ && client_.IsConnected()) SendSetBreakpoints(file);
        breakpoints_.erase(file);
    } else if (configured_ && client_.IsConnected()) {
        SendSetBreakpoints(file);
    }
    return now_set;
}

bool DebugSession::HasBreakpoint(const std::string& file, int line) const {
    auto it = breakpoints_.find(file);
    return it != breakpoints_.end() && it->second.count(line) > 0;
}

void DebugSession::ClearBreakpoints() {
    std::vector<std::string> files;
    for (const auto& entry : breakpoints_) files.push_back(entry.first);
    breakpoints_.clear();
    if (!configured_ || !client_.IsConnected()) return;
    for (const std::string& file : files) SendSetBreakpoints(file);
}

void DebugSession::Fail(const std::string& message) {
    error_ = message;
    AppendLog("error: " + message + "\n");
    phase_ = DebugPhase::Ended;
    ++generation_;
    frames_.clear();
    variables_.clear();
    finishing_ = true;
}

void DebugSession::Stop() {
    if (phase_ == DebugPhase::Idle) return;
    if (connect_thread_.joinable()) connect_thread_.join();
    connect_resolved_ = true;
    phase_ = DebugPhase::Ended;
    ++generation_;
    frames_.clear();
    variables_.clear();
    ReleaseClient();
}

void DebugSession::ReleaseClient() {
    if (client_.IsConnected()) {
        client_.SendRequest("disconnect");
        client_.Disconnect();
        server_released_ = true;
        return;
    }

    client_.Disconnect();
    if (server_released_) return;
    server_released_ = true;

    if (port_ > 0 && !connected_once_) {
        std::string ignored;
        if (client_.Connect(port_, kReleaseTimeoutMs, ignored)) client_.Disconnect();
    }
}

}  // namespace debug
}  // namespace studio
