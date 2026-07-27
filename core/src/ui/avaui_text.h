#ifndef AVA_UI_AVAUI_TEXT_H
#define AVA_UI_AVAUI_TEXT_H

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ui/component.h"

namespace ava {
namespace ui {

// Reader/writer for the .avaui file format -- see
// docs/architecture/08_DESIGNER_VIEW_PLAN.md section 3, and
// AvaLang.UI/Rendering/AvaComponentParser.cs (the C# prototype this is
// a direct port of) in the avalang-dotnet repo.
//
// This lives in core/src/ui/ (compiled into avalang.dll, exposed via
// public/include/avalang.h's ava_ui_parse_avaui_text/ava_ui_write_avaui_text)
// instead of in studio/src/design/ or a host binding's own language,
// specifically so every host (Ava Studio, the .NET binding, any future
// binding) shares one parser instead of hand-porting the same grammar
// N times -- see the design note this replaces:
//   "Dos implementaciones de la misma gramática, mantenidas a mano en
//   paralelo... Cualquier lenguaje nuevo obtiene el parser real gratis,
//   sin reimplementar la gramática."
// studio/src/design/avaui_text.{h,cpp} (DesignNode-based) and
// AvaComponentParser.cs (ComponentNode-based) are the two existing
// implementations this is meant to eventually obsolete -- see PROGRESS.md
// for the migration status of each caller.
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
// same convention the .NET reference relies on, made explicit here
// since our splitter accepts it as the *primary* terminator (falling
// back to "next section keyword" or end-of-file too, so a hand-edited
// file that's missing a closing `end` still loads instead of failing
// outright).
//
// `view` is parsed by indentation into a Component tree -- a type
// keyword alone on its line (`button`, `column`, ...) opens a block
// closed by `end` at the same indent; `key = value` lines inside it
// become properties (or events, for the fixed list of event prop
// names below); a bare `Word()` line (empty parens, PascalCase by
// convention) is a call to an imported component and becomes a leaf
// node with that exact type -- see ComponentResolver.cs in the .NET
// prototype for how that gets resolved to a real subtree there; this
// parser doesn't resolve it (no import-resolution wiring yet on either
// host -- see plan section 9.2 point 1), it just keeps the call node
// so the file round-trips instead of losing it.
//
// Property values: every value is stored as a Value::String holding
// the exact display text -- a value entirely wrapped in "double
// quotes" is a string literal (quotes stripped, \" unescaped);
// anything else (numbers, `true`/`false`, bare identifiers referencing
// `state`, or full expressions like `"Counter: " + counter`) is stored
// verbatim as written. This parser doesn't evaluate expressions (see
// plan section 2 point 3), so there's nothing to normalize there yet.
// On write, the inverse happens: `true`/`false` and anything that
// parses as a plain number are emitted unquoted, everything else is
// quoted (with `"`/`\` escaped). This is a lossy round-trip for the
// expression case above (`"Counter: " + counter` would come back
// double-quoted as a single string) -- acceptable for now since
// neither host writes arbitrary expressions back yet, only plain
// literal default values.
//
// `id = ...` is a reserved property name: it's pulled out into
// Component::SetId instead of landing in the generic properties list.

// Whether a `{name}` route segment is required or optional (`{name?}`)
// -- mirrors AvaLang.UI.Routing.RouteParameterKind in avalang-dotnet.
enum class RouteParameterKind {
    Required,
    Optional,
};

// One `{name}` / `{name?}` / `{name:constraint}` segment inside a
// `route "..."` template's path. `constraint` is the raw constraint
// name as written (e.g. "int", "guid", "slug"), empty when none --
// not resolved/validated here, see AvaComponentParser.cs::ParseConstraint
// in avalang-dotnet for how a host turns this into an IRouteConstraint;
// this parser just carries the name through unchanged.
struct RouteParameter {
    std::string name;
    RouteParameterKind kind = RouteParameterKind::Required;
    std::string constraint;
};

// One `route "/path/{param}"` declaration. A single component/page
// file can have more than one (e.g. a list + detail route sharing a
// page, as seen in avalang-dotnet's productos.avaui) -- see
// ParsedAvaui::routes below.
struct RouteDeclaration {
    // Named `route_template`, not `template` -- that's a reserved word.
    std::string route_template;
    std::vector<RouteParameter> parameters;
};

struct ParsedAvaui {
    // Always non-null, type "page" -- the file's `properties` block
    // becomes this node's own properties (there's no separate `page`
    // keyword in the file format itself); its children are the `view`
    // block's top-level component(s).
    std::shared_ptr<Component> root;
    // The `state` block, key/value in file order. Always strings (see
    // header comment above) -- same "display-ready string" convention
    // used by every property value in this parser.
    std::vector<std::pair<std::string, std::string>> state;
    // `import "..."` lines, verbatim and in file order. Not resolved.
    std::vector<std::string> imports;
    // The `methods` block, verbatim text (the `func ... end` bodies).
    // Real AvaLang source -- not parsed here, the language itself
    // parses it when it runs.
    std::string methods_text;
    // `extends "layout"` line, if present -- a top-level line (column
    // 0), same convention as `import`/`route`. First occurrence wins,
    // same as the .NET reference (AvaComponentParser.cs). Empty when
    // the file doesn't extend a layout.
    std::string extends;
    // `route "/path/{param}"` lines, in file order. See
    // RouteDeclaration above. Empty when the file declares no routes
    // (e.g. a layout or an imported component, not a routable page).
    std::vector<RouteDeclaration> routes;
};

// One node's event-prop names -- shared here so the parser and writer
// (and anything else that needs "is this prop actually an event
// handler binding") agree on the same fixed list. Case-insensitive (an
// .avaui file is not expected to rely on case for these).
bool IsEventPropertyName(const std::string& name);

// Parses a full .avaui file's text. Always succeeds (see the header
// comment above on the forgiving splitter) -- an unrecognized/garbage
// file just yields a mostly-empty page (root with no
// properties/children, empty methods_text/state/imports) instead of
// throwing.
ParsedAvaui ParseAvauiText(const std::string& text);

// Serializes a document back to .avaui text: the full file
// (extends/route/import lines, properties/state/view/methods), not
// just one section. `extends`/`routes` default to "none" so existing
// callers that only care about the other four sections keep compiling
// unchanged.
std::string WriteAvauiText(const Component& root,
                            const std::vector<std::pair<std::string, std::string>>& state,
                            const std::vector<std::string>& imports,
                            const std::string& methods_text,
                            const std::string& extends = "",
                            const std::vector<RouteDeclaration>& routes = {});

// --- JSON helpers for the C API boundary -----------------------------
//
// `state` and `imports` cross the C ABI (ava_ui_parse_avaui_text /
// ava_ui_write_avaui_text in avalang.h) as JSON text rather than a
// bespoke C struct, so any binding (C#, Python, Node, ...) can unpack
// them with its own JSON library instead of hand-marshalling a fixed
// layout -- see avalang.h's comment on those functions. Every state
// value is always a string (see above), so these are deliberately
// simple: a flat {"key": "value", ...} object and a flat
// ["a", "b", ...] array -- not a general-purpose JSON library, and not
// meant to round-trip arbitrary JSON, only what WriteAvauiText/callers
// of ava_ui_parse_avaui_text actually produce/consume here.
std::string StateToJson(const std::vector<std::pair<std::string, std::string>>& state);
std::vector<std::pair<std::string, std::string>> StateFromJson(const std::string& json);
std::string ImportsToJson(const std::vector<std::string>& imports);
std::vector<std::string> ImportsFromJson(const std::string& json);

// `routes` crosses the C ABI the same way, as a JSON array of objects:
// [{"template": "/products/{id}", "parameters": [{"name": "id",
// "optional": false, "constraint": "int"}]}, ...]. `constraint` is
// omitted from an object when empty, same "don't pad the wire format
// with nothing" convention as the rest of this boundary.
std::string RoutesToJson(const std::vector<RouteDeclaration>& routes);
std::vector<RouteDeclaration> RoutesFromJson(const std::string& json);

} // namespace ui
} // namespace ava

#endif // AVA_UI_AVAUI_TEXT_H
