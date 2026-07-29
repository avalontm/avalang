#include "runtime/runtime_host.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <utility>

#include <nlohmann/json.hpp>

using nlohmann::json;

namespace avahost {

namespace {

// Mirrors Ava Studio's design/state_eval.cpp anonymous-namespace
// LooksNumeric exactly (same digits/one-dot/optional-leading-sign
// grammar) -- kept as its own copy here since AvaHost and Studio don't
// share a common internal library across that boundary.
bool LooksNumeric(const std::string& v) {
    if (v.empty()) return false;
    size_t i = 0;
    if (v[i] == '+' || v[i] == '-') ++i;
    if (i >= v.size()) return false;
    bool hasDigits = false;
    bool hasDot = false;
    for (; i < v.size(); ++i) {
        if (std::isdigit(static_cast<unsigned char>(v[i]))) {
            hasDigits = true;
        } else if (v[i] == '.' && !hasDot) {
            hasDot = true;
        } else {
            return false;
        }
    }
    return hasDigits;
}

std::string NumberToDisplayString(double n) {
    std::ostringstream oss;
    oss << n;
    return oss.str();
}

} // namespace

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

void RuntimeHost::BindState(const std::string& stateJson) {
    if (!vm_) return;

    json parsed;
    try {
        parsed = json::parse(stateJson);
    } catch (const json::exception&) {
        return; // malformed state -- fail soft, same spirit as ParseRouteDeclarations
    }
    if (!parsed.is_object()) return;

    for (auto it = parsed.begin(); it != parsed.end(); ++it) {
        // AvaUiDocument::stateJson's values are always strings (the
        // `state` block's raw source text -- see ava::ui::StateToJson),
        // but tolerate a non-string value defensively rather than throw.
        const std::string raw = it.value().is_string() ? it.value().get<std::string>() : it.value().dump();

        ava_value_t value{};
        if (raw == "true" || raw == "false") {
            value.type = AVA_BOOL;
            value.as.b = (raw == "true") ? 1 : 0;
        } else if (LooksNumeric(raw)) {
            value.type = AVA_NUMBER;
            value.as.n = std::strtod(raw.c_str(), nullptr);
        } else {
            // Not released after this set -- same convention
            // SetRequestContext above already uses for this VM.
            value = ava_string_create(vm_, raw.data(), raw.size());
        }
        ava_set_global(vm_, it.key().c_str(), value);
    }
}

void RuntimeHost::BindCodeBehind(const std::string& methodsText) {
    if (!vm_ || methodsText.empty()) return;

    char* compileError = nullptr;
    AvaModule* module = ava_compile(vm_, methodsText.c_str(), "<avaui-code>", &compileError);
    if (!module) {
        // Best-effort, per header comment -- a `code` block that
        // doesn't compile leaves this request with no handlers bound,
        // not a broken response.
        if (compileError) ava_string_free(compileError);
        return;
    }

    ava_value_t result{};
    char* runError = nullptr;
    ava_run(vm_, module, &result, &runError);
    ava_module_destroy(module);
    if (runError) ava_string_free(runError);
}

bool RuntimeHost::InvokeHandler(const std::string& handlerName, std::string& outError) {
    if (!vm_ || handlerName.empty()) return false;

    // No parens/args support in v0.1 -- every handler AvaHost calls is
    // the zero-arg form (`func OnGuardarClick()`), same model
    // EventBinder's data-handler attributes assume.
    const std::string source = "__avahost_invoke__ = " + handlerName + "()";

    char* compileError = nullptr;
    AvaModule* module = ava_compile(vm_, source.c_str(), "<avahost-handler-call>", &compileError);
    if (!module) {
        if (compileError) {
            outError = compileError;
            ava_string_free(compileError);
        } else {
            outError = "unknown compile error invoking " + handlerName;
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

    // Discard the return value -- handlers are called for their
    // mutation of `state` globals, not for a result (matches Studio's
    // InvokeHandler).
    ava_value_t invokeResult = ava_get_global(vm_, "__avahost_invoke__");
    if (invokeResult.type == AVA_STRING) ava_value_release(vm_, invokeResult);
    return true;
}

bool RuntimeHost::InvokeHandlerIfDefined(const std::string& handlerName, std::string& outError) {
    if (!vm_ || handlerName.empty()) return true;

    // Peek the global without invoking it -- a page/layout that never
    // defines this lifecycle function (the common case) must render
    // exactly as if this call never happened, not surface a "call to
    // undefined function" runtime error from InvokeHandler.
    ava_value_t existing = ava_get_global(vm_, handlerName.c_str());
    bool isFunction = (existing.type == AVA_FUNCTION);
    ava_value_release(vm_, existing);
    if (!isFunction) return true;

    return InvokeHandler(handlerName, outError);
}

std::string RuntimeHost::EvalPropertyExpr(const std::string& rawValue) {
    if (!vm_ || rawValue.empty()) return rawValue;

    // Wrapped in parens so an expression like `"Counter: " + counter`
    // (or a plain literal/identifier) always lands as a single RHS --
    // same trick as Studio's EvalPropertyExpr, and same
    // `__avahost_eval__` naming convention to keep collision with a
    // real `state` var astronomically unlikely.
    const std::string source = "__avahost_eval__ = (" + rawValue + ")";

    char* compileError = nullptr;
    AvaModule* module = ava_compile(vm_, source.c_str(), "<avaui-prop>", &compileError);
    if (!module) {
        // Common case: rawValue is a plain string literal that isn't a
        // valid bare expression on its own (e.g. contains spaces) --
        // fall back to showing it as-is.
        if (compileError) ava_string_free(compileError);
        return rawValue;
    }

    ava_value_t result{};
    char* runError = nullptr;
    ava_run(vm_, module, &result, &runError);
    ava_module_destroy(module);
    if (runError) {
        ava_string_free(runError);
        return rawValue;
    }

    ava_value_t value = ava_get_global(vm_, "__avahost_eval__");
    switch (value.type) {
        case AVA_BOOL:
            return value.as.b ? "true" : "false";
        case AVA_NUMBER:
            return NumberToDisplayString(value.as.n);
        case AVA_STRING: {
            size_t len = 0;
            const char* data = ava_string_data(vm_, value, &len);
            std::string display(data, len);
            ava_value_release(vm_, value);
            return display;
        }
        case AVA_NIL:
            // Plain string literal / not-yet-defined identifier -- the
            // common case, see header comment.
            return rawValue;
        default:
            // List/Dict/Function/etc. aren't meaningful as rendered
            // text -- fall back rather than show "" or a handle.
            ava_value_release(vm_, value);
            return rawValue;
    }
}

std::string RuntimeHost::ExportStateJson(const std::string& templateStateJson) {
    if (!vm_) return templateStateJson;

    json parsed;
    try {
        parsed = json::parse(templateStateJson.empty() ? "{}" : templateStateJson);
    } catch (const json::exception&) {
        return templateStateJson; // malformed -- nothing sensible to export, keep as-is
    }
    if (!parsed.is_object()) return templateStateJson;

    json out = json::object();
    for (auto it = parsed.begin(); it != parsed.end(); ++it) {
        ava_value_t value = ava_get_global(vm_, it.key().c_str());
        switch (value.type) {
            case AVA_BOOL:
                out[it.key()] = value.as.b ? "true" : "false";
                break;
            case AVA_NUMBER:
                out[it.key()] = NumberToDisplayString(value.as.n);
                break;
            case AVA_STRING: {
                size_t len = 0;
                const char* data = ava_string_data(vm_, value, &len);
                out[it.key()] = std::string(data, len);
                ava_value_release(vm_, value);
                break;
            }
            default:
                // Nil (never set as a global, e.g. BindState skipped
                // because the source didn't compile) or a non-scalar
                // type -- fall back to whatever templateStateJson
                // already had rather than losing the value entirely.
                out[it.key()] = it.value().is_string() ? it.value().get<std::string>() : it.value().dump();
                break;
        }
    }
    return out.dump();
}

void RuntimeHost::BeginConsoleCapture() {
    consoleCaptureBuffer_.clear();
    if (!vm_) return;
    ava_vm_set_print_callback(
        vm_,
        [](const char* utf8, size_t len, void* userData) {
            // Both destinations: append to the buffer (relayed to the
            // browser console once the render finishes -- see
            // AvaHostApp::BuildConsoleScript) AND keep writing straight
            // to stdout, exactly like the default sink (VM::Print) did,
            // so the server terminal's output doesn't change.
            static_cast<std::string*>(userData)->append(utf8, len);
            std::fwrite(utf8, 1, len, stdout);
        },
        &consoleCaptureBuffer_);
}

std::string RuntimeHost::EndConsoleCapture() {
    if (vm_) ava_vm_set_print_callback(vm_, nullptr, nullptr);
    std::string out = std::move(consoleCaptureBuffer_);
    consoleCaptureBuffer_.clear();
    return out;
}

} // namespace avahost
