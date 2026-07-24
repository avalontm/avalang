#include "engine/engine_bridge.h"

namespace studio {

EngineBridge::EngineBridge() {
    vm_ = ava_vm_create();
}

EngineBridge::~EngineBridge() {
    if (vm_) ava_vm_destroy(vm_);
}

RunResult EngineBridge::RunScript(const std::string& source, const std::string& source_name) {
    RunResult result;

    char* compile_error = nullptr;
    AvaModule* module = ava_compile(vm_, source.c_str(), source_name.c_str(), &compile_error);
    if (!module) {
        result.success = false;
        result.message = compile_error ? compile_error : "unknown compile error";
        if (compile_error) ava_string_free(compile_error);
        return result;
    }

    ava_value_t out_result;
    char* run_error = nullptr;
    ava_run(vm_, module, &out_result, &run_error);
    ava_module_destroy(module);

    if (run_error) {
        result.success = false;
        result.message = run_error;
        ava_string_free(run_error);
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
