#pragma once
// AvaHost.Rendering.Event -- Fase 2 module 2
// (docs/architecture/AVAHOST_PROGRESS.md row 9: "Eventos / methods").
//
// core/src/ui/avaui_text.cpp already binds a component's `click`-style
// event to a handler name at parse time -- either explicitly
// (`click = "OnGuardarClick"` in `view`) or automatically by naming
// convention (AutoBindEvents: a `button` with `id = "Guardar"` plus a
// `code` function named `OnGuardarClick` gets wired up with no extra
// syntax). That binding lands as an ordinary AvaComponent event
// (ava_ui_set_event/get_event), readable at render time.
//
// EventBinder is the AvaHost-side half: it (1) turns those bound
// events into `data-event`/`data-handler` HTML attributes a small
// client script can read to know what to POST on interaction, and (2)
// parses that POST body back into a handler name server-side. It never
// touches the VM itself -- dispatching the named handler is
// RuntimeHost::InvokeHandler's job (see avahost/src/runtime/
// state_binder.h), same "only RuntimeHost touches ava_compile/ava_run"
// boundary as component_resolver.h explains for the UI-tree side.
#include <string>

#include "avalang.h"

namespace avahost {

class EventBinder {
public:
    // Renders every event bound on `comp` as HTML data-* attributes,
    // e.g. a `click` event bound to "OnGuardarClick" becomes:
    //   data-event="click" data-handler="OnGuardarClick"
    // A component with more than one bound event (rare, but the .avaui
    // format allows binding several -- e.g. `click` and `change` on the
    // same node) repeats the pair per event as data-event-2/
    // data-handler-2, data-event-3/data-handler-3, ... since HTML
    // attribute names can't repeat -- the first (common, single-event)
    // case keeps the plain unnumbered names so the client script's
    // default path stays simple. Returns "" (no attributes) for a
    // component with no bound events, which is most of them.
    static std::string RenderAttributes(AvaComponent* comp);

    // Extracts the "handler" field from an
    // application/x-www-form-urlencoded POST body (what the client
    // script RenderAttributes' counterpart sends -- see
    // AvaHostApp::HandleEventRoute in web/server/app.cpp). Returns ""
    // when the body has no `handler` key. Deliberately tiny/manual
    // (reuses web/protocol/url_codec.h's ParseQueryString, since
    // x-www-form-urlencoded is the same wire format as a query string)
    // rather than a general form-data parser -- v0.1's client only ever
    // sends this one field.
    static std::string ExtractHandlerName(const std::string& urlEncodedBody);
};

} // namespace avahost
