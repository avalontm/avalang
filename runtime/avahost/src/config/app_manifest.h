#pragma once
// AvaHost.Configuration -- loads app.ava's resource `import` lines.
//
// This is NOT AvaLang's real `import module.name` statement (grammar
// `importStatement: 'import' NAME ('.' NAME)*`, resolved through the
// compiler and ava_import()). This is a host-only manifest line:
//
//   import "css/app.css"
//   import "https://some-cdn.example/some-library.js"
//
// AvaUI does not use CSS/Tailwind for layout or styling -- every
// component's size and position comes from the native `LayoutEngine`
// (row/column/fill/padding/gap/width/height/align), rendered
// identically by `GdiRenderer` (desktop) and `HTMLRenderer` (web). A
// fresh `avahost new` project therefore ships with an empty manifest
// (see DefaultAppManifest()): no CSS, no Tailwind CDN. `import
// "css/..."` still works as an explicit opt-in for a project that
// genuinely wants hand-written CSS on top of the native render (e.g.
// styling something outside AvaUI's own element tree) -- it just
// isn't assumed by default anymore.
//
// .js imports (e.g. import "js/some-lib.js") are supported for the
// rare third-party JS *library* a project genuinely needs, but are
// NOT part of the default manifest and are never scaffolded by
// `avahost new`. App logic is written in AvaLang (.ava/.avaui) and
// run by the AvaLang runtime -- it is not hand-written browser JS.
//
// app.ava is read as plain text by AvaHost -- never compiled, never
// passed to the AvaLang core. A quoted string after `import` is never
// valid AvaLang grammar, so there is no ambiguity with real imports
// even if app.ava later also carries real AvaLang bootstrap code.
#include <string>
#include <vector>

namespace avahost {

struct ResourceImport {
    // Exactly what followed `import "..."` -- a project-relative path
    // (e.g. "css/app.css") or a full URL (starts with "http://" or
    // "https://").
    std::string location;

    bool IsUrl() const;

    // ".css" -> <link rel="stylesheet"> in <head>, always emitted
    // after any bare-URL head script (e.g. Tailwind) regardless of
    // declaration order in app.ava -- see BuildHeadTags.
    bool IsStylesheet() const;

    // ".js" -> <script src="..."> at the end of <body> (doesn't block
    // first paint). For a genuine third-party JS library a project
    // needs -- NOT a place to write app logic; that's AvaLang's job.
    bool IsBodyScript() const;

    // Anything else -- a bare CDN URL with no extension, e.g. the
    // Tailwind Play CDN -- <script> in <head>. These generate CSS at
    // runtime by scanning the DOM, so running them late (end of body)
    // causes a flash of unstyled content; loading them in <head>
    // avoids that, same as Tailwind's own docs recommend.
    bool IsHeadScript() const;
};

struct AppManifest {
    std::vector<ResourceImport> resources;
};

// Returns the manifest a fresh `avahost new` project ships with: no
// resources at all. AvaUI renders every component natively (no CSS,
// no Tailwind), so there is nothing to load by default -- see header
// comment above. Used both to generate app.ava's initial content and
// as the fallback when a project has no app.ava at all.
AppManifest DefaultAppManifest();

// Reads projectRoot/app.ava and parses its `import "..."` lines in
// declaration order. Blank lines and `#` comments (AvaLang's line
// comment syntax) are ignored. If app.ava does not exist, returns
// DefaultAppManifest() so a project without one still renders working
// <link>/<script> tags.
AppManifest LoadAppManifest(const std::string& projectRoot);

// Builds <head> tags in two passes, regardless of app.ava's
// declaration order: bare-URL CDN imports (e.g. Tailwind) first, then
// <link rel="stylesheet"> for .css imports -- guarantees CSS always
// loads after Tailwind while staying in <head> (no flash of unstyled
// content), all as plain server-rendered tags, no runtime JS involved.
// For RenderOptions::extraHead.
std::string BuildHeadTags(const AppManifest& manifest);

// Builds the <script src="..."> tags for .js imports, in declaration
// order, for RenderOptions::extraBodyEnd.
std::string BuildBodyEndTags(const AppManifest& manifest);

} // namespace avahost
