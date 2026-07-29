#pragma once
// AvaHost.DottedPath -- shared resolution rule for both `extends` and
// `import` lines in .avaui files (core/src/ui/avaui_text.cpp parses
// them as a plain dotted string, e.g. "layouts.main",
// "components.navbar", "routes.edit" -- see that file's header
// comments on ParseExtendsLine/ParseImportLines). AvaHost is the one
// that knows about the project's filesystem layout, so resolution
// lives here, not in core.
//
// Rule: dots become path separators, joined onto projectRoot, with
// ".avaui" appended -- literal, case-sensitive, no automatic
// capitalization of any segment:
//   "layouts.main"      -> "<projectRoot>/layouts/main.avaui"
//   "components.navbar" -> "<projectRoot>/components/navbar.avaui"
//   "routes.edit"        -> "<projectRoot>/routes/edit.avaui"
//
// A dotted path always spells out its top folder explicitly (there is
// no bare `extends main` shorthand that implicitly means
// "layouts/main.avaui") -- what you write is what gets joined, in
// full, onto projectRoot.
#include <filesystem>
#include <string>

namespace avahost {

// "a.b.c" -> "<projectRoot>/a/b/c.avaui". Empty `dotted` returns an
// empty path (caller's job to treat that as "no extends"/"no import").
std::filesystem::path ResolveDottedAvauiPath(const std::string& projectRoot, const std::string& dotted);

// The tag a dotted import is called by in `view` -- the last segment,
// first letter uppercased, rest unchanged (e.g. "components.navbar"
// -> "Navbar", so the page writes `Navbar()`). This is purely a
// lookup key for matching call nodes; it has no bearing on the
// on-disk path, which stays exactly as written (see rule above).
std::string CallableTagFromDotted(const std::string& dotted);

} // namespace avahost
