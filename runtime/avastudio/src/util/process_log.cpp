#include "util/process_log.h"

namespace studio {

void FlushLogToOutput(const std::string& log, std::string::size_type& forwarded_upto, bool flush_partial_tail,
                       const char* prefix, LogBridge& log_bridge) {
    std::string::size_type start = forwarded_upto;
    for (;;) {
        std::string::size_type nl = log.find('\n', start);
        if (nl == std::string::npos) {
            if (flush_partial_tail && start < log.size()) {
                log_bridge.Log(prefix + log.substr(start));
                start = log.size();
            }
            break;
        }
        std::string line = log.substr(start, nl - start);
        if (!line.empty()) log_bridge.Log(prefix + line);
        start = nl + 1;
    }
    forwarded_upto = start;
}

} // namespace studio
