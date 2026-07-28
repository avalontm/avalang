#pragma once
// AvaHost.CLI -- `avahost new|run|watch|build|publish|doctor` (plan
// section 18). One function per command, all taking the parsed
// argv tail so main.cpp stays a thin dispatcher.
#include <string>
#include <vector>

namespace avahost {

// Return value is the process exit code (0 = success).
int CmdNew(const std::vector<std::string>& args);
int CmdRun(const std::vector<std::string>& args);
int CmdWatch(const std::vector<std::string>& args);
int CmdBuild(const std::vector<std::string>& args);
int CmdPublish(const std::vector<std::string>& args);
int CmdDoctor(const std::vector<std::string>& args);

void PrintUsage();

} // namespace avahost
