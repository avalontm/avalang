#pragma once

#include <string>

namespace studio::util {

// The two file kinds "Generar" (§3.1) can produce inside an existing
// project. Deliberately just these two -- the wizard for a brand new
// *project* (main.ava + folder) is Fase 6, a separate flow.
enum class ScaffoldKind { kClass, kScreen };

// ".ava" for kClass, ".avaui" for kScreen. Explorer forces this extension on
// the created file regardless of what the user typed in the name field, so
// the boilerplate written to disk always matches what the extension says it
// is -- see BuildScaffoldContent below for why that pairing matters.
std::string ScaffoldExtension(ScaffoldKind kind);

// Renders the template for `kind` (loaded once from
// data/scaffold/file_templates.csv, same centralized-CSV pattern
// component_catalog.cpp already uses for the Toolbox catalog) with its
// placeholders substituted from `base_name` -- the target file's name
// without extension, e.g. Explorer passes target.stem().string().
//
// Two placeholders, substituted differently on purpose:
//   {ClassName}   -- sanitized into a valid AvaLang identifier (used as the
//                     class/constructor name in the .ava template, C#-style
//                     constructor: same name as the class, per the
//                     convention already fixed in tools/vscode/examples/
//                     example.ava). Falls back to "NewClass" if base_name
//                     sanitizes to nothing (e.g. the user typed only
//                     punctuation).
//   {DisplayName} -- base_name as typed, with only '"' and '\' escaped so
//                     it drops safely into a quoted .avaui string property
//                     (title/text). Not identifier-sanitized: it's never
//                     used as code, only as a string literal.
//
// Returns an empty string if `kind` has no row in the CSV (missing/
// corrupted data/ -- callers should treat that as "nothing to write" rather
// than writing a broken file).
std::string BuildScaffoldContent(ScaffoldKind kind, const std::string& base_name);

}
