#pragma once

#include <atomic>
#include <cstdint>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "debug/dap_client.h"

namespace studio {
namespace debug {

enum class DebugPhase { Idle, Starting, Running, Paused, Ended };

struct DebugLaunchOptions {
    std::string ava_cli_path;
    std::string program;
    std::string modules_path;
    std::vector<std::string> args;
    bool stop_on_entry = false;
};

struct DebugFrame {
    int64_t id = 0;
    std::string name;
    std::string file;
    int line = 0;
    int column = 0;
};

struct DebugVariable {
    std::string name;
    std::string value;
    std::string type;
};

class DebugSession {
public:
    DebugSession();
    DebugSession(const DebugSession&) = delete;
    DebugSession& operator=(const DebugSession&) = delete;
    ~DebugSession();

    bool Start(const DebugLaunchOptions& options, std::string& error);
    void Update();
    void Stop();

    void Continue();
    void StepOver();
    void StepInto();
    void StepOut();
    void Pause();
    void SelectFrame(size_t index);

    bool ToggleBreakpoint(const std::string& file, int line);
    bool HasBreakpoint(const std::string& file, int line) const;
    void ClearBreakpoints();
    const std::map<std::string, std::set<int>>& Breakpoints() const { return breakpoints_; }

    DebugPhase Phase() const { return phase_; }
    bool IsActive() const {
        return phase_ == DebugPhase::Starting || phase_ == DebugPhase::Running || phase_ == DebugPhase::Paused;
    }
    const std::vector<DebugFrame>& Frames() const { return frames_; }
    size_t SelectedFrame() const { return selected_frame_; }
    const std::vector<DebugVariable>& Variables() const { return variables_; }
    const std::string& StopReason() const { return stop_reason_; }
    const std::string& Error() const { return error_; }
    int ExitCode() const { return exit_code_; }
    const std::string& Log() const { return log_; }
    std::string TakeNewLog();

private:
    void Reset();
    void RunProcess();
    void AppendProcessOutput(const std::string& chunk);
    void DrainProcessOutput();
    void StartConnect();
    void ResolveConnect();
    void BeginHandshake();
    void SendLaunch();
    void SendConfiguration();
    void SendSetBreakpoints(const std::string& file);
    void OnEvent(const std::string& name, const ava::dap::JsonValue& body);
    void OnStopped(const ava::dap::JsonValue& body);
    void OnClosed();
    void RefreshStack();
    void LoadVariables(int64_t frame_id);
    void Resume(const char* command);
    void Fail(const std::string& message);
    void ReleaseClient();
    void AppendLog(const std::string& text);

    DapClient client_;
    DebugLaunchOptions options_;

    DebugPhase phase_ = DebugPhase::Idle;
    std::string error_;
    std::string log_;
    size_t log_taken_ = 0;
    int exit_code_ = 0;
    std::string stop_reason_;

    std::vector<DebugFrame> frames_;
    size_t selected_frame_ = 0;
    std::vector<DebugVariable> variables_;
    uint64_t generation_ = 0;

    std::map<std::string, std::set<int>> breakpoints_;
    bool configured_ = false;
    bool finishing_ = false;
    bool server_released_ = false;
    bool connected_once_ = false;

    std::thread process_thread_;
    std::atomic<bool> process_done_{false};
    std::atomic<int> port_{0};
    std::mutex output_mutex_;
    std::string process_output_;

    std::thread connect_thread_;
    std::atomic<int> connect_result_{0};
    std::string connect_error_;
    bool connect_started_ = false;
    bool connect_resolved_ = false;
};

}  // namespace debug
}  // namespace studio
