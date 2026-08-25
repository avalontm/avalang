#pragma once

#include <string>
#include <vector>

namespace studio {

struct LogLine {
    std::string text;
};

class LogBridge {
public:
    void Log(const std::string& line) { lines_.push_back({line}); }
    void Clear() { lines_.clear(); }

    const std::vector<LogLine>& Lines() const { return lines_; }

private:
    std::vector<LogLine> lines_;
};

}
