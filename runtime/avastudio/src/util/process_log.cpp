#include "util/process_log.h"

#include "util/ava_error_line.h"

namespace studio {

namespace {

std::string FormatAvaErrorText(const char* prefix, const ParsedAvaError& parsed) {
    std::string text = std::string(prefix) + "error (" + parsed.kind + "): " + parsed.message;
    if (!parsed.file.empty()) {
        text += " (" + parsed.file;
        if (parsed.line > 0) text += ":" + std::to_string(parsed.line);
        text += ")";
    }
    return text;
}

void PushLine(const char* prefix, const std::string& raw_line, LogBridge& log_bridge) {
    if (raw_line.empty()) return;

    ParsedAvaError parsed;
    if (ParseAvaErrorLine(raw_line, parsed)) {
        log_bridge.LogError(FormatAvaErrorText(prefix, parsed), parsed.file, parsed.line, parsed.col);
        return;
    }
    log_bridge.Log(prefix + raw_line);
}

}

void FlushLogToOutput(const std::string& log, std::string::size_type& forwarded_upto, bool flush_partial_tail,
                       const char* prefix, LogBridge& log_bridge) {
    std::string::size_type start = forwarded_upto;
    for (;;) {
        std::string::size_type nl = log.find('\n', start);
        if (nl == std::string::npos) {
            if (flush_partial_tail && start < log.size()) {
                PushLine(prefix, log.substr(start), log_bridge);
                start = log.size();
            }
            break;
        }
        PushLine(prefix, log.substr(start, nl - start), log_bridge);
        start = nl + 1;
    }
    forwarded_upto = start;
}

}
