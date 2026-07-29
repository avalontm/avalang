#pragma once
// AvaHost.Runtime.State -- Fase 2 module 3
// (docs/architecture/AVAHOST_PROGRESS.md row 10: "Estado reactivo").
//
// Thin per-request orchestrator over RuntimeHost's state/code-behind/
// handler surface (RuntimeHost::BindState/BindCodeBehind/InvokeHandler/
// EvalPropertyExpr -- see runtime_host.h; that's the only place
// actually allowed to touch ava_compile/ava_run/ava_set_global, per its
// own header comment). StateBinder just sequences those calls the way
// a request needs them and hands back a text evaluator
// HtmlRenderer::RenderOptions::evalText can call, so this module (not
// AvaHostApp::RenderAvaUiRoute directly) is what "state reactivity"
// means in practice: `state` block -> VM globals -> handler mutates
// them -> view text re-evaluated against the mutated globals.
//
// Per-request (plan Fase 2 decision A, "state per-request, KISS"):
// nothing here persists across requests on its own -- Bind() always
// starts from the stateJson text passed in (the page's `state` block,
// merged with every imported component's own `state` by
// ComponentResolver -- see component/component_resolver.h), the same
// as a fresh page load. A future per-session pass (plan doc: "Sessions
// -> Fase 3") would sit in front of this class, not inside it.
#include <functional>
#include <string>

#include "runtime/runtime_host.h"

namespace avahost {

class StateBinder {
public:
    explicit StateBinder(RuntimeHost& runtime) : runtime_(runtime) {}

    // Binds `stateJson` as VM globals, then `methodsText` (the page's
    // `code`/`methods` block) as callable globals, in that order so a
    // handler body sees this request's state values. Call once per
    // request, before Dispatch/TextEvaluator.
    void Bind(const std::string& stateJson, const std::string& methodsText);

    // Runs `handlerName()` if non-empty -- the event a POST'd
    // interaction named (see rendering/event_binder.h::
    // ExtractHandlerName). No-op, returns true, when `handlerName` is
    // empty (the ordinary GET-request case: nothing to dispatch, just
    // render). The handler body is expected to mutate state globals
    // directly (`counter = counter + 1`), which TextEvaluator's
    // evaluator then reflects on the re-render that follows.
    bool Dispatch(const std::string& handlerName, std::string& outError);

    // Calls a lifecycle hook (`OnLoad`, currently the only one AvaHost
    // actually fires -- see RenderAvaUiRoute's header comment for why
    // `OnShow`/`OnHide`/`OnUnload` stay no-ops here) if and only if the
    // page/layout's `code` block defined it. A page that never writes
    // `func OnLoad()` renders exactly as before this existed.
    bool DispatchLifecycle(const std::string& hookName, std::string& outError);

    // A callable for HtmlRenderer::RenderOptions::evalText: evaluates a
    // property's raw source text against whatever Bind/Dispatch left on
    // this request's VM globals. Safe to call as many times as
    // HtmlRenderer needs (once per rendered property) -- each call is
    // independent, side-effect-free (RuntimeHost::EvalPropertyExpr
    // never mutates state, only reads it).
    std::function<std::string(const std::string&)> TextEvaluator();

private:
    RuntimeHost& runtime_;
};

} // namespace avahost
