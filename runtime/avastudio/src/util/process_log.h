#pragma once

#include <string>

#include "util/log_bridge.h"

namespace studio {

// Forwards whatever's new in `log` (since `forwarded_upto`, a byte
// offset into it) to the Output panel, one LogBridge line at a time --
// meant to be called every frame while a background process is running
// (as well as once more right after it finishes), so output shows up in
// Output in near real time instead of as a single dump at the end. Only
// forwards *complete* lines (up to the last '\n') unless
// `flush_partial_tail` is set, in which case a trailing line with no
// newline yet (the very last bit of output once the process has
// actually exited) is forwarded too.
void FlushLogToOutput(const std::string& log, std::string::size_type& forwarded_upto, bool flush_partial_tail,
                       const char* prefix, LogBridge& log_bridge);

} // namespace studio
