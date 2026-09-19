#pragma once

#include <string>
#include <vector>

namespace studio {

struct LogLine {
    std::string text;
    bool is_error = false;
    std::string file;
    int line = 0;
    int col = 0;
};

class LogBridge {
public:
    void Log(const std::string& line) { lines_.push_back({line}); }

    void LogError(const std::string& text, const std::string& file, int line, int col) {
        LogLine entry;
        entry.text = text;
        entry.is_error = true;
        entry.file = file;
        entry.line = line;
        entry.col = col;
        lines_.push_back(std::move(entry));
    }

    void Clear() { lines_.clear(); }

    const std::vector<LogLine>& Lines() const { return lines_; }

private:
    std::vector<LogLine> lines_;
};

}
