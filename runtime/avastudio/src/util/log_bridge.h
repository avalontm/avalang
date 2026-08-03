#pragma once

#include <string>
#include <vector>

namespace studio {

// One line in the Output panel's general log (panels/logs_panel.h) --
// separate from EngineBridge::ConsoleLine, which is only ever a running
// script's own stdout/errors. See LogBridge below for why these two
// streams are split.
struct LogLine {
    std::string text;
};

// Host-side log sink for everything that *isn't* a script's own
// print()/compile output: plugin lifecycle messages (loaded/initialized),
// PluginHost applying/rejecting an edit, future engine/IO diagnostics,
// etc.
//
// This used to just be EngineBridge::LogExternal(), writing straight
// into the same scrollback as RunScript()'s stdout -- that made the
// Output panel a mix of "what my script printed" and "what the studio's
// plugins are doing in the background", with no way to tell which was
// which at a glance (see the Output/Terminal split this type exists
// for). LogBridge is intentionally its own tiny type instead of a new
// EngineBridge method: this stream has nothing to do with the AvaVM, so
// it shouldn't live on the same object that owns one.
//
// main.cpp owns one LogBridge for the whole session, same lifetime as
// EngineBridge, and hands it to PluginHost's callbacks and to
// DrawLogsPanel() (panels/logs_panel.h).
class LogBridge {
public:
    void Log(const std::string& line) { lines_.push_back({line}); }
    void Clear() { lines_.clear(); }

    const std::vector<LogLine>& Lines() const { return lines_; }

private:
    std::vector<LogLine> lines_;
};

} // namespace studio
