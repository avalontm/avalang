#pragma once

#include "Export.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace avalang::ui::theme {

// One resolved "style" block from a project's declared style file(s),
// holding only the fields the block actually set (see ProjectStyleOverrides.h
// syntax below). Every field is optional so RenderTheme can tell
// "this project didn't configure a margin for buttons" (fall through
// to ITheme) apart from "this project explicitly set margin = 0"
// (use the 0). Mirrors the same property names RenderTheme::
// ApplyTypeDefaults already writes onto components (backgroundColor,
// textColor, fontSize, ...) so wiring a field in here is a 1:1 match
// with an existing `comp->SetProperty(...)` call, not a new naming
// scheme to learn.
struct ControlStyleOverride {
    std::optional<std::string> backgroundColor;
    std::optional<std::string> textColor;
    std::optional<std::string> borderColor;
    // Project-relative path to a .ttf/.otf, same convention as
    // ProjectFontOverrides' `filePath` -- resolved to an absolute
    // path by LoadProjectStyleOverrides, same as font paths are.
    std::optional<std::string> fontName;

    std::optional<double> fontSize;
    std::optional<double> borderWidth;
    std::optional<double> borderRadius;
    // Container-only in practice (RenderTheme only ever fills these
    // for row/column/container/... types today), but nothing stops a
    // project from setting them on any type -- ApplyTypeDefaults
    // still gates which types actually consume "padding"/"spacing"/
    // "margin", same as it already gates ThemeSpacing's fields.
    std::optional<double> padding;
    std::optional<double> margin;
    std::optional<double> spacing; // row/column/grid/scrollview gap

    // Overlays `more` on top of `*this`: any field `more` has set
    // wins, anything `more` leaves empty keeps `*this`'s value.  Used
    // to layer a type-specific `style button` block on top of the
    // project-wide `style *` block.
    void MergeOnto(ControlStyleOverride& base) const;
};

// Loads a project's style sheet(s) and merges them into one
// ProjectStyleSheet -- a simplified, non-cascading analogue of a
// WPF/XAML `<Style TargetType="Button">` block, scoped to exactly the
// properties RenderTheme::ApplyTypeDefaults already knows how to
// default (see ControlStyleOverride's fields).
//
// Unlike fonts (declared directly in app.ava with `font ...`), style
// properties live in their own file(s), declared from app.ava the
// same way:
//
//   style "styles.ava"
//   style "styles.dark.ava"
//
// One or more `style "path/to/file.ava"` lines, same quoted-path
// convention as `font "path"` (see ProjectFontOverrides.h) and parsed
// from app.ava by this same function -- there is no implicit
// "styles.ava" load: a project with no `style "..."` line in app.ava
// gets no style overrides at all, even if a styles.ava happens to
// exist on disk. Declared files are applied in order; where two files
// set the same property for the same target, the later file wins
// (same per-field overlay ControlStyleOverride::MergeOnto already
// uses to layer `style <type>` over `style *`) -- everything else
// from the earlier file is kept. This is how a project splits, say, a
// shared base style from a per-app or per-theme variant instead of
// being limited to one predetermined file.
//
// Each declared file holds one `style <target>` ... `end` block per
// control type, same block/`end` shape .avaui component bodies
// already use so the file reads like the rest of the language instead
// of inventing a new one:
//
//   style *
//       fontName = "assets/fonts/Inter-Regular.ttf"
//       fontSize = 14
//       textColor = "1F2937"
//       padding = 8
//       margin = 4
//   end
//
//   style button
//       backgroundColor = "0078D4"
//       textColor = "FFFFFF"
//       fontSize = 14
//       borderRadius = 6
//   end
//
//   style text
//       fontSize = 14
//       textColor = "333333"
//   end
//
// `<target>` optionally carries a `:<state>` suffix -- `hover`,
// `focus`, `active`, or `disabled` (the most common interactive
// states; case-insensitive) -- to style just that state instead of
// the control's normal/resting appearance:
//
//   style button:hover
//       backgroundColor = "1D4ED8"
//   end
//
//   style textbox:focus
//       borderColor = "2563EB"
//       borderWidth = 2
//   end
//
//   style button:active
//       backgroundColor = "1E40AF"
//   end
//
//   style button:disabled
//       backgroundColor = "9CA3AF"
//       textColor = "E5E7EB"
//   end
//
// A `style <type>:<state>` block only ever contributes the fields it
// itself sets -- it is NOT merged with that type's normal `style
// <type>` block the way a type-specific block is merged with `style
// *`. There is no explicit "unfocus"/"unhover" state to declare
// either: a state block only describes what changes while that state
// is active, and the control simply reverts to its normal style (or
// whatever other state applies) the instant the state ends -- the
// same way CSS `:hover`/`:focus` naturally revert, which is exactly
// how this is rendered on the web target (see
// HTMLRenderer::EmitProjectStateCSS). `style *:hover` (a global
// hover default for every control type, the same relationship `style
// *` has to `style <type>`) is also recognized. An unrecognized state
// after the `:` (a typo, or anything other than the four above) makes
// the whole block silently skipped, same tolerant-skip stance as
// everything else in this file.
//
// State styling only takes effect for control types that get a
// stable, dedicated CSS class in the web renderer's output -- today
// that's `button`, `textbox`, `combobox`, `link`, and `text` (see
// HTMLRenderer.cpp's CssClassForControlType). Structural/container
// types (`row`, `column`, `container`, `dialog`, ...) render through
// a generic, unmarked class and have no selector for a `:hover` rule
// to target, so a `style container:hover` block parses fine but has
// nothing to apply to. State styling also has no effect on the
// desktop (Gdi) renderer today, which paints one static frame per
// layout pass with no hover/focus repaint loop wired up -- it's a
// web-only feature for now.
//
// A `style` target wrapped in quotes instead of a bare word --
// `style "my_button" ... end` -- declares a standalone, NAMED style
// instead of a control-type default. A named style is never applied
// automatically (unlike `style button`): it only takes effect on a
// component that opts in via its own `style = "my_button"` property,
// same idea as an Avalonia/XAML `<Style x:Key="...">` pulled in with
// `Style="{StaticResource ...}"` instead of an implicit
// `TargetType`-only style.
//
//   style "my_button"
//       backgroundColor = "16A34A"
//       textColor = "FFFFFF"
//       borderRadius = 999
//   end
//
// ...then, in an .avaui file:
//
//   button style = "my_button"
//       text = "Test"
//   end
//
// That button gets my_button's fields (backgroundColor, textColor,
// borderRadius) in place of the ordinary `style button` block --
// see RenderTheme::ApplyToComponent, which resolves the component's
// own `style` property against ProjectStyleSheet::ResolveNamed
// before it resolves the type-based `style <type>` block, so the
// named style wins over the type default (component-authored
// properties, other than `style` itself, still win over the named
// style too -- same `!comp->GetProperty(...)` guard as everything
// else in RenderTheme). A named style is a flat, standalone block:
// it is NOT merged with `style *` or `style <type>` the way a
// type-specific block is layered onto the global one, and it
// recognizes no `:<state>` suffix. Two named styles must not share
// a name (case-insensitively) within one project; where they do,
// later-declared fields win field-by-field, same overlay rule
// `style <type>` blocks already follow across multiple declared
// files. A `style` value that doesn't match any declared named
// style (e.g. Button's own built-in "primary"/"secondary" default)
// is simply left alone -- it's not an error, there's just nothing
// to resolve.
//
// A bare target with a dot in it -- `style button.primary` -- scopes
// the block to components of that type that ALSO opt in via their
// own `style = "primary"` property (space-separated: a component can
// list more than one token, e.g. `style = "primary large"`). Unlike
// `style <type>`, a class-scoped block is never applied automatically
// just by being that type -- same "must opt in" idea `style "name"`
// uses, just keyed by (type, class) pair instead of a free-standing
// name, closer to Avalonia/CSS's `Selector="Button.primary"` +
// `Classes="primary"`:
//
//   style button.primary
//       backgroundColor = "007ACC"
//       borderRadius = 4
//   end
//
// ...then, in an .avaui file:
//
//   button style = "primary"
//       text = "Submit"
//   end
//
// A component's `style` property does double duty and is resolved
// left-to-right, one token at a time (see RenderTheme::
// ApplyToComponent): each token first tries a class-scoped `style
// <type>.<token>` block for the component's own type; if there's no
// such block, it falls back to a standalone named `style "token"`
// block instead (see the section above) -- so `style = "primary"` on
// a button resolves `button.primary` if that's declared, or a
// free-standing `style "primary"` block otherwise. A later token
// wins over an earlier one for the same field, same precedence CSS
// itself gives `class="a b"`. There is deliberately only ONE
// property for both of these (not a separate web-flavored `class`/
// `styleClass`): the language isn't web-only, so this stays the same
// `style` keyword the .avaui/styles.ava syntax already uses
// elsewhere, resolved on whichever renderer/platform is active, not
// just HTMLRenderer. This is a DIFFERENT property from the
// pre-existing `class` property, which stays a raw pass-through CSS
// class name/escape hatch (HTMLRenderer-only -- see OnDrawButton and
// friends), untouched by any of this. A `style` token matching
// neither a class-scoped nor a named block simply contributes
// nothing -- e.g. Button's own built-in "primary"/"secondary"
// default is a no-op unless a project actually declares a matching
// `style button.primary`/`style "primary"` block. `style
// <type>.<class>:<state>` (e.g. `style button.primary:hover`) parses
// without error but isn't wired to anything yet -- the HTML
// renderer's `:hover`/`:focus`/etc. CSS today only targets the
// type's fixed class (`.ava-button`, see HTMLRenderer::
// CssClassForControlType), not a per-style-token selector, so a
// class-scoped state block is silently a no-op for now.
//
// `style *` is the project-wide default -- every field it sets
// applies to every control type that doesn't override that same
// field in its own `style <type>` block. `<target>` in `style
// <target>` is matched case-insensitively against IComponent::
// TypeName() (see RenderTheme.cpp's Lowercase()), and accepts the
// same type aliases ApplyTypeDefaults already treats as one family
// (e.g. "row"/"hstack"/"stack" are separate `style` blocks -- there's
// no group syntax; repeat the block per alias if a project wants the
// same look on all of them).
//
// Recognized keys inside a block are exactly ControlStyleOverride's
// fields, written the same as an .avaui property assignment:
// `backgroundColor`, `textColor`, `borderColor`, `fontName` (a
// project-relative path to a .ttf/.otf, or a bare family name to
// leave unmeasured -- same distinction ThemeFont::filePath documents),
// `fontSize`, `borderWidth`, `borderRadius`, `padding`, `margin`,
// `spacing`. Color values are bare "RRGGBB"/"RRGGBBAA" hex (no `#`,
// matching every other color property in this codebase -- see
// controls/*.avaui). Unrecognized keys, blank lines, and `#` comments
// are silently skipped, same tolerance ProjectFontOverrides.h
// documents for app.ava -- a typo'd key should not fail a whole
// project's styling, just that one line.
//
// This never overrides a property an .avaui component sets itself
// (component-authored properties always win -- see RenderTheme.cpp's
// `!comp->GetProperty(...)` guards) or a theme role this file didn't
// mention (falls through to ITheme, same as ProjectTheme does for
// fonts). A project with no `style "..."` line in app.ava, or whose
// declared file(s) have no recognized blocks, gets an empty
// ProjectStyleSheet -- not an error.
class ProjectStyleSheet;

// Reads projectRoot/app.ava for `style "path/to/file.ava"` lines and
// merges every declared file into one ProjectStyleSheet, in
// declaration order (see the header comment above for the merge
// rule). Returns an empty sheet (never an error) if projectRoot is
// empty, app.ava doesn't exist, app.ava has no `style "..."` lines, or
// a declared file doesn't exist -- same tolerant-skip stance
// LoadProjectFontOverrides documents for `font` lines.
AVA_UI_API ProjectStyleSheet LoadProjectStyleOverrides(const std::string& projectRoot);

// Holds every `style` block parsed from a project's declared style
// file(s) (see LoadProjectStyleOverrides). Cheap to construct empty
// (LoadProjectStyleOverrides always returns one, even when app.ava
// declares no style files) so callers never need to null-check before
// passing a ProjectStyleSheet* into RenderTheme::Apply.
class AVA_UI_API ProjectStyleSheet {
public:
    ProjectStyleSheet() = default;

    // Merges the project-wide `style *` block (if any) with the
    // type-specific `style <typeLower>` block (if any) and returns
    // the result -- type-specific fields win, `*` fields fill in the
    // rest, and a field neither block set comes back empty (caller
    // falls through to ITheme). `typeLower` must already be
    // lowercased (RenderTheme.cpp's Lowercase() does this once per
    // component; doing it again here per-field would be wasted work).
    ControlStyleOverride Resolve(const std::string& typeLower) const;

    // True if the project's declared style file(s) set anything at
    // all (a `style *` block, or at least one `style <type>` block)
    // -- lets a caller skip the per-component Resolve() walk entirely
    // for the common case of no declared style files. Not required
    // for correctness (Resolve() on an empty sheet just returns an
    // all-empty override, a no-op), only a fast path.
    bool HasAnyStyles() const { return hasGlobal_ || !perType_.empty() || !classPerType_.empty(); }

    // Resolves the `style *:<state>` (if any) and `style
    // <typeLower>:<state>` (if any) blocks for one interactive state
    // ("hover", "focus", "active", or "disabled" -- see the header
    // comment above), type-specific fields winning over the global
    // ones. Unlike Resolve(), this is deliberately NOT merged with
    // the type's normal/resting style -- a caller renders this as its
    // own CSS rule (see HTMLRenderer::EmitProjectStateCSS) and lets
    // the browser's own cascade apply it only while that state is
    // active, then fall back to the normal style automatically.
    ControlStyleOverride ResolveState(const std::string& typeLower, const std::string& state) const;

    // True if any `style *:<state>` or `style <type>:<state>` block
    // was declared at all -- same fast-path purpose as HasAnyStyles().
    bool HasAnyStateStyles() const { return !stateGlobal_.empty() || !statePerType_.empty(); }

    // Resolves a standalone `style "name"` block (see the header
    // comment above) by name, case-insensitively. Returns an
    // all-empty override when no such name was declared -- same
    // "caller falls through to nothing" contract Resolve() has for
    // an unstyled type. Unlike Resolve(), never merged with `style
    // *`/`style <type>`: a named style is a fully standalone block a
    // component opts into itself (see RenderTheme::ApplyToComponent).
    ControlStyleOverride ResolveNamed(const std::string& name) const;

    // True if `name` matches a declared named `style "..."` block,
    // case-insensitively -- lets a caller tell "this style="..." value
    // names a project style" apart from "this is some other string
    // (e.g. Button's built-in \"primary\"/\"secondary\")" before
    // bothering to call ResolveNamed().
    bool HasNamedStyle(const std::string& name) const;

    // Merges every class-scoped `style <type>.<class>` block named in
    // `classesLower`, in the order given -- a later class wins over
    // an earlier one for the same field (see the header comment's
    // `style <type>.<class>` section). Never merged with `style
    // *`/`style <type>`; RenderTheme::ApplyToComponent layers this in
    // between the named `style="..."` override and the type default.
    // A class with no declared block for this type contributes
    // nothing -- not an error.
    ControlStyleOverride ResolveClasses(const std::string& typeLower,
                                         const std::vector<std::string>& classesLower) const;

private:
    friend ProjectStyleSheet LoadProjectStyleOverrides(const std::string&);
    // Merges one declared style file into an in-progress sheet -- see
    // the .cpp for why this needs direct field access (field-by-field
    // overlay across multiple declared files, not just a whole-sheet
    // replace).
    friend void MergeStyleFileInto(const std::string& styleFilePath, const std::string& projectRoot,
                                    ProjectStyleSheet& sheet);

    std::unordered_map<std::string, ControlStyleOverride> perType_;
    ControlStyleOverride global_;
    bool hasGlobal_ = false;

    // State ("hover"/"focus"/"active"/"disabled") overrides. Keyed
    // separately from perType_/global_ above because these are never
    // merged with the normal style -- see ResolveState(). statePerType_
    // is keyed by "<typeLower>:<state>" (a composite string key,
    // rather than a nested map) since lookups are always by the exact
    // pair together, never by type or state alone.
    std::unordered_map<std::string, ControlStyleOverride> stateGlobal_;
    std::unordered_map<std::string, ControlStyleOverride> statePerType_;

    // Standalone `style "name"` blocks (see ResolveNamed()), keyed by
    // lowercased name. Kept separate from perType_ (a bare `style
    // button` block) so a named style can never accidentally shadow,
    // or be shadowed by, a same-spelled control type.
    std::unordered_map<std::string, ControlStyleOverride> named_;

    // Class-scoped `style <type>.<class>` blocks (see ResolveClasses()),
    // keyed by "<typeLower>.<classLower>" -- a composite string key,
    // same reasoning statePerType_ already uses for "<type>:<state>":
    // lookups are always by the exact (type, class) pair together.
    std::unordered_map<std::string, ControlStyleOverride> classPerType_;
};

} // namespace avalang::ui::theme
