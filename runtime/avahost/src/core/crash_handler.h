#pragma once
// AvaHost.Core -- last line of defense logging.
//
// Everything in web/server/http_server.cpp's try/catch (and the SEH
// translator installed by seh_guard.h around request handling) only
// catches failures that happen *while serving a request*. Two classes
// of crash fall outside that net entirely:
//   1. A crash on the hot-reload watcher thread or during startup
//      (plugin load, initial route scan) -- not inside HandleConnection
//      at all.
//   2. Anything the SEH translator itself can't turn into a catchable
//      exception (e.g. a stack overflow, which corrupts the stack the
//      handler would need to run on).
//
// InstallCrashHandlers wires std::set_terminate and (on Windows) a
// SetUnhandledExceptionFilter so that, even in those cases, one last
// "FATAL" line is written -- via the logger, so it lands in both the
// console and the error log file from EnableErrorFileLogging -- before
// the process actually goes down. Without this, AvaHost's previous
// behavior for any of the above was to vanish with zero output, which
// is indistinguishable from someone closing the window.
#include "core/logger.h"

namespace avahost {

void InstallCrashHandlers(Logger& logger);

} // namespace avahost
