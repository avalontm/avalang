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

private:
    AvaVM* vm_ = nullptr;
};

} // namespace avahost
