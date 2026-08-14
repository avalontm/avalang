#pragma once

#include "Export.h"

#include <optional>
#include <string>
#include <unordered_map>

namespace avalang::ui::theme {

// One resolved `animation <target>:<trigger> ... end` block from a
// project's declared animation file(s) -- the animation-file analogue
// of ControlStyleOverride (see ProjectStyleOverrides.h), holding only
// the fields the block actually set so a caller can tell "this
// project didn't configure this transition" apart from "this project
// explicitly set duration = 0".
//
// Deliberately narrow (four fields) rather than mirroring every field
// the in-.avaui `animate ... end` block accepts (property/from/to/
// duration/easing/trigger/mode, see AvauiParser.cpp's AnimationSpec):
// today exactly one render path actually consumes an animation file's
// values -- HTMLRenderer's dialog open/close fade (EmitHTMLHeader) --
// and that path only ever animates `opacity`, so `property` isn't a
// free-form field here, it's implied by which target the block names.
// See the header comment below for why: the dialog's open/close
// transition animates a flat group of position:absolute siblings
// (backdrop + panel + its children) in unison, and opacity is the
// only CSS property that can do that without becoming a new
// containing block and desyncing their baked-in coordinates. Adding
// more targets later (a per-field `property` value, `transform`-based
// entrance animations on a single non-absolute element, etc.) is a
// matter of widening this struct and its consumer, not a change to
// the file format or the app.ava declaration below.
struct AnimationOverride {
    // CSS opacity value (0-1) the animation starts from.
    std::optional<double> from;
    // CSS opacity value (0-1) the animation ends at.
    std::optional<double> to;
    // Any valid CSS <time> (e.g. "160ms", "0.3s"). Kept as a string,
    // not a parsed number, since it's written straight into the CSS
    // `animation:` shorthand HTMLRenderer already emits -- see
    // EmitHTMLHeader's ava-overlay-fade-in/out keyframes.
    std::optional<std::string> duration;
    // Any valid CSS <easing-function> keyword ("ease-out", "ease-in",
    // "linear", "ease-in-out", ...) or cubic-bezier(...) expression.
    std::optional<std::string> easing;

    // Overlays `more` on top of `*this`: any field `more` has set
    // wins, anything `more` leaves empty keeps `*this`'s value. Same
    // later-file/later-declaration-wins overlay ControlStyleOverride::
    // MergeOnto uses.
    void MergeOnto(AnimationOverride& base) const;
};

// Loads a project's animation sheet(s) and merges them into one
// ProjectAnimationSheet, keyed by "<target>:<trigger>" (today only
// "dialog:open" and "dialog:close" are ever resolved by a renderer,
// see HTMLRenderer::EmitHTMLHeader, but nothing here hardcodes that --
// an unresolved target is simply never looked up).
//
// Declared from app.ava the same way `style "..."` files are (see
// ProjectStyleOverrides.h):
//
//   animation "animations.ava"
//   animation "animations.dark.ava"
//
// One or more `animation "path/to/file.ava"` lines -- same quoted-path
// convention as `style "path"` / `font "path"`, parsed from app.ava by
// this same function. There is no implicit "animations.ava" load: a
// project with no `animation "..."` line in app.ava gets no animation
// overrides at all (HTMLRenderer falls back to its own hardcoded
// 160ms ease-out/ease-in dialog fade), even if an animations.ava
// happens to exist on disk. Declared files are applied in declaration
// order; where two files set the same target:trigger, the later file
// wins field-by-field (same overlay AnimationOverride::MergeOnto
// documents above) -- everything else from the earlier file is kept.
//
// Each declared file holds one `animation <target>:<trigger> ... end`
// block per transition, same block/`end` shape every other AvaLang
// file (styles.ava, .avaui bodies, the in-component `animate` block)
// already uses, so the file reads like the rest of the language
// instead of inventing a new one:
//
//   animation dialog:open
//       from = 0
//       to = 1
//       duration = "220ms"
//       easing = "ease-out"
//   end
//
//   animation dialog:close
//       from = 1
//       to = 0
//       duration = "180ms"
//       easing = "ease-in"
//   end
//
// `<target>` is matched case-insensitively; `:<trigger>` is required
// (unlike `style <type>`, there is no target-only, trigger-less
// `animation dialog` block -- an animation is meaningless without
// knowing which transition it drives). Today only `open` and `close`
// are recognized triggers for `dialog` -- the only render path wired
// up to consume them (see HTMLRenderer::EmitHTMLHeader) -- but, same
// tolerant-skip stance as everything else in this file family, an
// unrecognized trigger or target still parses fine, it's just never
// looked up by anything.
//
// Recognized keys inside a block: `from`, `to` (bare numbers 0-1, or
// percentages like "100%" -- both normalize to a 0-1 opacity value),
// `duration`, `easing` (bare or quoted CSS strings, same
// quote-optional tolerance ProjectStyleOverrides.h documents for its
// own values). Unrecognized keys, blank lines, and `#` comments are
// silently skipped -- a typo'd key should not fail a whole project's
// animation file, just that one line.
//
// A project with no `animation "..."` line in app.ava, or whose
// declared file(s) have no recognized blocks, gets an empty
// ProjectAnimationSheet -- not an error, and every renderer that
// consumes one treats "not declared" as "use the built-in default"
// rather than "no animation at all".
class ProjectAnimationSheet;

// Reads projectRoot/app.ava for `animation "path/to/file.ava"` lines
// and merges every declared file into one ProjectAnimationSheet, in
// declaration order (see the header comment above for the merge
// rule). Returns an empty sheet (never an error) if projectRoot is
// empty, app.ava doesn't exist, app.ava has no `animation "..."`
// lines, or a declared file doesn't exist -- same tolerant-skip
// stance LoadProjectStyleOverrides documents.
AVA_UI_API ProjectAnimationSheet LoadProjectAnimationOverrides(const std::string& projectRoot);

// Holds every `animation <target>:<trigger>` block parsed from a
// project's declared animation file(s) (see
// LoadProjectAnimationOverrides). Cheap to construct empty (that
// function always returns one, even when app.ava declares no
// animation files) so callers never need to null-check before passing
// a ProjectAnimationSheet* into HTMLRenderer::SetProjectAnimations.
class AVA_UI_API ProjectAnimationSheet {
public:
    ProjectAnimationSheet() = default;

    // Resolves the `animation <typeLower>:<trigger>` block, if any.
    // `typeLower`/`trigger` must already be lowercased (same
    // caller-lowercases-once convention ProjectStyleSheet::Resolve
    // documents). Returns an all-empty override (every field
    // std::nullopt) when nothing was declared for this target:trigger
    // -- callers fall back to their own built-in default per field,
    // same "caller falls through to nothing" contract
    // ProjectStyleSheet::ResolveNamed has for an undeclared name.
    AnimationOverride Resolve(const std::string& typeLower, const std::string& trigger) const;

    // True if the project's declared animation file(s) set anything
    // at all -- lets a caller skip Resolve() entirely for the common
    // case of no declared animation files. Not required for
    // correctness (Resolve() on an empty sheet just returns an
    // all-empty override, a harmless no-op), only a fast path.
    bool HasAnyAnimations() const { return !targets_.empty(); }

    friend void MergeAnimationFileInto(const std::string& animationFilePath,
                                        ProjectAnimationSheet& sheet);

private:
    // Keyed by "<typeLower>:<triggerLower>", e.g. "dialog:open".
    std::unordered_map<std::string, AnimationOverride> targets_;
};

} // namespace avalang::ui::theme
