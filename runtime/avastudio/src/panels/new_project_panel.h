#pragma once

#include <optional>
#include <string>

namespace studio {

// The only two variants Fase 6 covers, per §3.1/§9: an empty project (just
// main.ava) or one that also drops a starter ".avaui" screen next to it, so
// there's something to see in the Designer Canvas right after creating a
// project. Deliberately just these two, same "wizard can be this simple"
// scope §9 gives the whole phase -- no manifest, no ava_cli new (doesn't
// exist yet), no arbitrary template list.
enum class NewProjectTemplateKind { kEmpty, kWithScreen };

struct NewProjectState {
    std::string name;
    std::string destination;

    NewProjectTemplateKind template_kind = NewProjectTemplateKind::kEmpty;

    // i18n key of the current validation error, empty when there isn't one
    // -- same "key, not a pre-rendered string" shape ProblemEntry/BuildPanel
    // validation messages already use, so the string is looked up fresh
    // every frame and reacts to a language change immediately.
    std::string error_key;

    // Mirrors CommandPaletteState::focus_query_field / QuickOpenState's own
    // field of the same name -- set by OpenNewProjectDialog, consumed (and
    // cleared) by DrawNewProjectDialog on the next frame it draws.
    bool focus_name_field = false;
};

// What the dialog produces on a successful "Create" -- both paths already
// in the same native/".string()" form Explorer's own DrawCreatePopup uses
// for `ExplorerResult::file_to_open` (not the generic_string() slash-
// normalized form Quick Open uses for its own unrelated reasons), so
// callers can hand project_dir straight to ExplorerState::root_dir and
// entry_file straight to OpenFileInTab without reformatting either.
struct NewProjectResult {
    std::string project_dir;
    std::string entry_file;
};

struct NewProjectDrawResult {
    std::optional<NewProjectResult> created;

    // Same single-purpose idea as BuildBrowseField, just collapsed to one
    // bool -- this dialog only ever browses one field (the destination
    // folder), so a whole enum would be one more indirection with nothing
    // to disambiguate. Caller (main.cpp) reacts the same frame: run the
    // native folder dialog and, if the user picked something, assign it
    // straight to NewProjectState::destination for the next Draw call.
    bool browse_destination_requested = false;
};

// Resets name/template/error to their defaults, prefills `destination` with
// default_destination (callers pass the current Explorer root -- same
// "prefill with what the user was already looking at" reasoning
// OpenQuickOpen/OpenFindInProject already follow with project_root), arms
// focus_name_field, and opens the popup. Callers must invoke this from an
// edge-triggered condition (a want_*/*_requested bool), not every frame --
// same requirement OpenCommandPalette/OpenQuickOpen already have.
void OpenNewProjectDialog(NewProjectState& state, const std::string& default_destination);

// Must be called every frame regardless of whether the dialog is open --
// same requirement DrawCommandPalette/DrawQuickOpen already have. No-ops
// (returns a default-constructed result) whenever the popup isn't open.
//
// On "Create": validates name/destination are non-empty, that destination
// exists as a directory, and that <destination>/<name> doesn't already
// exist (this dialog only ever creates a brand new folder, never writes
// into one that's already there); on failure sets state.error_key and
// keeps the popup open. On success: creates the project folder, writes
// main.ava (and, for kWithScreen, a starter "screen.avaui" reusing the
// exact same boilerplate Fase 5's "Generar" already produces for a new
// screen -- BuildScaffoldContent(ScaffoldKind::kScreen, "Home")), closes
// the popup, and returns the created paths via `created`.
NewProjectDrawResult DrawNewProjectDialog(NewProjectState& state);

}
