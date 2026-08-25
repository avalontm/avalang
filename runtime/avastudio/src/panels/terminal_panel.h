#pragma once

#include <atomic>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include "engine/engine_bridge.h"

namespace studio {

struct ScriptRunState {
    std::atomic<bool> running{false};
    std::thread worker;

    std::mutex mutex;
    std::string log;
    std::string::size_type log_forwarded_upto = 0;
    bool has_result = false;
    bool result_consumed = false;
    bool launch_failed = false;
    int exit_code = -1;
};

struct TerminalState {
    std::string input_buffer;
    bool has_run_result = false;
    RunResult last_run;

    int selection_anchor = -1;
    int selection_cursor = -1;

    ScriptRunState run;
};

void StartScriptRun(TerminalState& state, EngineBridge& engine, std::string ava_cli_path, std::string script_path);

void PollScriptRun(TerminalState& state, EngineBridge& engine);

struct TerminalFileClickRequest {
    std::string file_path;
    int line = 0;
    int column = 0;
    std::string message;
};

std::optional<TerminalFileClickRequest> DrawTerminalPanel(TerminalState& state, EngineBridge& engine,
                                                            bool* p_open = nullptr);

}
