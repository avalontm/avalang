#pragma once

#include <string>
#include <vector>

#include "design/design_document.h"

namespace studio::design {

// Reader/writer for the REAL .avaui file format --
// docs/architecture/08_DESIGNER_VIEW_PLAN.md section 3 (updated after
// section 0.1's finding: this is NOT JSON, it's the same AvaLang UI
// text syntax that already runs in the AvaLang.UI .NET prototype --
// see AvaLang.UI/Rendering/AvaComponentParser.cs, which is the direct
// reference this file ports to C++). Replaces the earlier
// avaui_json.{h,cpp} (a JSON envelope) written before that finding.
//
// File shape (see the plan doc for the full worked example):
//
//   import "components/navbar"
//
//   properties
//       title = "Mi App"
//   end
//
//   state
//       counter = 0
//   end
//
//   view
//       column
//           fill = "true"
//           padding = 20
//
//           Navbar()
//
//           text
//               value = "Counter: " + counter
//               fontSize = 32
//           end
//
//           button
//               text = "Guardar"
//               click = btnGuardar_Click
//           end
//       end
//   end
//
//   methods
//       func btnGuardar_Click()
//           -- handler
//       end
//   end
//
// Every top-level block (`properties`/`state`/`view`/`methods`) starts
// at column 0 and is closed by a matching `end` also at column 0 --
// same convention AvaComponentParser.cs relies on, just made explicit
// here since our splitter accepts it as the *primary* terminator
// (falling back to "next section keyword" or end-of-file too, so a
// hand-edited file that's missing a closing `end` still loads instead
// of failing outright -- same forgiving spirit as the .NET parser,
// which never throws on structural issues, only on VM.Eval calls it
// doesn't make here).
//
// `view` is parsed by indentation into the DesignNode tree -- a type
// keyword alone on its line (`button`, `column`, ...) opens a block
// closed by `end` at the same indent; `key = value` lines inside it
// become properties (or events, for the fixed list of event prop
// names below); a bare `Word()` line (empty parens, PascalCase by
// convention) is a call to an imported component and becomes a leaf
// node with that exact type -- see ComponentResolver.cs in the .NET
// prototype for how that gets resolved to a real subtree there; this
// Designer doesn't resolve it yet (no multi-file Explorer wiring for
// .avaui imports -- see plan section 3), it just keeps the call node
// so the file round-trips instead of losing it.
//
// Property values: a value entirely wrapped in "double quotes" is a
// string literal (quotes stripped, \" unescaped) and is stored in
// PropertyRow::value unquoted, matching PropertyRow's existing
// "display-ready string, already stringified" convention (see
// panels/properties_panel.h). Anything else (numbers, `true`/`false`,
// bare identifiers referencing `state`, or full expressions like
// `"Counter: " + counter`) is stored verbatim as written -- this
// Designer doesn't evaluate expressions (see plan section 2 point 3),
// so there's nothing to normalize there yet. On write, the inverse
// happens: `true`/`false` and anything that parses as a plain number
// are emitted unquoted, everything else is quoted (with `"`/`\`
// escaped). This is a lossy round-trip for the expression case above
// (`"Counter: " + counter` would come back double-quoted as a single
// string) -- acceptable for now since the Designer only ever *writes*
// plain literal default values (see design/component_catalog.cpp);
// editing arbitrary expressions through the Properties panel is a
// later phase (PROPERTIES_EDITABLE, see panels/properties_panel.cpp).
//
// `id = ...` is a reserved property name: it's pulled out into
// DesignNode::id / DesignDocument.root's page-level id instead of
// landing in the generic `properties` vector.

// One node's event-prop names -- shared here so parser and writer
// (and anything else that needs "is this prop actually an event
// handler binding") agree on the same fixed list. Case-insensitive
// (an .avaui file is not expected to rely on case for these).
bool IsEventPropertyName(const std::string& name);

// Serializes the whole in-memory document back to .avaui text: the
// full file (properties/state/view/methods, plus any `import` lines
// preserved verbatim from the last parse), not just one section --
// this is what F7 (Design -> Code) shows verbatim in the TextEditor,
// see panels/editor_panel.cpp's ToggleTabViewMode.
std::string WriteAvauiText(const DesignNode& root, const std::string& code_behind,
                            const std::vector<PropertyRow>& initial_state,
                            const std::vector<std::string>& imports);

// Parses a full .avaui file's text. Always succeeds (see the header
// comment above on the forgiving splitter) -- `out_error` exists for
// interface symmetry with the old JSON parser and any future case
// that does want to surface a hard failure, but nothing in this
// parser sets it today; an unrecognized/garbage file just yields a
// mostly-empty page (root with no properties/children, empty
// code_behind/initial_state/imports) instead of failing the tab open.
// `out_root` always comes back as a single synthetic "page" node
// (see design_document.h's NewBlankAvauiDocument) whose `properties`
// come from the file's `properties` block and whose `children` are
// the `view` block's top-level component(s) -- there's no separate
// "page" keyword in the file format itself, section 3's `properties`
// block *is* the page's own properties.
bool ParseAvauiText(const std::string& text, DesignNode& out_root, std::string& out_code_behind,
                     std::vector<PropertyRow>& out_initial_state, std::vector<std::string>& out_imports,
                     std::string& out_error);

} // namespace studio::design
