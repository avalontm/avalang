#pragma once

#include <string>

#include "design/design_document.h"

namespace studio::design {

// Minimal, hand-written JSON reader/writer for exactly the .avaui
// schema from 08_DESIGNER_VIEW_PLAN.md section 3 -- NOT a general-
// purpose JSON library (same spirit as util/csv.cpp: small and correct
// for the one format that needs it, no external dependency pulled in
// for it). Kept separate from design_document.h/.cpp so the tree-model
// and the text-format concerns don't get tangled -- design_document.cpp
// calls into these two functions and nothing else in here.
//
// File shape:
//   {
//     "design": { "type": "...", "id": "...", "properties": {...},
//                 "events": {...}, "children": [ ... ] },
//     "code": "func ...\nend\n"
//   }
//
// Property/event values round-trip as JSON strings only (matching
// PropertyRow::value's existing "display-ready string, already
// stringified" convention from properties_panel.h) -- a property like
// `"enabled": true` in the conceptual model is written/read here as
// `"enabled": "true"`. This intentionally does not attempt to preserve
// JSON's bool/number/null types; nothing in the Designer needs that
// distinction yet (see 08_DESIGNER_VIEW_PLAN.md section 5.7, property
// *editing* widgets are a later phase and can special-case specific
// keys/types then if it turns out to matter).

// Serializes `root` (a design tree only, no code_behind) to JSON text,
// pretty-printed with 2-space indents for readability when a user opens
// a .avaui file outside Ava Studio.
std::string ComponentTreeToJson(const DesignNode& root);

// Parses JSON produced by ComponentTreeToJson (or hand-written JSON in
// the same shape) back into a tree. Every node gets a fresh node_uid
// (see DesignNode::node_uid) -- uids are never part of the file format.
// Returns false and leaves `out_root` untouched on any parse error,
// with a human-readable reason in `out_error`.
bool ComponentTreeFromJson(const std::string& json, DesignNode& out_root, std::string& out_error);

// Wraps ComponentTreeToJson's output together with `code_behind` into
// the full .avaui file text (the {"design": ..., "code": ...} envelope
// described above).
std::string WriteAvauiText(const DesignNode& root, const std::string& code_behind);

// Inverse of WriteAvauiText: splits the envelope and parses the
// "design" section via ComponentTreeFromJson. Returns false (leaving
// both out-params untouched) if the text isn't valid JSON, is missing
// the "design" key, or "design" itself fails to parse.
bool ParseAvauiText(const std::string& text, DesignNode& out_root, std::string& out_code_behind,
                     std::string& out_error);

} // namespace studio::design
