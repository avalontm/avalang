#pragma once
// AvaHost.Web -- routing.
//
// Two independent mechanisms, both file-based (plan section 11 /
// section 20 v0.2 "Route Parameters"):
//
//   1. Filename convention (static, no parameters):
//        routes/index.avaui             -> /
//        routes/about.avaui             -> /about
//        routes/products/index.avaui    -> /products
//        routes/products/details.avaui  -> /products/details
//      .ava is accepted as a fallback extension for routes that are
//      plain scripts rather than .avaui pages. For dynamic/parameterized
//      routes, use the `route "/path/{param}"` keyword (see section 2 below)
//      instead of filename-based conventions.
//
//   2. Declared routes (dynamic, parameterized): a `.avaui` file can
//      declare one or more top-level `route "/path/{param}"` lines
//      (docs/architecture/17_AVAUI_FILE_FORMAT.md, "extends / route
//      (paginas)"; parsed by core/src/ui/avaui_text.cpp's
//      ParseRouteLines and carried across the C API as
//      out_routes_json -- see RuntimeHost::ParseRouteDeclarations).
//      `{name}` is a required segment, `{name?}` optional, and
//      `{name:constraint}` adds a constraint (int, long, guid, slug,
//      alpha) -- e.g.:
//        route "/products/{id}"
//        route "/products/{id:int}"
//        route "/users/{id}/edit"
//      A file that declares routes is reached ONLY through those
//      templates, not through its filename -- the declaration is
//      what makes routing "file-based" per that doc, same spirit as
//      an explicit @page directive overriding path inference.
//
// Matching order: declared routes are tried first (in file-scan
// order), then the filename convention, so an explicit `route` line
// always wins over an accidental literal-path collision.
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "runtime/runtime_host.h"

namespace avahost {

struct RouteMatch {
    std::string filePath;  // absolute/relative path to the matched .avaui or .ava file
    bool isAvaUi = true;   // false when the match is a plain .ava script

    // Captured dynamic segments, in left-to-right path order, e.g. for
    // route "/users/{id}/posts/{postId}" matching /users/42/posts/7 ->
    // {{"id", "42"}, {"postId", "7"}}. Values are NOT url-decoded here
    // (the router only ever sees raw path segments coming out of
    // SplitPath); the app layer decodes them the same way it decodes
    // the query string -- see web/url_codec.h.
    std::vector<std::pair<std::string, std::string>> params;
};

class Router {
public:
    // `runtime` is used to parse each .avaui file's `route "..."`
    // declarations (RuntimeHost is the only avahost layer allowed to
    // talk to avalang.h -- see runtime/runtime_host.h), both now and on
    // every later Rescan() call (plan section 15, Hot Reload). The
    // caller must keep `runtime` alive for Router's entire lifetime,
    // not just this constructor call -- see class comment below on
    // AvaHostApp's member order guaranteeing this.
    Router(std::string routesDir, const RuntimeHost& runtime);

    // Resolves a request path ("/", "/about", "/products/42") to a
    // file under routesDir, trying declared `route` templates first
    // and falling back to the filename convention. Returns nullopt
    // when nothing matches.
    std::optional<RouteMatch> Resolve(const std::string& requestPath) const;

    // Lists every resolvable route path, for `avahost doctor` /
    // logging / `avahost build`. Declared routes are listed with
    // their `{name}` segments literal, same as they're written in the
    // `route "..."` line, since there's no concrete value to show
    // outside of an actual request.
    std::vector<std::string> ListRoutes() const;

    // Every resolvable route's file, one entry per declared `route`
    // template plus one per filename-convention route not already
    // covered by a declared route (same dedup rule as ListRoutes).
    // Unlike Resolve(), this doesn't need a request path to match
    // against, so it works for a declared route with a `{id:int}`-style
    // constraint too -- used by `avahost build` (plan section 20 v0.2),
    // which just needs to read+parse/run every route file, not capture
    // param values.
    std::vector<RouteMatch> AllRouteFiles() const;

    // Re-scans routesDir_ and replaces the declared-routes table
    // (plan section 15, Hot Reload: "When a source file changes:
    // Recompile -> Replace Bytecode -> Refresh Browser"). Unlike a
    // `.ava`/`.avaui` route's own content -- re-read from disk on
    // every request already, see web/app.cpp's ReadFile calls -- the
    // declared-routes table IS state built once at startup, so it's
    // the one thing Hot Reload actually needs to rebuild here. Safe to
    // call from a different thread than Resolve()/ListRoutes()/
    // AllRouteFiles() (guarded by mutex_ below) -- this is what
    // web/app.h's file-watcher thread calls when a `.avaui`/`.ava`
    // file changes.
    void Rescan();

    // One `{name}` / `{name?}` / `{name:constraint}` template segment,
    // or a plain literal one (isParam == false). Public so router.cpp's
    // free-function matcher (shared by Resolve) can name it.
    struct TemplateSegment {
        bool isParam = false;
        std::string literal;    // when !isParam
        std::string paramName;  // when isParam
        bool optional = false;  // when isParam
        std::string constraint; // when isParam; empty when none
    };

private:
    struct DeclaredRoute {
        std::string pathTemplate;               // as written, e.g. "/products/{id}"
        std::vector<TemplateSegment> segments;
        std::string filePath;
    };

    std::string routesDir_;
    const RuntimeHost* runtime_ = nullptr;
    mutable std::mutex mutex_;
    std::vector<DeclaredRoute> declaredRoutes_;

    void ScanDeclaredRoutes(const RuntimeHost& runtime);
};

} // namespace avahost
