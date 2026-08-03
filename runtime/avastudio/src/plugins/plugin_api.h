#ifndef AVA_STUDIO_PLUGIN_API_H
#define AVA_STUDIO_PLUGIN_API_H

// Ava Studio Plugin ABI -- Fase 0.
//
// Dear ImGui has no stable ABI between compilations: a .dll/.so built
// separately from ava_studio.exe cannot call ImGui::Begin() safely
// unless it was built with the exact same compiler/flags/ImGui commit
// as the host -- any later recompile of Ava Studio would silently break
// every third-party plugin. So a plugin never links ImGui (or any C++
// Ava Studio header) directly. It talks to the host through this file
// only: a plain C struct of function pointers, versioned, the same
// pattern AvaHost already uses for its own Stable C API (see
// runtime/avalang/api/include/avalang.h) and the same spirit as
// avaui's IRenderer (DrawRectangle/DrawText/DrawButton) -- a small,
// closed set of drawing primitives instead of raw ImGui access.
//
// A plugin is a shared library exporting exactly these three C
// symbols (see the *_SYMBOL names below):
//   int  ava_plugin_abi_version(void);
//   bool ava_plugin_init(AvaStudioHost* host);
//   void ava_plugin_shutdown(void);
//
// ava_plugin_abi_version() is checked BEFORE the host ever calls
// ava_plugin_init() with a live AvaStudioHost* -- a plugin built
// against an older/newer ABI gets rejected outright instead of being
// handed a struct whose layout it might misinterpret (fields only ever
// get appended at the end in a new AVA_STUDIO_PLUGIN_ABI_VERSION, never
// reordered or removed -- see the note above the struct below).
//
// Everything here is plain C (no C++ name mangling, no STL types
// crossing the boundary) so it works from any language/compiler that
// can produce a native shared library, not just the same MSVC version
// Ava Studio itself is built with.

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Bump this whenever AvaStudioHost's layout changes. New fields are
// only ever appended at the end of AvaStudioHost / AvaUiApi /
// AvaHostServices -- never inserted in the middle, reordered, or
// removed -- so a plugin built against ABI N can still be loaded (and
// simply not use) the fields a newer host added in ABI N+1. The host
// itself still only *offers* to load a plugin whose reported version
// is <= its own; see plugin_host.cpp.
// Fase 5 (see PLAN_agente_ia_openrouter.md) appended AvaHostServices::
// apply_edit/run_project at the end of the struct -- existing fields
// keep the same offsets, so ABI 1 plugins still load fine (see the
// note above), they just don't see these two.
// Fase 6 appended AvaHostServices::design_add_component/
// design_edit_component -- same rule, ABI 1/2 plugins still load,
// they just don't see these.
// Fase 8 appended AvaUiApi::input_text_multiline_submit -- same rule,
// ABI 1/2/3 plugins still load, they just don't see it (and keep using
// plain input_text_multiline, where Enter always inserts a newline).
// Fase 9 appended AvaUiApi::input_text_multiline_submit_hint and
// button_disabled -- same rule again.
#define AVA_STUDIO_PLUGIN_ABI_VERSION 5

typedef struct AvaStudioHost AvaStudioHost;

// Opaque per-panel drawing context. A plugin never looks inside this --
// it only ever passes the pointer it was handed straight back into the
// AvaUiApi calls. Host-side, this identifies which ImGui window
// (panel) the primitive should draw into this frame.
typedef struct AvaPanelContext AvaPanelContext;

// Where the host tries to dock a newly-registered panel the very
// first time Ava Studio runs (mirrors the existing
// ImGui::DockBuilderDockWindow calls in main.cpp). Purely a hint for
// that first layout -- like every other panel, the user can drag it
// anywhere afterward and ImGui remembers that for next time.
typedef enum AvaDockSlot {
    AVA_DOCK_LEFT = 0,
    AVA_DOCK_RIGHT = 1,
    AVA_DOCK_BOTTOM = 2,
    AVA_DOCK_CENTER = 3,
} AvaDockSlot;

// Called once per frame while the panel is visible/docked, same as any
// built-in panel's DrawXPanel(). `user_data` is whatever the plugin
// passed to register_panel() -- typically a pointer to the plugin's
// own state struct (chat history, input buffer, etc.), since the
// plugin has no other hook into "per-frame" than this callback.
typedef void (*AvaPanelDrawFn)(AvaPanelContext* ctx, void* user_data);

// --- UI primitives ----------------------------------------------------
// Minimal set to build a usable panel (a chat view is the driving
// case -- see PLAN_agente_ia_openrouter.md Fase 1): text, a button,
// single/multi-line text input, a combo box, and a scrollable child
// region to hold a growing list of messages. Extend this list later
// (Fase 7+) rather than letting plugins reach for ImGui directly --
// that's the whole point of the boundary.
typedef struct AvaUiApi {
    void (*label)(AvaPanelContext* ctx, const char* text);

    // Wraps at the panel's width, unlike label() -- use for anything
    // longer than one short line (a chat message body, an error
    // message, etc).
    void (*text_wrapped)(AvaPanelContext* ctx, const char* text);

    bool (*button)(AvaPanelContext* ctx, const char* label);

    // Same in/out contract as ImGui::InputText: `buffer` is the
    // plugin's own storage (a member of its state struct), `buffer_size`
    // is its capacity in bytes. Returns true the frame the text
    // changed. `label` is also the widget's ImGui id -- must be unique
    // within this panel (append "##something" for a blank visible
    // label, exactly like real ImGui).
    bool (*input_text)(AvaPanelContext* ctx, const char* label, char* buffer, size_t buffer_size);

    // Same as input_text but multi-line; `height` is in pixels (pass 0
    // for ImGui's own default).
    bool (*input_text_multiline)(AvaPanelContext* ctx, const char* label, char* buffer, size_t buffer_size,
                                  float height);

    // `items`/`items_count`: a plain array of C strings, not a single
    // "a\0b\0c\0" blob -- simpler to build from a plugin's own
    // std::vector<std::string> (collect .c_str()s into a temporary
    // array before calling). `current_index` is read on entry and
    // written back on change; returns true the frame the selection
    // changed.
    bool (*combo)(AvaPanelContext* ctx, const char* label, int* current_index, const char* const* items,
                  int items_count);

    void (*separator)(AvaPanelContext* ctx);
    // Places the next widget on the same line as the previous one --
    // EXCEPT the host may still drop it to a new line anyway if the
    // panel isn't wide enough for it (a narrow docked panel is common:
    // Explorer/Editor/Output all fight for width too). This is
    // deliberate: an input_text()+same_line()+button() row is a
    // completely normal thing to write, and the alternative (the host
    // honoring same_line() unconditionally) is a button or field
    // silently clipped past the panel's edge with no way back to it
    // short of a horizontal scrollbar -- worse for reading text or
    // using a form either way. A plugin never needs to compute panel
    // width itself to avoid this; just call same_line() normally.
    void (*same_line)(AvaPanelContext* ctx);
    void (*spacing)(AvaPanelContext* ctx);

    // Scrollable child region -- `id` must be unique within the panel.
    // begin_child returns false if the region is clipped/collapsed
    // (mirrors ImGui::BeginChild); a plugin should still call
    // end_child() unconditionally either way, exactly like real ImGui.
    bool (*begin_child)(AvaPanelContext* ctx, const char* id, float height);
    void (*end_child)(AvaPanelContext* ctx);

    // Scrolls the *current* child region (call between begin_child/
    // end_child) to its bottom -- the "stick to latest message while
    // streaming" behavior a chat panel needs every frame new text
    // arrives.
    void (*scroll_to_bottom)(AvaPanelContext* ctx);

    // Fase 8: same widget as input_text_multiline, but wired for chat-
    // style submission -- plain Enter submits (reports it through
    // `out_submit` and does NOT insert a newline), Shift+Enter inserts
    // a newline like any other character. `out_submit` may be NULL if
    // the caller doesn't care (equivalent to plain input_text_multiline
    // plus swallowing bare Enter). Returns true the frame the buffer's
    // text changed, exactly like input_text_multiline -- check
    // `*out_submit` separately to know whether that change was "typed a
    // character" or "hit Enter to send".
    bool (*input_text_multiline_submit)(AvaPanelContext* ctx, const char* label, char* buffer, size_t buffer_size,
                                         float height, bool* out_submit);

    // Fase 9: same as input_text_multiline_submit, but shows `hint` as
    // greyed-out placeholder text whenever the buffer is empty (like
    // ImGui::InputTextWithHint) -- so an empty chat box reads e.g.
    // "Describe qué construir" instead of sitting there blank. Pass ""
    // (or NULL) for no hint, which behaves exactly like
    // input_text_multiline_submit above.
    bool (*input_text_multiline_submit_hint)(AvaPanelContext* ctx, const char* label, const char* hint,
                                              char* buffer, size_t buffer_size, float height, bool* out_submit);

    // Same as button(), but visually greyed out and unclickable while
    // `disabled` is true -- always returns false in that case. For a
    // send button that should only "light up" once the user has typed
    // something.
    bool (*button_disabled)(AvaPanelContext* ctx, const char* label, bool disabled);

    // Pixel height of one line of text at the current font/size --
    // lets a plugin size a growing multiline input (N lines typed ->
    // N * text_line_height() + padding) without needing raw ImGui
    // access just for that one measurement.
    float (*text_line_height)(AvaPanelContext* ctx);
} AvaUiApi;

// --- Read-only host services (Fase 0) ----------------------------------
// Fase 3 (automatic context) and Fase 4 (read-only tool calling) are
// both just the ai_agent plugin calling these same three getters --
// nothing new needs to be added to the host for them, which is the
// point of exposing them here from the start rather than only once a
// consumer needs them.
typedef struct AvaHostServices {
    // Absolute path to the currently open project's root folder (the
    // same folder Explorer is rooted at -- see ExplorerState::root_dir).
    // Never null; returns an empty string if nothing meaningful is
    // open yet. Host-owned, valid only until the next host call --
    // copy it (e.g. into a std::string) if you need it past this frame.
    const char* (*get_project_root)(AvaStudioHost* host);

    // The active editor tab, if any. Returns false (and leaves every
    // out-param untouched) when no tab is open, or the active tab is
    // the startup Welcome tab. `out_path` is "" for an unsaved/untitled
    // buffer. `out_selection_start`/`out_selection_end` are UTF-8 byte
    // offsets into `out_content`, or both -1 if the host build in use
    // doesn't report a selection yet (true today -- selection
    // reporting lands with Fase 3, this signature is already shaped
    // for it so plugins don't need updating when it does). All
    // pointers are host-owned, valid only for this frame.
    bool (*get_active_file)(AvaStudioHost* host, const char** out_path, const char** out_content,
                             int* out_selection_start, int* out_selection_end);

    // The most recent Run/compile result this session (see
    // panels/terminal_panel.h / EngineBridge::RunScript). Returns false
    // if nothing has been run yet. `out_text` is the same summary
    // string shown in the Output panel ("OK -> ..." or the error
    // text); host-owned, valid only for this frame.
    bool (*get_last_run_output)(AvaStudioHost* host, const char** out_text, bool* out_had_error);

    // Prints one line into Ava Studio's own Output panel, prefixed
    // with the plugin's registered name -- lets a plugin surface
    // diagnostics or results the same place a failed compile already
    // does, instead of needing its own separate log surface.
    void (*log)(AvaStudioHost* host, const char* message);

    // --- Write services (Fase 5) ---------------------------------------
    // Both of these can be called from any thread the plugin owns (not
    // just the frame that draws its panel) -- the ai_agent plugin's
    // tool-use loop, for example, runs on its own worker thread (see
    // ai_agent_plugin.cpp's SendMessage). The host marshals the actual
    // work back onto its main thread internally; a plugin never needs
    // to know or care which thread it called these from.

    // Proposes replacing the *entire* contents of `path` (relative to
    // the open project's root, same rules as read_file: no `../`, no
    // absolute override) with `new_content`. Never writes anything by
    // itself -- queues the proposal for the person to review (the host
    // shows a diff against the file's current contents, with Aplicar/
    // Rechazar buttons) and returns immediately, before it's been
    // decided either way. `description` is a short human-readable
    // summary of the change shown above the diff (e.g. "Arreglar el
    // typo en el mensaje de error"); pass "" if there's nothing worth
    // saying beyond the diff itself.
    //
    // Returns true once the proposal is queued (this says nothing
    // about whether the person will approve it -- a plugin has no way
    // to find that out today, Fase 5 is fire-and-forget from here).
    // Returns false if `path` doesn't resolve inside the project root,
    // no project is open, or `path`/`new_content` is null -- in which
    // case `*out_error` (if non-null) is set to a host-owned message
    // describing why, valid only until the next call into this host on
    // the calling thread.
    bool (*apply_edit)(AvaStudioHost* host, const char* path, const char* new_content, const char* description,
                        const char** out_error);

    // Runs the exact same compile+run pipeline as pressing F5 on the
    // currently active editor tab, and blocks the calling thread until
    // it actually finishes -- unlike apply_edit, there is nothing to
    // approve here (running code the person can already see and Run
    // themselves isn't a new capability the way writing a file is), so
    // this can just do the work and hand back a real result.
    //
    // Returns true with `*out_output`/`*out_had_error` filled in
    // (host-owned, valid only until the next call into this host on
    // the calling thread -- copy what you need before making another
    // one) once the run has completed. Returns false if there was no
    // real tab open to run (no tabs, or only the Welcome tab) --
    // `*out_error` (if non-null) explains why.
    bool (*run_project)(AvaStudioHost* host, const char** out_output, bool* out_had_error, const char** out_error);

    // --- Design services (Fase 6) ---------------------------------------
    // Both work against the ACTIVE editor tab's .avaui document (see
    // AvaHostServices::get_active_file for the general "active tab"
    // notion) and, like apply_edit, never write anything by
    // themselves: they compute the resulting AvaLang UI source and
    // queue it as a normal apply_edit-style proposal for the person to
    // review (Aplicar/Rechazar) against the tab's file path. This is
    // deliberately the SAME approval gate as apply_edit, not a second
    // one -- "el agente nunca escribe ni ejecuta nada sin confirmacion
    // explicita del usuario" (see the plan's principio rector) has to
    // hold here exactly as it does for a plain text edit. Returns
    // false if the active tab isn't a .avaui document, or (design_add_
    // component only) if `parent_id`/`type` don't resolve to a real
    // container / known catalog type -- `*out_error` (if non-null)
    // explains why, host-owned, valid only until the next call on the
    // calling thread.

    // Adds a new component of `type` as the last child of the node
    // whose id is `parent_id` (pass "" for the document's root).
    // `properties_kv` is a flat "key=value;key2=value2" string (no
    // escaping of ';'/'=' inside a value -- keep property values that
    // need those out of this call, same limitation apply_edit's
    // single-string interface already has for its own args). Returns
    // true once the proposal is queued.
    bool (*design_add_component)(AvaStudioHost* host, const char* parent_id, const char* type, const char* id,
                                  const char* properties_kv, const char** out_error);

    // Edits the existing component whose id is `node_id`: overlays
    // `properties_kv` (same flat format as design_add_component)
    // onto its current properties, and renames it to `new_id` if
    // `new_id` is non-null and non-empty. Returns true once the
    // proposal is queued; false (besides the reasons above) if
    // `node_id` doesn't match any node in the active document.
    bool (*design_edit_component)(AvaStudioHost* host, const char* node_id, const char* properties_kv,
                                   const char* new_id, const char** out_error);
} AvaHostServices;

// --- Panel registration -------------------------------------------------
typedef struct AvaPanelRegistration {
    // Also the docked tab's title, and the id register_panel() uses to
    // reject a duplicate. Must outlive the call to register_panel (the
    // host copies it internally, so a stack buffer used only for this
    // one call is fine).
    const char* name;
    AvaPanelDrawFn draw;
    void* user_data; // passed back to every draw() call unchanged
    AvaDockSlot default_dock_slot;
} AvaPanelRegistration;

// Handed to the plugin in ava_plugin_init(). Every field is set by the
// host before that call and stays valid until ava_plugin_shutdown()
// returns -- a plugin must not keep using `host` (or anything reached
// through it) after that point.
struct AvaStudioHost {
    // Set by the host to AVA_STUDIO_PLUGIN_ABI_VERSION at the time
    // Ava Studio itself was built. A plugin can check this if it wants
    // to feature-detect a newer host at runtime, but the host has
    // already validated ava_plugin_abi_version() before calling
    // ava_plugin_init() at all -- see plugin_host.cpp.
    int abi_version;

    // Reserved for the host's own bookkeeping (Ava Studio's PluginHost
    // instance, see plugin_host.cpp). A plugin must never read, write,
    // or make any assumption about this field -- it exists purely so
    // the host's C function-pointer callbacks (register_panel,
    // services.*, which only ever receive `AvaStudioHost*`) can find
    // their way back to the C++ object that owns them, without relying
    // on `host` being at any particular offset inside that object.
    void* _internal_host_reserved;

    AvaUiApi ui;
    AvaHostServices services;

    // Registers a panel to be docked and drawn every frame from now
    // until ava_plugin_shutdown(). Only safe to call from within
    // ava_plugin_init() (Fase 0 does not support registering panels
    // later, e.g. from inside a draw callback). Returns an opaque
    // panel id (>= 0), or -1 on failure -- e.g. `registration->name`
    // is already registered, or `registration` is malformed
    // (name/draw null).
    int (*register_panel)(AvaStudioHost* host, const AvaPanelRegistration* registration);
};

// Every plugin .dll/.so must export exactly these three C symbols
// (extern "C" if the plugin itself is written in C++, so the linker
// doesn't mangle the names).
typedef int (*AvaPluginAbiVersionFn)(void);
typedef bool (*AvaPluginInitFn)(AvaStudioHost* host);
typedef void (*AvaPluginShutdownFn)(void);

#define AVA_PLUGIN_ABI_VERSION_SYMBOL "ava_plugin_abi_version"
#define AVA_PLUGIN_INIT_SYMBOL "ava_plugin_init"
#define AVA_PLUGIN_SHUTDOWN_SYMBOL "ava_plugin_shutdown"

// --- Plugin metadata (Fase 9) --------------------------------------------
// Purely informational, shown next to a plugin's checkbox in the
// "Plugins" menu (see titlebar_panel.cpp) so a person can tell what
// they're enabling/disabling without opening the file itself --
// display name, version, and who made it. Unlike the three symbols
// above, a plugin does NOT have to export any of these: the host
// resolves each independently and simply leaves that field blank in
// the menu if it's missing, never rejects the plugin over it (same
// "optional, additive" spirit as fields appended to AvaUiApi/
// AvaHostServices -- see the ABI note up top -- except these aren't
// even part of the versioned struct, so they don't need an ABI bump
// either).
//
// The returned pointer must stay valid for as long as the plugin's
// library stays loaded (a string literal, e.g. `return "1.2.0";`,
// trivially satisfies this -- don't return a pointer into a buffer
// that could be freed or overwritten).
typedef const char* (*AvaPluginMetadataFn)(void);

#define AVA_PLUGIN_DISPLAY_NAME_SYMBOL "ava_plugin_display_name" // e.g. "AI Agent (OpenRouter)"
#define AVA_PLUGIN_VERSION_SYMBOL "ava_plugin_version"           // e.g. "1.3.0" -- any format, shown as-is
#define AVA_PLUGIN_AUTHOR_SYMBOL "ava_plugin_author"             // e.g. "Jane Doe"

#ifdef __cplusplus
}
#endif

#endif // AVA_STUDIO_PLUGIN_API_H
