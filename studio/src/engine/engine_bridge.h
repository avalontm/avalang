#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "avalang.h"

namespace studio {

// Result of a compile+run cycle. Kept mainly so callers (main.cpp) can
// check success/failure without scanning the console; the Output panel
// itself now reads the full scrollback from EngineBridge::Console()
// instead of just the last result (see below).
struct RunResult {
    bool success = false;
    std::string message; // final summary ("OK -> ...") or error text

    // Source position of the failure, 1-based (0 = unknown -- e.g. an
    // error with no meaningful line, or success). Mirrors
    // ava_last_error_line/ava_last_error_column (see avalang.h), read
    // right after the failing ava_compile/ava_run call in RunScript().
    // Only meaningful when !success.
    int error_line = 0;
    int error_column = 0;

    // File the failure actually happened in, mirroring
    // ava_last_error_source (see avalang.h). Empty when the VM didn't
    // know (e.g. a proto compiled before source_name tracking existed) --
    // callers should fall back to the file that was run in that case,
    // since a top-level error is in that same file. Non-empty and
    // different from the run's own source_name means the error is inside
    // an `import`ed module: main.cpp opens *that* file's tab so the
    // highlighted line points at the real offending code instead of
    // wherever the import statement happens to sit.
    std::string error_source;
};

// One line of the Output panel's execution console -- built to feel like
// a real terminal (stdout as the script prints it, plus markers for run
// boundaries and results) instead of a static "last result" label.
struct ConsoleLine {
    enum class Kind {
        Info,    // "> Run script.ava" separators
        Stdout,  // one line of accumulated print() output
        Error,   // compile or runtime error. AvaLang's compile errors are
                 // already formatted multi-line with the offending
                 // source line and a "^" column caret (see
                 // core/src/frontend/frontend_antlr.cpp), so this can
                 // itself contain embedded '\n's -- render with
                 // TextUnformatted, not Text, to keep them.
        Success, // final "OK" / "OK -> <value>" summary of a successful run
        Input,   // echo of text submitted via the console's input box
    };
    Kind kind;
    std::string text;

    // Only meaningful when kind == Error. Mirrors RunResult::error_source/
    // error_line/error_column (see below) -- carried per-line, not just
    // on the RunResult, so a *previous* run's error line still knows
    // where to jump to even after a later run has overwritten
    // OutputState::last_run. source empty + line 0 means "unknown",
    // same convention as RunResult. The Output panel uses these to make
    // the line clickable (see output_panel.cpp).
    std::string error_source;
    int error_line = 0;
    int error_column = 0;
};

// Thin C++ wrapper around the avalang C API (public/include/avalang.h).
// Owns one AvaVM for the lifetime of the studio session.
//
// This is intentionally NOT a generic "engine service" abstraction --
// it only wraps exactly what the Milestone 1 panels need. Grow it as
// the Designer/Toolbox/live-reload panels come online.
class EngineBridge {
public:
    EngineBridge();
    ~EngineBridge();

    EngineBridge(const EngineBridge&) = delete;
    EngineBridge& operator=(const EngineBridge&) = delete;

    // Compiles and runs `source` (source_name is only used for error
    // messages, e.g. the file path). Does not persist any state between
    // calls beyond the shared AvaVM globals. Every call appends to the
    // console scrollback (see Console() below) rather than replacing it,
    // so a session's whole run history stays visible -- like a real
    // terminal, not a single-shot result panel.
    RunResult RunScript(const std::string& source, const std::string& source_name);

    // Sets the base modules folder `import` falls back to when a module
    // isn't found relative to the running script (see
    // ava_vm_set_stdlib_path in avalang.h). Called once at startup with
    // the persisted setting (util/settings.h), and again any time the
    // user changes it via the Properties dialog -- takes effect on the
    // next RunScript(), no restart needed. `path` empty means "use the
    // default modules/ folder next to the executable" (see
    // util::ResolveDefaultModulesDir()) -- this is where that's resolved,
    // so the caller/settings file can stay portable and keep storing "".
    void SetModulesPath(const std::string& path);

    // The execution console: every print() from every RunScript() call
    // this session, in order, plus the Run/error/result markers around
    // them. print() output is captured live via ava_vm_set_print_callback
    // (see the .cpp) instead of going to the process's stdout, which a
    // windowed GUI app usually has no visible console for anyway.
    const std::vector<ConsoleLine>& Console() const { return console_; }
    void ClearConsole() { console_.clear(); }

    // Called by the Output panel's console input box when the user
    // presses Enter. Echoes the text into the console like a terminal
    // would.
    //
    // IMPORTANT: this is scaffolding, not a working REPL input yet.
    // There is no `input()` builtin registered anywhere in
    // core/src/builtins, and there's a real design problem to solve
    // before there can be one: ava_run() runs a script's top-level code
    // synchronously start-to-finish on the calling thread, so there is
    // no point at which a blocking input() call could hand control back
    // to this GUI's event loop without freezing the whole window.
    // AvaLang already has coroutines (ava_coroutine_resume, see
    // avalang.h) which are the natural mechanism for this -- input()
    // would need to suspend the coroutine instead of blocking, and the
    // Output panel would resume it once SubmitConsoleInput() is called.
    // Wiring that up is future work; for now this just queues the text
    // (see input_queue_) for whenever that lands.
    void SubmitConsoleInput(const std::string& text);

    // A read-only, host-side mirror of one AvaComponent -- built while we
    // construct the demo tree below, so the Preview panel has something
    // interactive to walk without re-implementing JSON parsing.
    struct PreviewNode {
        std::string type;
        std::string id;
        std::vector<std::pair<std::string, std::string>> properties;
        std::vector<PreviewNode> children;
    };

    struct DemoTree {
        PreviewNode root;
        std::string json; // from ava_ui_tree_to_json
    };

    // Builds a small fixed Component Tree via ava_ui_* (page > stack >
    // text + button). This is a stand-in for "run a script that declares
    // UI" until the `page`/`stack`/`button` builtins
    // (core/src/ui/builtins.cpp) are wired into the VM -- see that
    // file's header comment. Validates the Component/Property/Child/
    // JSON path the Preview panel depends on.
    DemoTree BuildDemoComponentTree();

private:
    // AvaPrintFn trampoline (avalang.h's callback is a plain C function
    // pointer, not a std::function, so it can't capture `this` directly).
    static void PrintCallbackTrampoline(const char* utf8, size_t len, void* user_data);
    void OnScriptPrint(const std::string& chunk);
    void FlushPendingStdoutLine();

    AvaVM* vm_ = nullptr;
    std::vector<ConsoleLine> console_;

    // print() always hands us a chunk ending in '\n' today (see
    // builtin_print in core/src/builtins/builtin_natives.cpp), but this
    // buffers/splits on '\n' rather than assuming that, so one console
    // line is still produced per physical line even if that ever changes
    // (e.g. a native that prints without a trailing newline).
    std::string pending_stdout_line_;

    // See SubmitConsoleInput() above -- queued but not yet consumed by
    // anything.
    std::vector<std::string> input_queue_;
};

} // namespace studio
