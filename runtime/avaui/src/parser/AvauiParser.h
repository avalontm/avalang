#ifndef AVA_UI_PARSER_AVAUIPARSER_H
#define AVA_UI_PARSER_AVAUIPARSER_H

#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "components/ComponentTree.h"
#include "Export.h"

namespace avalang {
namespace ui {
namespace parser {

// Phase 14 -- standalone .avaui text -> ComponentTree parser. Owns no
// dependency on core/src/ui (Phase 12 decision: ui/ never shares a
// parser with the VM's dynamic .ava pipeline; see
// docs/AVAUI_CONVERGENCE_DECISION.md).
//
// Grammar (see docs/architecture/17_AVAUI_FILE_FORMAT.md):
//   extends <name>            -- bareword or "quoted", at most one
//                                 significant (first wins)
//   route "<pattern>"         -- repeatable
//   import "<path>"           -- repeatable
//   properties ... end        -- key = value pairs (document-level)
//   state ... end             -- key = value pairs (initial state)
//   style ... end             -- key = value pairs
//   code ... end              -- raw text, not parsed by this phase
//                                 (event/lifecycle code-behind belongs
//                                 to a future phase -- see ParseError
//                                 note below)
//   view ... end              -- component tree; each line is either
//                                 `typeName [id]` opening an indented
//                                 block closed by a matching `end`, or
//                                 a no-body call `Name()`.
//
// Indentation is significant: a block's body is every line indented
// deeper than the line that opened it, and the block closes on the
// first `end` back at the opening line's own indentation. Blank lines
// and `#` line comments are ignored anywhere.
//
// Error policy (Fase 14 gap noted in AVAUI_PLAN_FASE12_PLUS.md,
// consistent with LayoutEngine's own fallback for an unrecognized
// TypeName, see LayoutProperties.h):
//   - Structural errors (unterminated block, inconsistent indentation,
//     malformed `key = value`) are HARD failures -- throw ParseError.
//   - Semantic gaps (unknown component type name, a property value the
//     engine doesn't happen to read, an unresolved `Name()` import
//     call) are SOFT -- parsing continues, the node/property is kept
//     as-authored and simply does nothing further down the pipeline,
//     exactly like an unrecognized TypeName already does in Phase 3.
class ParseError : public std::runtime_error {
public:
    ParseError(const std::string& message, int line);
    int Line() const { return line_; }

private:
    int line_;
};

// Phase 19.4 -- one `animate` block parsed from inside a component's
// body in the `view` tree. Grammar (nested inside a component block,
// same indentation rules as any other property block):
//
//   button "myButton"
//     text = "Click me"
//     animate
//       property = "opacity"    -- opacity | position | scale | rotation
//       from = "1.0"            -- number, or "x,y" for position/scale
//       to = "0.4"
//       duration = "0.3"        -- seconds
//       easing = "ease-in-out"  -- see animation::EasingFromString
//       trigger = "click"       -- "click", or a `state` block key name
//       mode = "once"           -- once | loop | pingpong
//     end
//   end
//
// A component may have at most one `animate` block today (Fase 19.4
// scope) -- a second one would simply overwrite the first in
// `properties`-block style parsers, but here it's additive: multiple
// `animate` blocks under the same component each produce their own
// AnimationSpec, so more than one is already supported without extra
// work, just undocumented as a "supported" feature yet.
//
// This phase (like Fase 14's own `state`/`code` blocks) does not
// resolve `from`/`to`/`duration` into real animation::AnimatableValue/
// EasingFunction/PlaybackMode -- that conversion is
// animation::WireAnimations()'s job (ui/include/avalang/ui/animation/
// AnimationBinding.h), which is what actually needs those enums and
// therefore depends on animation/ (parser/ deliberately does not, to
// keep the Components -> Layout -> ... dependency direction in Fwd.h
// intact -- parser depends on components/ only, animation/ depends on
// scene/, so parser must not depend on animation/).
struct AnimationSpec {
    ComponentId target = 0;
    std::string property;       // as authored: "opacity" | "position" | "scale" | "rotation"
    std::string fromRaw, toRaw; // as authored: "1.0" or "x,y"
    std::string duration;       // as authored, seconds (e.g. "0.3")
    std::string easing;         // as authored (e.g. "ease-in-out"), may be empty
    std::string trigger;        // "click" | a `state` block key name | empty (manual only)
    std::string mode;           // "once" | "loop" | "pingpong", may be empty (defaults to "once")
};

// Result of parsing one .avaui document. `tree` is populated only from
// the `view` block; `properties`/`state`/`style` are flat string maps
// -- this phase does not interpret them (Resources/Theme, Phase 15-16,
// are the first consumers that will). `code` is the raw, un-parsed
// text between `code`/`end` (see class comment: code-behind parsing is
// out of scope for Phase 14, which only owns the view -> ComponentTree
// path exercised by Layout/Render/Scene/Commands).
struct ParsedAvaui {
    std::unique_ptr<ComponentTree> tree;
    std::string extends;
    std::vector<std::string> routes;
    std::vector<std::string> imports;
    std::unordered_map<std::string, std::string> properties;
    std::unordered_map<std::string, std::string> state;
    std::unordered_map<std::string, std::string> style;
    std::string code;
    // Phase 19.4 -- one entry per `animate` block found anywhere in
    // the `view` tree (see AnimationSpec comment above).
    std::vector<AnimationSpec> animations;
};

class AVA_UI_API AvauiParser {
public:
    // Throws ParseError on structural failure (see class comment
    // above). Never returns a null `ParsedAvaui::tree` on success --
    // an empty `view` still produces a tree with a synthetic "Page"
    // root and no children.
    static ParsedAvaui Parse(const std::string& source);
};

} // namespace parser
} // namespace ui
} // namespace avalang

#endif // AVA_UI_PARSER_AVAUIPARSER_H