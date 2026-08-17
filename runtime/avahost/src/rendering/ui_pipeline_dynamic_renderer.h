#pragma once
// AvaHost.Rendering.UiPipelineDynamic -- Fase 20.0 + Fase 20.2.
// Extends RenderAvauiStatic (ui_pipeline_static_renderer.h) with the
// VM bridge (ui_vm_state_bridge.h / ui_vm_event_bridge.h) plus the
// Fase 20.2 extensions: page-level wrapper options (title / extraHead /
// extraBodyEnd), `extends` + `slot` layout composition, and state cache
// overlay/persistence across requests.
#include <string>

namespace avalang::ui::parser {
struct ParseErrorInfo;
}

namespace avahost {

class RuntimeHost;

struct UiPipelineRenderOptions;

// avauiPath (Fase 2, PLAN_DIAGNOSTICOS_AVAUI.md) identifies the route
// file avauiSource came from. It's threaded into AvauiParser::Parse so a
// ParseError from the route itself reports its own filename instead of
// an unlabeled line number. Defaults to "" so existing callers keep
// compiling unchanged and just get an unlabeled error, same as before
// this parameter existed.
//
// outParseError (Fase 3) is filled in, when non-null, if the failure was
// a avalang::ui::parser::ParseError -- i.e. a genuine .avaui syntax
// error as opposed to a runtime/binding failure. Mirrors what
// EngineResult::error_line/error_column already does for .ava
// (avastudio/src/engine/engine_bridge.h). Left null by default (existing
// callers), unset (line == 0) when the failure wasn't a ParseError, so
// callers can tell "this really is a parse error, here's where" apart
// from "something else failed, only the message is meaningful".
bool RenderAvauiDynamic(RuntimeHost& host, const std::string& avauiSource,
                        const UiPipelineRenderOptions& options, std::string& outHtml, std::string& outError,
                        const std::string& avauiPath = "",
                        avalang::ui::parser::ParseErrorInfo* outParseError = nullptr);

bool RenderAvauiDynamicWithState(RuntimeHost& host, const std::string& avauiSource,
                                  const UiPipelineRenderOptions& options,
                                  const std::string& cachedStateJson,
                                  const std::string& pendingHandler,
                                  const std::string& pendingCompId,
                                  const std::string& pendingValue,
                                  std::string& outStateJson,
                                  std::string& outHtml, std::string& outError,
                                  const std::string& avauiPath = "",
                                  avalang::ui::parser::ParseErrorInfo* outParseError = nullptr);

bool RenderAvauiDynamicWithLayout(const std::string& projectRoot, RuntimeHost& host,
                                   const std::string& avauiSource,
                                   const UiPipelineRenderOptions& options,
                                   std::string& outHtml, std::string& outError,
                                   const std::string& avauiPath = "",
                                   avalang::ui::parser::ParseErrorInfo* outParseError = nullptr);

bool RenderAvauiDynamicWithLayoutAndState(const std::string& projectRoot, RuntimeHost& host,
                                           const std::string& avauiSource,
                                           const UiPipelineRenderOptions& options,
                                           const std::string& cachedStateJson,
                                           const std::string& pendingHandler,
                                           const std::string& pendingCompId,
                                           const std::string& pendingValue,
                                           std::string& outStateJson,
                                           std::string& outHtml, std::string& outError,
                                           const std::string& avauiPath = "",
                                           avalang::ui::parser::ParseErrorInfo* outParseError = nullptr);

}  // namespace avahost
