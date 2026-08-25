#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "avalang.h"

namespace studio {

struct RunResult {
    bool success = false;
    std::string message;

    int error_line = 0;
    int error_column = 0;

    std::string error_source;
};

struct ConsoleLine {
    enum class Kind {
        Info,
        Stdout,
        Error,

        Success,
        Input,
    };
    Kind kind;
    std::string text;

    std::string error_source;
    int error_line = 0;
    int error_column = 0;
};

class EngineBridge {
public:
    EngineBridge();
    ~EngineBridge();

    EngineBridge(const EngineBridge&) = delete;
    EngineBridge& operator=(const EngineBridge&) = delete;

    RunResult RunScript(const std::string& source, const std::string& source_name);

    // Parses + type-checks source without running it (no bytecode execution,
    // no console_ output) -- the "Check" operation behind the Problems panel.
    // Reuses the same diagnostic shape as RunScript's failure path so both
    // can feed the same Problems sink.
    RunResult CheckScript(const std::string& source, const std::string& source_name);

    void SetModulesPath(const std::string& path);

    const std::vector<ConsoleLine>& Console() const { return console_; }
    void ClearConsole() { console_.clear(); }

    void AppendConsoleLine(ConsoleLine::Kind kind, const std::string& text,
                            const std::string& error_source = "", int error_line = 0,
                            int error_column = 0) {
        console_.push_back({kind, text, error_source, error_line, error_column});
    }

    void AppendExternalOutput(const std::string& raw_text) { OnScriptPrint(raw_text); }
    void FlushExternalOutput() { FlushPendingStdoutLine(); }

    void SubmitConsoleInput(const std::string& text);

    struct PreviewNode {
        std::string type;
        std::string id;
        std::vector<std::pair<std::string, std::string>> properties;
        std::vector<PreviewNode> children;
    };

    struct DemoTree {
        PreviewNode root;
        std::string json;
    };

    DemoTree BuildDemoComponentTree();

private:

    static void PrintCallbackTrampoline(const char* utf8, size_t len, void* user_data);
    void OnScriptPrint(const std::string& chunk);
    void FlushPendingStdoutLine();

    AvaVM* vm_ = nullptr;
    std::vector<ConsoleLine> console_;

    std::string pending_stdout_line_;

    std::vector<std::string> input_queue_;
};

}
