#pragma once

#include <string>
#include <unordered_set>

#include "TextEditor.h"

namespace studio::languages {

const TextEditor::Language* AvaLang();

// Registers `added` and releases `removed` in the tokenizer's shared table
// of interface names, used when coloring class-name and heritage-clause
// tokens (`class Circle : IShape` colors IShape differently from a base
// class). AvaLang() is a single Language instance shared by every open tab,
// so the table is shared too -- each name is reference-counted by how many
// currently-open tabs currently declare it as an interface (see
// EditorTab::known_interface_names), so renaming or deleting an interface
// in one tab, or closing that tab outright, only forgets the name once no
// other open tab still declares it. Call this from RebuildIndexAndTrie with
// the diff between a tab's previous and freshly computed interface-name
// set, and from tab close with (tab's last known set, {}) to release its
// contribution entirely.
void UpdateKnownInterfaceNames(const std::unordered_set<std::string>& removed,
                                const std::unordered_set<std::string>& added);

// Bumped every time UpdateKnownInterfaceNames actually flips a name's
// known/unknown status (not on every call -- e.g. a no-op removal of an
// already-forgotten name doesn't count). The vendored text editor colorizes
// a buffer once, up front, in SetText()/setText() -- it does not re-run the
// tokenizer just because some *other* tab's class_index rebuild later
// registers or releases an interface name. So a tab that opened before the
// interface it uses was known (or before some other tab released a name it
// itself still needs) would otherwise keep its first-pass colors forever.
// Callers should remember the generation they last colorized against (see
// EditorTab::colored_interface_generation) and, when it's stale, force a
// full recolor -- e.g. via TextEditor::SetLanguage(AvaLang()), which sets
// the editor's internal languageChanged flag -- right before that tab is
// next rendered.
int KnownInterfaceNamesGeneration();

// Same mechanism as UpdateKnownInterfaceNames/KnownInterfaceNamesGeneration
// above, for variable names (assigned with `x = ...`, `x as Type`, or the
// `for x in ...` loop variable) instead of interface names -- see
// EditorTab::known_variable_names / colored_variable_generation.
void UpdateKnownVariableNames(const std::unordered_set<std::string>& removed,
                               const std::unordered_set<std::string>& added);

int KnownVariableNamesGeneration();

void UpdateKnownClassNames(const std::unordered_set<std::string>& removed,
                            const std::unordered_set<std::string>& added);

int KnownClassNamesGeneration();

// Scans `text` for every NAME that AvaLangTokenizer would color as
// Color::variableName -- a plain or augmented assignment target
// (`x = ...`, `x += ...`), a typed declaration/assignment (`x as Type`),
// or a `for x in ...` loop variable -- regardless of whether its type can
// be resolved. Unlike VariableTypeIndex (member_access_resolver.h), which
// only keeps a variable once it can infer a type for it (needed there for
// dot-member-access autocomplete, not here), this keeps every name so a
// plain `msg = ""` still gets tracked. Call this from RebuildIndexAndTrie
// and diff the result against EditorTab::known_variable_names the same
// way interface names are diffed, then hand the diff to
// UpdateKnownVariableNames.
std::unordered_set<std::string> ScanKnownVariableNames(const std::string& text);

}
