#pragma once

#include <string>

#include "util/log_bridge.h"

namespace studio {

void FlushLogToOutput(const std::string& log, std::string::size_type& forwarded_upto, bool flush_partial_tail,
                       const char* prefix, LogBridge& log_bridge);

}
