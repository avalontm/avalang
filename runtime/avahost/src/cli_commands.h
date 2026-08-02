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

// Fase 20.1 -- renders a single .avaui file through the new
// avalang.ui pipeline (parser -> theme -> layout -> render tree ->
// scene graph -> HTML), independent of RuntimeHost/HtmlRenderer.
// Static only: no `state`/`code`/event-handler binding (see
// rendering/ui_pipeline_static_renderer.h). Reports itself
// unavailable (exit code 1, explanatory message) when avahost was
// built without AVA_BUILD_UI -- see cli_commands.cpp.
int CmdRenderStatic(const std::vector<std::string>& args);

// Fase 20.0 -- like CmdRenderStatic, but binds `state`/`code`/click
// handlers onto a real VM first (rendering/ui_pipeline_dynamic_renderer.h),
// so `OnLoad` and any state the source references actually run/exist.
// Same AVA_BUILD_UI availability gate as CmdRenderStatic.
int CmdRenderDynamic(const std::vector<std::string>& args);

void PrintUsage();

} // namespace avahost
