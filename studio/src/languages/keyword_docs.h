#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace studio {

// Syntax help for every AvaLang control-flow/statement keyword
// (languages::AvaLang()'s lang.keywords, mirroring grammar/AvaLang.g4).
//
// The data itself lives in data/keyword_docs.csv, next to ava_studio.exe
// -- editable in any plain text editor, no recompile needed. This struct
// and KeywordDocs() are NOT derived from the grammar at build time (Ava
// Studio has no ANTLR introspection available to the editor), so the CSV
// is kept in sync by hand: if a keyword's grammar rule changes, mirror it
// there too, or the Code Editor's keyword hint tooltip (DrawKeywordHint in
// editor_panel.cpp) will drift out of date with what the parser actually
// accepts. If the CSV is missing or fails to parse, KeywordDocs() falls
// back to DefaultKeywordDocs() below, which mirrors the CSV's shipped
// content -- so a corrupted or deleted file never leaves the editor
// without tooltips, it just stops picking up hand edits until fixed.
struct KeywordDoc {
    std::string name;

    // One or more usage patterns, each a small multi-line block shown
    // verbatim in the tooltip (e.g. "if expr then\n    ...\nend"). More
    // than one entry means the grammar accepts more than one form (e.g.
    // while's optional parens around its condition). Shown in a
    // monospaced "code box" in the tooltip -- this is the abstract
    // pattern, with placeholder names like `condition`/`expr`, not
    // runnable code.
    std::vector<std::string> syntax;

    // A single concrete, runnable snippet using real values instead of
    // placeholders (e.g. for `if`, an actual `if age >= 18 then ... end`
    // instead of `if condition then ... end`). Aimed specifically at
    // someone who isn't a programmer and finds the abstract syntax
    // pattern above harder to map onto real code than a worked example.
    // Optional -- empty for keywords that only make sense paired with
    // another one already shown (e.g. `then`, `in`, `as`, `catch`),
    // where the owning keyword's example already covers it.
    std::string example;

    // One or two sentences explaining what the keyword does, aimed at
    // someone new to AvaLang -- shown under the syntax/example blocks the
    // same way FunctionSignature::doc is shown under a function's
    // signature (see BuiltinSignatures()/DrawParameterHint).
    std::string doc;
};

// Keyed by keyword spelling ("if", "while", "for", ...). Every entry in
// languages::AvaLang()'s lang.keywords has an entry here. Re-checks
// data/keyword_docs.csv's modification time on every call (cheap: a
// filesystem stat, not a re-parse, unless the timestamp actually
// changed) so editing the CSV and switching back to Ava Studio picks it
// up without restarting.
const std::unordered_map<std::string, KeywordDoc>& KeywordDocs();

// The hardcoded fallback table, used when data/keyword_docs.csv is
// missing or fails to parse. Also what tools/dump_docs.cpp writes out to
// bootstrap a fresh keyword_docs.csv.
const std::unordered_map<std::string, KeywordDoc>& DefaultKeywordDocs();

} // namespace studio
