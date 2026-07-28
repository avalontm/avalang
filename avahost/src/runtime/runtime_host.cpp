#include "runtime/runtime_host.h"

#include <utility>

#include <nlohmann/json.hpp>

using nlohmann::json;

namespace avahost {

RuntimeHost::AvaUiDocument::~AvaUiDocument() {
    if (tree) ava_ui_destroy_tree(tree);
}

RuntimeHost::AvaUiDocument::AvaUiDocument(AvaUiDocument&& other) noexcept
    : tree(other.tree),
      stateJson(std::move(other.stateJson)),
      importsJson(std::move(other.importsJson)),
      methodsText(std::move(other.methodsText)),
      extends(std::move(other.extends)),
      routesJson(std::move(other.routesJson)),
      ok(other.ok),
      error(std::move(other.error)) {
    other.tree = nullptr;
}

RuntimeHost::AvaUiDocument& RuntimeHost::AvaUiDocument::operator=(AvaUiDocument&& other) noexcept {
    if (this == &other) return *this;
    if (tree) ava_ui_destroy_tree(tree);
    tree = other.tree;
    stateJson = std::move(other.stateJson);
    importsJson = std::move(other.importsJson);
    methodsText = std::move(other.methodsText);
    extends = std::move(other.extends);
    routesJson = std::move(other.routesJson);
    ok = other.ok;
    error = std::move(other.error);
    other.tree = nullptr;
    return *this;
}

RuntimeHost::RuntimeHost() {
    vm_ = ava_vm_create();
}

RuntimeHost::~RuntimeHost() {
    if (vm_) ava_vm_destroy(vm_);
}

void RuntimeHost::AddSearchPath(const std::string& path) {
    if (vm_) ava_vm_add_search_path(vm_, path.c_str());
}

void RuntimeHost::SetCurrentDir(const std::string& path) {
    if (vm_) ava_vm_set_current_dir(vm_, path.c_str());
}

RuntimeHost::AvaUiDocument RuntimeHost::ParseAvaUiFile(const std::string& text) const {
    AvaUiDocument doc;

    char* stateJson = nullptr;
    char* importsJson = nullptr;
    char* methodsText = nullptr;
    char* error = nullptr;
    char* extends = nullptr;
    char* routesJson = nullptr;

    doc.tree = ava_ui_parse_avaui_text(
        text.c_str(),
        &stateJson,
        &importsJson,
        &methodsText,
        &error,
        &extends,
        &routesJson);

    if (stateJson)   { doc.stateJson = stateJson;   ava_ui_text_free(stateJson); }
    if (importsJson) { doc.importsJson = importsJson; ava_ui_text_free(importsJson); }
    if (methodsText) { doc.methodsText = methodsText; ava_ui_text_free(methodsText); }
    if (extends)     { doc.extends = extends;       ava_ui_text_free(extends); }
    if (routesJson)  { doc.routesJson = routesJson; ava_ui_text_free(routesJson); }
    if (error) {
        if (*error != '\0') {
            doc.ok = false;
            doc.error = error;
        }
        ava_ui_text_free(error);
    }

    return doc;
}

std::vector<RuntimeHost::RouteTemplate> RuntimeHost::ParseRouteDeclarations(const std::string& text) const {
    std::vector<RouteTemplate> result;

    char* routesJson = nullptr;
    AvaComponentTree* tree = ava_ui_parse_avaui_text(
        text.c_str(),
        /*out_state_json=*/nullptr,
        /*out_imports_json=*/nullptr,
        /*out_methods_text=*/nullptr,
        /*out_error=*/nullptr,
        /*out_extends=*/nullptr,
        &routesJson);
    if (tree) ava_ui_destroy_tree(tree);
    if (!routesJson) return result;

    // Shape is exactly ava_ui_parse_avaui_text's out_routes_json (see
    // avalang.h): [{"template": "...", "parameters": [{"name": "...",
    // "optional": bool, "constraint": "..." (optional key)}]}].
    try {
        json parsed = json::parse(routesJson);
        for (const auto& routeObj : parsed) {
            RouteTemplate tmpl;
            tmpl.pathTemplate = routeObj.value("template", "");
            for (const auto& paramObj : routeObj.value("parameters", json::array())) {
                RouteParam param;
                param.name = paramObj.value("name", "");
                param.optional = paramObj.value("optional", false);
                param.constraint = paramObj.value("constraint", "");
                tmpl.params.push_back(std::move(param));
            }
            result.push_back(std::move(tmpl));
        }
    } catch (const json::exception&) {
        // Malformed routes_json would be a core bug, not a project
        // authoring error -- fail soft (no declared routes) rather
        // than crash the host.
    }

    ava_ui_text_free(routesJson);
    return result;
}

void RuntimeHost::SetRequestContext(const RequestContext& ctx) {
    if (!vm_) return;

    ava_value_t params = ava_dict_create(vm_);
    for (const auto& [name, value] : ctx.params) {
        ava_dict_set(vm_, params, name.c_str(), ava_string_create(vm_, value.data(), value.size()));
    }

    ava_value_t query = ava_dict_create(vm_);
    for (const auto& [name, value] : ctx.query) {
        ava_dict_set(vm_, query, name.c_str(), ava_string_create(vm_, value.data(), value.size()));
    }

    ava_value_t request = ava_dict_create(vm_);
    ava_dict_set(vm_, request, "method", ava_string_create(vm_, ctx.method.data(), ctx.method.size()));
    ava_dict_set(vm_, request, "path", ava_string_create(vm_, ctx.path.data(), ctx.path.size()));
    ava_dict_set(vm_, request, "params", params);
    ava_dict_set(vm_, request, "query", query);

    // Not released after this -- same convention as
    // studio/src/design/state_eval.cpp's BuildStateVM (ava_set_global
    // replaces whatever the global slot pointed to; nothing here is
    // retained beyond that assignment). AvaHost's VM is long-lived
    // (reused across requests, see class comment), unlike Studio's
    // per-call VM, but object lifetime past ava_set_global is the
    // engine's allocator/GC's concern, not this host's -- see
    // core/src/vm/vm_containers.cpp for how the engine itself allocates
    // dicts internally (same `new DictObj()` pattern, no manual free).
    ava_set_global(vm_, "request", request);
}

bool RuntimeHost::RunScript(const std::string& source, const std::string& scriptName,
                             std::string& outError) {
    char* compileError = nullptr;
    AvaModule* module = ava_compile(vm_, source.c_str(), scriptName.c_str(), &compileError);
    if (!module) {
        if (compileError) {
            outError = compileError;
            ava_string_free(compileError);
        } else {
            outError = "unknown compile error";
        }
        return false;
    }

    ava_value_t result{};
    char* runError = nullptr;
    ava_run(vm_, module, &result, &runError);
    ava_module_destroy(module);

    if (runError) {
        outError = runError;
        ava_string_free(runError);
        return false;
    }
    return true;
}

bool RuntimeHost::RunScriptCapturingOutput(const std::string& source, const std::string& scriptName,
                                            std::string& outOutput, std::string& outError) {
    outOutput.clear();

    ava_vm_set_print_callback(
        vm_,
        [](const char* utf8, size_t len, void* userData) {
            static_cast<std::string*>(userData)->append(utf8, len);
        },
        &outOutput);

    bool success = RunScript(source, scriptName, outError);

    // Restore the default (fn = NULL) so a later caller that doesn't go
    // through this method -- or the next request on this same reused VM
    // -- never writes into a buffer that's gone out of scope.
    ava_vm_set_print_callback(vm_, nullptr, nullptr);

    return success;
}

} // namespace avahost
