#pragma once
// AvaHost.Rendering.UiPipeline -- Fase 20 (Integracion con AvaUI).
//
// Renders a .avaui document end-to-end through the NEW avalang.ui
// engine (runtime/avaui: parser::AvauiParser -> RenderTheme ->
// LayoutEngine -> IRenderTree -> ISceneGraph -> HTMLRenderer), fully
// separate from AvaHost's existing production path
// (RuntimeHost::ParseAvaUiFile + HtmlRenderer, which goes through
// avalang.h's C API onto core/src/ui -- see runtime/runtime_host.h).
//
// Why "static" and why a separate file instead of touching
// html_renderer.cpp/runtime_host.cpp: avalang.ui has no AvaLang VM
// integration yet -- no `state`/`code` block binding, no event
// handler dispatch against a running script. AvaHost's real request
// path depends on exactly that (RuntimeHost::BindState/
// BindCodeBehind/InvokeHandler). Swapping the production renderer for
// avalang.ui today would silently drop state/code-behind/event
// handling for every existing app -- see
// runtime/avaui/docs/AVAUI_FASE20_INTEGRATION.md for the full
// analysis and why this phase does NOT touch RuntimeHost/HtmlRenderer/
// web/app.cpp. This adapter is additive only: a new, opt-in code path
// (the `avahost render-static` CLI command, Fase 20.1) for documents
// that have no `state`/`code`/event handlers -- exactly the "static
// preview" case html_renderer.h's own RenderOptions::evalText comment
// already calls out ("a caller with no VM/state to bind against, e.g.
// avahost build's static preview").
//
// Only compiled/linked when AVA_BUILD_UI is ON (see
// runtime/avahost/CMakeLists.txt: `if(TARGET avalang_ui)`), guarded by
// the AVAHOST_HAS_UI_PIPELINE compile definition so callers (namely
// cli_commands.cpp) don't need their own #ifdef -- CmdRenderStatic()
// itself is always declared and always callable, it just returns a
// "not built with AVA_BUILD_UI" error when this file isn't part of the
// build.
#include <string>

namespace avahost {

struct UiPipelineRenderOptions {
    int viewportWidth = 1280;
    int viewportHeight = 720;

    // Fase 20.2.1: field names mirror html_renderer's RenderOptions
    // so the 20.2.5 swap can forward them with zero translation.
    std::string title = "AvaHost App";
    std::string extraHead;
    std::string extraBodyEnd;

    // Gap C / Fase B: components directory passed through to
    // UiComponentResolver so `import components.X` lines resolve to the
    // right filesystem location. Empty == no resolver (render-static
    // CLI, render-dynamic CLI w/o project).
    std::string componentsDir;

    // Gap C / Fase B: project root for dotted-path resolution.
    // Empty == import resolution falls back to componentsDir only.
    std::string projectRoot;
};

bool RenderAvauiStatic(const std::string& avauiSource, const UiPipelineRenderOptions& options,
                       std::string& outHtml, std::string& outError);

} // namespace avahost
