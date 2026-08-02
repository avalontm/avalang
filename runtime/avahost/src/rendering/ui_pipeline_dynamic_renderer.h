#pragma once
// AvaHost.Rendering.UiPipelineDynamic -- Fase 20.0 + Fase 20.2.
// Extends RenderAvauiStatic (ui_pipeline_static_renderer.h) with the
// VM bridge (ui_vm_state_bridge.h / ui_vm_event_bridge.h) plus the
// Fase 20.2 extensions: page-level wrapper options (title / extraHead /
// extraBodyEnd), `extends` + `slot` layout composition, and state cache
// overlay/persistence across requests.
#include <string>

namespace avahost {

class RuntimeHost;

struct UiPipelineRenderOptions;

bool RenderAvauiDynamic(RuntimeHost& host, const std::string& avauiSource,
                        const UiPipelineRenderOptions& options, std::string& outHtml, std::string& outError);

bool RenderAvauiDynamicWithState(RuntimeHost& host, const std::string& avauiSource,
                                  const UiPipelineRenderOptions& options,
                                  const std::string& cachedStateJson,
                                  const std::string& pendingHandler,
                                  const std::string& pendingCompId,
                                  const std::string& pendingValue,
                                  std::string& outStateJson,
                                  std::string& outHtml, std::string& outError);

bool RenderAvauiDynamicWithLayout(const std::string& projectRoot, RuntimeHost& host,
                                   const std::string& avauiSource,
                                   const UiPipelineRenderOptions& options,
                                   std::string& outHtml, std::string& outError);

bool RenderAvauiDynamicWithLayoutAndState(const std::string& projectRoot, RuntimeHost& host,
                                           const std::string& avauiSource,
                                           const UiPipelineRenderOptions& options,
                                           const std::string& cachedStateJson,
                                           const std::string& pendingHandler,
                                           const std::string& pendingCompId,
                                           const std::string& pendingValue,
                                           std::string& outStateJson,
                                           std::string& outHtml, std::string& outError);

}  // namespace avahost
