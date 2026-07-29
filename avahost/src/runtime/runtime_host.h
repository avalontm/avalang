#pragma once
// AvaHost.Runtime -- the ONLY file in AvaHost allowed to include
// avalang.h directly (plan section 9/10: "AvaHost never parses source
// code directly" / "Every interaction with AvaLang goes through
// avalang.h"). Everything above this layer talks to RuntimeHost, never
// to AvaVM/AvaModule/AvaComponent* directly.
//
// The Runtime is a black box: it doesn't know it's running inside a web
// host (plan section "Platform Independent").
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "avalang.h"

namespace avahost {

// Per-request data exposed to running scripts as the `request` global
// (plan section 20 v0.2, "Route Parameters"). Built by AvaHostApp from
// the matched Router::RouteMatch + the parsed query string before every
// RunScript / ParseAvaUiFile-driven render -- see web/app.cpp.
struct RequestContext {
    std::string method;
    std::string path;  // decoded request path, e.g. "/products/42"

    // Route params captured from a declared `route "/products/{id}"`
    // template (see RouteTemplate below), e.g. {"id", "42"} when that
    // template matches "/products/42".
    std::vector<std::pair<std::string, std::string>> params;

    // Parsed query string key/value pairs (web/url_codec.h), e.g.
    // {"sort", "price"} for "?sort=price".
    std::vector<std::pair<std::string, std::string>> query;
};

// Owns one AvaVM ("isolate") for the lifetime of the host process.
// v0.1 is single-VM / single-threaded (matches the blocking HTTP server
// in web/http_server.h); a pool of VMs for concurrent requests is future
// work, not part of this base.
class RuntimeHost {
public:
    RuntimeHost();
    ~RuntimeHost();

    RuntimeHost(const RuntimeHost&) = delete;
    RuntimeHost& operator=(const RuntimeHost&) = delete;

    // Adds a directory AvaLang's `import` resolves relative paths
    // against (routes/, components/, services/, models/, ...).
    void AddSearchPath(const std::string& path);
    void SetCurrentDir(const std::string& path);

    // Publishes `ctx` as the `request` global (a dict: request.method,
    // request.path, request.params.<name>, request.query.<name>) so
    // routes/*.ava and routes/*.avaui can read route params and query
    // string values -- e.g. `request.params.id` for a .avaui page that
    // declares `route "/products/{id}"`. Call once per request, before
    // RunScript/ParseAvaUiFile; overwrites whatever the previous request
    // (if any, on this same reused VM -- see class comment above) set.
    void SetRequestContext(const RequestContext& ctx);

    // Result of parsing a .avaui page/layout/component file. Mirrors
    // ava_ui_parse_avaui_text's out-params 1:1 -- see avalang.h for the
    // exact semantics of each field.
    struct AvaUiDocument {
        AvaComponentTree* tree = nullptr; // owned; release via ~AvaUiDocument
        std::string stateJson   = "{}";
        std::string importsJson = "[]";
        std::string methodsText;
        std::string extends;
        std::string routesJson  = "[]";
        bool ok = true;
        std::string error;

        AvaUiDocument() = default;
        ~AvaUiDocument();
        AvaUiDocument(const AvaUiDocument&) = delete;
        AvaUiDocument& operator=(const AvaUiDocument&) = delete;
        AvaUiDocument(AvaUiDocument&& other) noexcept;
        AvaUiDocument& operator=(AvaUiDocument&& other) noexcept;
    };

    // Parses a .avaui file's full text. Always succeeds structurally
    // (see avalang.h ava_ui_parse_avaui_text doc comment) -- `ok`/`error`
    // reflect the C API's out_error, which is normally empty.
    AvaUiDocument ParseAvaUiFile(const std::string& text) const;

    // Compiles + runs a plain .ava script (e.g. a route's server-side
    // logic, or a services/*.ava module) in this VM. Returns false and
    // fills `outError` on a compile or runtime error.
    bool RunScript(const std::string& source, const std::string& scriptName,
                    std::string& outError);

    // Same as RunScript, but also captures everything the script writes
    // via `print` into `outOutput` instead of letting it go to the
    // process's stdout (AvaPrintFn / ava_vm_set_print_callback --
    // avalang.h). Used for `.ava` routes (web/app.cpp), whose printed
    // output IS the HTTP response body. The callback is cleared again
    // before returning, since this VM is reused across requests (see
    // class comment) and a request's output buffer must not leak into
    // the next one.
    bool RunScriptCapturingOutput(const std::string& source, const std::string& scriptName,
                                   std::string& outOutput, std::string& outError);

    // One `{name}` / `{name?}` / `{name:constraint}` segment inside a
    // declared `route "..."` template. Mirrors the "parameters" shape
    // of avalang.h's out_routes_json (see ava_ui_parse_avaui_text doc
    // comment) -- kept as its own small struct here rather than
    // exposing the core's RouteParameter type, same boundary rule as
    // AvaUiDocument above (avalang.h is the only thing that crosses).
    struct RouteParam {
        std::string name;
        bool optional = false;
        std::string constraint; // empty when the segment has none
    };

    // One `route "/path/{param}"` declaration parsed out of a .avaui
    // file's text (docs/architecture/17_AVAUI_FILE_FORMAT.md, "extends
    // / route (paginas)"). `pathTemplate` is the raw template as
    // written, e.g. "/products/{id}".
    struct RouteTemplate {
        std::string pathTemplate;
        std::vector<RouteParam> params;
    };

    // Parses only the `route "..."` declarations out of a .avaui file's
    // text -- lighter than ParseAvaUiFile since it doesn't need the
    // file's component tree. Used by web/router.h to build its route
    // table once at startup (file-based routing driven by declared
    // `route` lines, NOT by "[name]" filenames -- this project never
    // adopted that convention, see router.h). Empty when the file
    // declares no routes, which is normal: most pages rely on
    // filename-convention routing instead (plan section 11).
    std::vector<RouteTemplate> ParseRouteDeclarations(const std::string& text) const;

    // --- State / code-behind / event bridge (Fase 2, plan section 20 --
    // docs/architecture/AVAHOST_PROGRESS.md rows 9/10) --------------------
    // These four methods are the only place outside ParseAvaUiFile/
    // RunScript* that reach into this VM's globals -- kept here rather
    // than in avahost/src/runtime/state_binder.h or
    // avahost/src/rendering/event_binder.h (which drive them) per this
    // file's own header comment: RuntimeHost is the one layer allowed
    // to touch ava_compile/ava_run/ava_set_global directly.

    // Binds `stateJson` (shape: ava::ui::StateToJson's output, i.e.
    // {"key":"raw text value", ...} -- see AvaUiDocument::stateJson)
    // as globals on this VM, inferring bool/number/string the same way
    // Ava Studio's design/state_eval.cpp's BuildStateVM does for its
    // own per-call VM. Per-request (plan Fase 2 decision A): call once
    // per request with the page's (+ imported components', merged by
    // ComponentResolver) `state` block -- it always overwrites whatever
    // the previous request left on this reused VM's globals for the
    // same keys.
    void BindState(const std::string& stateJson);

    // Compiles+runs `methodsText` (the page's `code`/`methods` block,
    // verbatim) against this VM so every top-level `func Name(...) ...
    // end` becomes a callable global -- same model as Studio's
    // BindCodeBehind. Call after BindState, before InvokeHandler, so a
    // handler body sees this request's state. Best-effort: a
    // compile/run error here is swallowed silently (nothing sensible to
    // surface for a `code` block failing mid-request; the page still
    // renders with whatever state BindState set).
    void BindCodeBehind(const std::string& methodsText);

    // Calls `handlerName()` (zero-arg) against this VM -- e.g. the
    // "OnGuardarClick" a button's `data-handler` attribute names (see
    // avahost/src/rendering/event_binder.h). A handler is expected to
    // mutate `state` globals directly (`counter = counter + 1`), same
    // model AvaLang.UI already uses elsewhere. Returns false and fills
    // `outError` on a compile or runtime error.
    bool InvokeHandler(const std::string& handlerName, std::string& outError);

    // Same as InvokeHandler, but first checks whether `handlerName` was
    // actually defined by the page/layout's `code` block (as an
    // AVA_FUNCTION global) and simply no-ops (returns true, outError
    // untouched) when it wasn't -- the model lifecycle hooks need,
    // since `OnLoad`/`OnShow`/`OnHide`/`OnUnload` are all optional per
    // docs/architecture/17_AVAUI_FILE_FORMAT.md ("Ciclo de vida") and a
    // page that never defines `OnLoad` must render exactly like today,
    // not fail the request. Call after BindCodeBehind.
    bool InvokeHandlerIfDefined(const std::string& handlerName, std::string& outError);

    // Evaluates `rawValue` (a property's verbatim source text, e.g.
    // `title` or a plain string literal like `"Guardar"`) as an
    // expression against this VM's current globals and returns its
    // display text. Falls back to `rawValue` unchanged when it doesn't
    // compile/run, or evaluates to Nil (the common case: a bare string
    // literal isn't a valid identifier) -- same fallback rule as
    // Studio's EvalPropertyExpr. Used as HtmlRenderer::RenderOptions::
    // evalText so rendered markup reflects current state instead of
    // raw source text.
    std::string EvalPropertyExpr(const std::string& rawValue);

    // Reads this VM's *current* globals back out for every key present
    // in `templateStateJson` (typically the same stateJson a prior
    // BindState call used), producing an updated JSON blob in the same
    // shape (StateToJson's {"key":"raw text value", ...}). Used by
    // AvaHostApp to persist state across requests (see app.cpp's
    // stateCache_): after BindState + BindCodeBehind + a handler
    // Dispatch mutates `counter`, this captures the mutated value so
    // the *next* request's BindState starts from it instead of the
    // page's original `state` block default. A key whose current global
    // isn't a plain bool/number/string (Nil -- never set, or some other
    // type) keeps whatever `templateStateJson` already had for it,
    // rather than losing the value.
    std::string ExportStateJson(const std::string& templateStateJson);

    // Redirects everything the VM prints (via `print(...)`) into an
    // internal buffer for the duration between this call and the
    // matching EndConsoleCapture() -- on top of, not instead of, the
    // normal stdout output (see EndConsoleCapture). Used by
    // AvaHostApp to relay whatever a page's `OnLoad`/event handlers
    // printed into the *browser's* console (web/server/app.cpp's
    // BuildConsoleScript) -- plain `print()` alone only ever reached
    // the server's own terminal, which the person looking at the page
    // in a browser can't see. Call once per request, right before
    // BindState/BindCodeBehind/DispatchLifecycle/InvokeHandler run.
    void BeginConsoleCapture();

    // Stops capturing (restores the plain stdout-only sink) and
    // returns everything printed since the matching
    // BeginConsoleCapture call -- one `print(...)` call's text per
    // line, same shape RunScriptCapturingOutput produces.
    std::string EndConsoleCapture();

private:
    AvaVM* vm_ = nullptr;
    std::string consoleCaptureBuffer_;
};

} // namespace avahost
