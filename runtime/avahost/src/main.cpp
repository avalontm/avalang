#include <iostream>
#include <string>
#include <vector>

#include "cli_commands.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        avahost::PrintUsage();
        return 1;
    }

    std::string command = argv[1];
    std::vector<std::string> args;
    for (int i = 2; i < argc; ++i) args.emplace_back(argv[i]);

    if (command == "new")     return avahost::CmdNew(args);
    if (command == "run")     return avahost::CmdRun(args);
    if (command == "watch")   return avahost::CmdWatch(args);
    if (command == "build")   return avahost::CmdBuild(args);
    if (command == "publish") return avahost::CmdPublish(args);
    if (command == "doctor")  return avahost::CmdDoctor(args);
    if (command == "render-static") return avahost::CmdRenderStatic(args);
    if (command == "render-dynamic") return avahost::CmdRenderDynamic(args);

    if (command == "-h" || command == "--help" || command == "help") {
        avahost::PrintUsage();
        return 0;
    }

    std::cerr << "avahost: unknown command '" << command << "'\n\n";
    avahost::PrintUsage();
    return 1;
}
