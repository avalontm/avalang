#include "engine/engine_bridge.h"

#include "util/data_dir.h"

namespace studio {

namespace {

// "a/b/c.ava" -> "a/b". "" if there's no separator (unsaved buffer, source
// passed as a bare in-memory string with no real path).
std::string DirOf(const std::string& file_path) {
    auto pos = file_path.find_last_of("/\\");
    return pos == std::string::npos ? "" : file_path.substr(0, pos);
}

} // namespace

EngineBridge::EngineBridge() {
    vm_ = ava_vm_create();
    // Route every print() call from any script this VM runs into
    // OnScriptPrint() instead of the process's stdout -- see the Console()
    // comment in engine_bridge.h for why.
    ava_vm_set_print_callback(vm_, &EngineBridge::PrintCallbackTrampoline, this);
}

EngineBridge::~EngineBridge() {
    if (vm_) ava_vm_destroy(vm_);
}

void EngineBridge::PrintCallbackTrampoline(const char* utf8, size_t len, void* user_data) {
    static_cast<EngineBridge*>(user_data)->OnScriptPrint(std::string(utf8, len));
}

void EngineBridge::OnScriptPrint(const std::string& chunk) {
    pending_stdout_line_ += chunk;

    // Split on '\n', pushing one ConsoleLine per complete physical line
    // and leaving any trailing partial line buffered for the next chunk.
    size_t start = 0;
    while (true) {
        size_t newline = pending_stdout_line_.find('\n', start);
        if (newline == std::string::npos) break;
        console_.push_back({ConsoleLine::Kind::Stdout, pending_stdout_line_.substr(start, newline - start)});
        start = newline + 1;
    }
    pending_stdout_line_.erase(0, start);
}

void EngineBridge::FlushPendingStdoutLine() {
    if (!pending_stdout_line_.empty()) {
        console_.push_back({ConsoleLine::Kind::Stdout, pending_stdout_line_});
        pending_stdout_line_.clear();
    }
}

void EngineBridge::SubmitConsoleInput(const std::string& text) {
    console_.push_back({ConsoleLine::Kind::Input, "> " + text});
    input_queue_.push_back(text);
}

void EngineBridge::SetModulesPath(const std::string& path) {
    const std::string resolved = path.empty() ? util::ResolveDefaultModulesDir() : path;
    ava_vm_set_stdlib_path(vm_, resolved.c_str());
}

RunResult EngineBridge::RunScript(const std::string& source, const std::string& source_name) {
    RunResult result;

    console_.push_back({ConsoleLine::Kind::Info, "Run " + (source_name.empty() ? std::string("<script>") : source_name)});

    // ava_compile/ava_run take `source` as an in-memory string, not a file
    // path, so the VM has no way to know where it "lives" on disk -- and
    // without that, `import` falls back to resolving relative to the
    // process's CWD instead of the script's own folder. Fix that up front
    // using source_name (which main.cpp passes as the tab's real file_path).
    ava_vm_set_current_dir(vm_, DirOf(source_name).c_str());

    char* compile_error = nullptr;
    AvaModule* module = ava_compile(vm_, source.c_str(), source_name.c_str(), &compile_error);
    if (!module) {
        result.success = false;
        result.message = compile_error ? compile_error : "unknown compile error";
        result.error_line = ava_last_error_line(vm_);
        result.error_column = ava_last_error_column(vm_);
        if (compile_error) ava_string_free(compile_error);
        console_.push_back({ConsoleLine::Kind::Error, result.message});
        return result;
    }

    ava_value_t out_result;
    char* run_error = nullptr;
    ava_run(vm_, module, &out_result, &run_error);
    ava_module_destroy(module);

    // Flush any print() output before the error/success marker below, so
    // the console reads top-to-bottom the way a real terminal would
    // (script output, then the final status line).
    FlushPendingStdoutLine();

    if (run_error) {
        result.success = false;
        result.message = run_error;
        result.error_line = ava_last_error_line(vm_);
        result.error_column = ava_last_error_column(vm_);
        ava_string_free(run_error);
        console_.push_back({ConsoleLine::Kind::Error, result.message});
        return result;
    }

    result.success = true;
    switch (out_result.type) {
        case AVA_NIL:
            result.message = "OK (nil)";
            break;
        case AVA_NUMBER:
            result.message = "OK -> " + std::to_string(out_result.as.n);
            break;
        case AVA_BOOL:
            result.message = std::string("OK -> ") + (out_result.as.b ? "true" : "false");
            break;
        case AVA_STRING: {
            size_t len = 0;
            const char* data = ava_string_data(vm_, out_result, &len);
            result.message = "OK -> \"" + std::string(data, len) + "\"";
            ava_value_release(vm_, out_result);
            break;
        }
        default:
            result.message = "OK (script ran, result not a printable primitive)";
            ava_value_release(vm_, out_result);
            break;
    }
    console_.push_back({ConsoleLine::Kind::Success, result.message});
    return result;
}

EngineBridge::DemoTree EngineBridge::BuildDemoComponentTree() {
    AvaComponentTree* tree = ava_ui_create_tree();

    AvaComponent* page = ava_ui_create_component("page");
    ava_ui_set_id(page, "Main");

    AvaComponent* stack = ava_ui_create_component("stack");
    ava_ui_set_layout(stack, 1 /* LayoutType::Column, see ui/component.h */);

    AvaComponent* text = ava_ui_create_component("text");
    ava_value_t text_value = ava_string_create(vm_, "Hello", 5);
    ava_ui_set_property(text, "value", text_value);

    AvaComponent* button = ava_ui_create_component("button");
    ava_value_t button_text = ava_string_create(vm_, "Save", 4);
    ava_ui_set_property(button, "text", button_text);

    ava_ui_add_child(stack, text);
    ava_ui_add_child(stack, button);
    ava_ui_add_child(page, stack);
    ava_ui_set_root(tree, page);

    DemoTree result;
    result.json = ava_ui_tree_to_json(tree);

    // Mirror the same tree on the host side for the interactive Preview
    // panel. We already hold the values we set above, so this is a plain
    // C++ struct copy rather than reading properties back through the C
    // API (there's no "list all property keys" call yet -- see
    // ava_ui_has_property/get_property in avalang.h, both take a known
    // key). Good enough for a fixed demo tree; a script-driven tree will
    // need that enumeration added to the C API.
    result.root.type = "page";
    result.root.id = "Main";
    PreviewNode host_stack;
    host_stack.type = "stack";
    PreviewNode host_text;
    host_text.type = "text";
    host_text.properties.push_back({"value", "Hello"});
    PreviewNode host_button;
    host_button.type = "button";
    host_button.properties.push_back({"text", "Save"});
    host_stack.children.push_back(std::move(host_text));
    host_stack.children.push_back(std::move(host_button));
    result.root.children.push_back(std::move(host_stack));

    // ava_ui_get_child/get_root hand back new wrapper pointers that own a
    // shared_ptr to the same underlying node -- each one we created here
    // (page, stack, text, button) needs its own destroy call. The tree
    // itself only destroys the AvaComponentTree wrapper, not the nodes
    // (see ava_ui_destroy_tree in c_api.cpp), so this mirrors that
    // ownership model rather than fighting it.
    ava_ui_destroy_component(button);
    ava_ui_destroy_component(text);
    ava_ui_destroy_component(stack);
    ava_ui_destroy_component(page);
    ava_ui_destroy_tree(tree);

    return result;
}

} // namespace studio
