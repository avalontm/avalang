#include "engine/engine_bridge.h"

#include "util/data_dir.h"
#include "parser/AvauiPropertyCoercion.h"

namespace studio {

namespace {

using avalang::ui::parser::NumberToDisplayString;

std::string DirOf(const std::string& file_path) {
    auto pos = file_path.find_last_of("/\\");
    return pos == std::string::npos ? "" : file_path.substr(0, pos);
}

std::string ReadLastErrorSource(AvaVM* vm, const std::string& ran_source_name) {
    char* raw = ava_last_error_source(vm);
    std::string source = raw ? raw : "";
    if (raw) ava_string_free(raw);
    return source.empty() ? ran_source_name : source;
}

// `import Foo` (segmento único, sin alias) se resuelve buscando
// "Foo.ava" como sibling en disco, tanto para el harvesting estático de
// clases en tiempo de compilación (Compiler::RegisterImportedClasses,
// compiler.cpp) como para el import real en runtime (VM::DoImport,
// vm_import.cpp) -- ninguno de los dos mira los buffers de las demás
// pestañas abiertas en el editor. Si `source_name` está vacío (pestaña
// "Untitled", nunca guardada -- ver EditorTab::file_path/NewUntitledTab,
// editor_panel.cpp), la resolución de siblings no tiene ninguna carpeta
// real donde buscar y cae en el cwd del proceso, que casi nunca es
// donde vive el otro archivo. El resultado es un error confuso y
// aparentemente no relacionado (ej. "'Foo' is not a class", o en
// runtime "could not find module: Foo") en vez de decir lo que en
// realidad pasa: el archivo (o el que importa) todavía no está guardado
// en disco. Heurística barata: solo importa si el source declara algún
// `import` de un solo segmento (dotted imports y namespaces nativos
// como `system`/`io` no dependen de un sibling .ava, así que no
// aplica el aviso).
bool HasBareSingleSegmentImport(const std::string& source) {
    size_t pos = 0;
    while ((pos = source.find("import", pos)) != std::string::npos) {
        bool at_line_start = (pos == 0) || source[pos - 1] == '\n' || source[pos - 1] == '\r';
        size_t after = pos + 6;
        if (at_line_start && after < source.size() && (source[after] == ' ' || source[after] == '\t')) {
            size_t line_end = source.find('\n', after);
            std::string rest = source.substr(after, line_end == std::string::npos ? std::string::npos
                                                                                    : line_end - after);
            if (rest.find('.') == std::string::npos && rest.find(" as ") == std::string::npos) {
                return true;
            }
        }
        pos = after;
    }
    return false;
}

const char* kUnsavedImportHint =
    "\n(este archivo no esta guardado en disco -- los imports a otros .ava del "
    "mismo proyecto solo se resuelven contra archivos ya guardados en la misma "
    "carpeta, no contra otras pestanas abiertas sin guardar. Guarda este archivo "
    "y el/los que importa, y volve a correr.)";

}

EngineBridge::EngineBridge() {
    vm_ = ava_vm_create();

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

    ++run_count_;
    console_.push_back({ConsoleLine::Kind::Info,
        "Run #" + std::to_string(run_count_) + " " +
        (source_name.empty() ? std::string("<script>") : source_name)});

    ava_vm_set_current_dir(vm_, DirOf(source_name).c_str());

    char* compile_error = nullptr;
    AvaModule* module = ava_compile(vm_, source.c_str(), source_name.c_str(), &compile_error);
    if (!module) {
        result.success = false;
        result.message = compile_error ? compile_error : "unknown compile error";
        result.error_line = ava_last_error_line(vm_);
        result.error_column = ava_last_error_column(vm_);
        result.error_source = ReadLastErrorSource(vm_, source_name);
        if (compile_error) ava_string_free(compile_error);
        if (source_name.empty() && HasBareSingleSegmentImport(source)) {
            result.message += kUnsavedImportHint;
        }
        console_.push_back({ConsoleLine::Kind::Error, result.message,
                             result.error_source, result.error_line, result.error_column});
        return result;
    }

    ava_value_t out_result;
    char* run_error = nullptr;
    ava_run(vm_, module, &out_result, &run_error);
    ava_module_destroy(module);

    FlushPendingStdoutLine();

    if (run_error) {
        result.success = false;
        result.message = run_error;
        result.error_line = ava_last_error_line(vm_);
        result.error_column = ava_last_error_column(vm_);
        result.error_source = ReadLastErrorSource(vm_, source_name);
        ava_string_free(run_error);
        if (source_name.empty() && HasBareSingleSegmentImport(source)) {
            result.message += kUnsavedImportHint;
        }
        console_.push_back({ConsoleLine::Kind::Error, result.message,
                             result.error_source, result.error_line, result.error_column});
        return result;
    }

    result.success = true;
    switch (out_result.type) {
        case AVA_NIL:

            result.message = "OK";
            break;
        case AVA_NUMBER:
            result.message = "OK -> " + NumberToDisplayString(out_result.as.n);
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

RunResult EngineBridge::CheckScript(const std::string& source, const std::string& source_name) {
    RunResult result;

    ava_vm_set_current_dir(vm_, DirOf(source_name).c_str());

    char* compile_error = nullptr;
    AvaModule* module = ava_compile(vm_, source.c_str(), source_name.c_str(), &compile_error);
    if (!module) {
        result.success = false;
        result.message = compile_error ? compile_error : "unknown compile error";
        result.error_line = ava_last_error_line(vm_);
        result.error_column = ava_last_error_column(vm_);
        result.error_source = ReadLastErrorSource(vm_, source_name);
        if (compile_error) ava_string_free(compile_error);
        return result;
    }

    ava_module_destroy(module);
    result.success = true;
    result.message = "OK";
    return result;
}

EngineBridge::DemoTree EngineBridge::BuildDemoComponentTree() {
    AvaComponentTree* tree = ava_ui_create_tree();

    AvaComponent* page = ava_ui_create_component("page");
    ava_ui_set_id(page, "Main");

    AvaComponent* stack = ava_ui_create_component("stack");
    ava_ui_set_layout(stack, 1 );

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

    ava_ui_destroy_component(button);
    ava_ui_destroy_component(text);
    ava_ui_destroy_component(stack);
    ava_ui_destroy_component(page);
    ava_ui_destroy_tree(tree);

    return result;
}

}
