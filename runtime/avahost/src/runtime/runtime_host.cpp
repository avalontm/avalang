#include "runtime/runtime_host.h"

#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <utility>

#include <nlohmann/json.hpp>

#ifdef AVAHOST_HAS_UI_PIPELINE
#include "parser/AvauiPropertyCoercion.h"
using avalang::ui::parser::InferValue;
using avalang::ui::parser::NumberToDisplayString;
using avalang::ui::parser::LooksLikeCall;
#endif

using nlohmann::json;

namespace avahost {

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

std::vector<RuntimeHost::RouteTemplate> RuntimeHost::ParseRouteDeclarations(const std::string& text) const {
    std::vector<RouteTemplate> result;

    char* routesJson = nullptr;
    AvaComponentTree* tree = ava_ui_parse_avaui_text(
        text.c_str(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &routesJson);
    if (tree) ava_ui_destroy_tree(tree);
    if (!routesJson) return result;




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



    }

    ava_ui_text_free(routesJson);
    return result;
}

bool RuntimeHost::ValidateAvaUiFile(const std::string& text, std::string& outError) const {
    char* error = nullptr;

    AvaComponentTree* tree = ava_ui_parse_avaui_text(
        text.c_str(),
        nullptr,
        nullptr,
        nullptr,
        &error,
        nullptr,
        nullptr);
    if (tree) ava_ui_destroy_tree(tree);

    bool ok = true;
    if (error) {
        if (*error != '\0') {
            ok = false;
            outError = error;
        }
        ava_ui_text_free(error);
    }
    return ok;
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




    ava_vm_set_print_callback(vm_, nullptr, nullptr);

    return success;
}

bool RuntimeHost::SplitNamespacedKey(const std::string& key, std::string& outNamespace, std::string& outField) {
    size_t dot = key.find('.');
    if (dot == std::string::npos || dot == 0 || dot + 1 >= key.size()) return false;
    outNamespace = key.substr(0, dot);
    outField = key.substr(dot + 1);
    return true;
}

ava_value_t RuntimeHost::GetOrCreateDictGlobal(const std::string& ns) {
    ava_value_t existing = ava_get_global(vm_, ns.c_str());  // retained, see VM::GetGlobal
    if (existing.type == AVA_DICT) {
        return existing;  // caller owns this reference and must release it when done
    }
    ava_value_release(vm_, existing);  // safe no-op for Nil; drops a wrongly-typed collision otherwise

    ava_value_t created = ava_dict_create(vm_);
    ava_set_global(vm_, ns.c_str(), created);  // SetGlobal retains internally
    ava_value_release(vm_, created);           // drop our local ref; the global now owns the only one
    return ava_get_global(vm_, ns.c_str());    // fetch again -> caller gets a fresh, owned reference
}

namespace {

// Quotes+escapes a string for embedding back into AvaLang source text
// (the mirror image of AvauiPropertyCoercion's Unquote). Only needs to
// handle what round-tripping through our own SerializeAvaValueToLiteral
// below can produce, but escapes generally for safety.
std::string QuoteForAvaLang(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c; break;
        }
    }
    out += "\"";
    return out;
}

// Renders a live VM value back into AvaLang literal source text, e.g.
// {id: 3, name: "Latte"} or [1, 2, 3]. This is what makes List/Dict
// state (e.g. `cart = []` in a page's `state` block, then mutated with
// `cart.append(...)`) actually survive being exported to the
// session's cached-state JSON and re-bound on the next request --
// previously ExportStateJson's `default:` case for AVA_LIST/AVA_DICT
// just echoed back whatever raw text was already cached (see fix
// notes), so appended items were silently dropped on every request
// after the one that added them, and BindState's InferValue() doesn't
// recognize `[`/`{` syntax either, so even the *first* render turned
// `cart = []` into the four-character string "[]" instead of an empty
// list -- calling `cart.append(...)` on that string is exactly the
// "attempt to call a non-callable value" seen at runtime (`.append`
// isn't a builtin string method, so the lookup falls through to nil,
// and nil isn't callable).
std::string SerializeAvaValueToLiteral(AvaVM* vm, ava_value_t value) {
    switch (value.type) {
        case AVA_BOOL:
            return value.as.b ? "true" : "false";
        case AVA_NUMBER:
#ifdef AVAHOST_HAS_UI_PIPELINE
            return NumberToDisplayString(value.as.n);
#else
            {
                std::ostringstream oss;
                oss << value.as.n;
                return oss.str();
            }
#endif
        case AVA_STRING: {
            size_t len = 0;
            const char* data = ava_string_data(vm, value, &len);
            return QuoteForAvaLang(std::string(data, len));
        }
        case AVA_LIST: {
            std::string out = "[";
            size_t n = ava_list_length(vm, value);
            for (size_t i = 0; i < n; ++i) {
                if (i) out += ", ";
                out += SerializeAvaValueToLiteral(vm, ava_list_get(vm, value, i));
            }
            out += "]";
            return out;
        }
        case AVA_DICT: {
            std::string out = "{";
            void* rawEntries = nullptr;
            size_t n = ava_dict_entries(vm, value, &rawEntries);
            auto* entries = static_cast<ava_dict_pair_t*>(rawEntries);
            for (size_t i = 0; i < n; ++i) {
                if (i) out += ", ";
                out += std::string(entries[i].key, entries[i].key_len) + ": " +
                       SerializeAvaValueToLiteral(vm, entries[i].value);
            }
            out += "}";
            return out;
        }
        default:
            return "nil";
    }
}

}  // namespace

void RuntimeHost::BindState(const std::string& stateJson) {
    if (!vm_) return;

    json parsed;
    try {
        parsed = json::parse(stateJson);
    } catch (const json::exception&) {
        return;
    }
    if (!parsed.is_object()) return;

    for (auto it = parsed.begin(); it != parsed.end(); ++it) {



        const std::string raw = it.value().is_string() ? it.value().get<std::string>() : it.value().dump();

        ava_value_t value{};
        bool valueOwnedFromGetGlobal = false;
#ifdef AVAHOST_HAS_UI_PIPELINE
        auto pv = InferValue(raw);
        switch (pv.Type()) {
            case avalang::ui::PropertyType::Bool:
                value.type = AVA_BOOL;
                value.as.b = pv.AsBool() ? 1 : 0;
                break;
            case avalang::ui::PropertyType::Number:
                value.type = AVA_NUMBER;
                value.as.n = pv.AsNumber();
                break;
            default: {
                // InferValue only distinguishes Bool/Number/(everything
                // else is String) -- it has no concept of List/Dict, so
                // raw text like "[]" or "[{id: 1, qty: 2}]" (state
                // written by SerializeAvaValueToLiteral above, or a
                // page's own `state` block default such as `cart =
                // []`) lands here too, indistinguishable from an actual
                // string value. Trim and sniff for `[`/`{` before
                // falling back to wrapping raw as a literal VM string:
                // if it looks like a list/dict literal, compile+run it
                // as a real AvaLang expression (same technique
                // EvalPropertyExpr already uses for property
                // expressions) so `cart` actually becomes a List, not
                // the four-character string "[]".
                size_t start = raw.find_first_not_of(" \t\r\n");
                bool looksComplex = start != std::string::npos &&
                                     (raw[start] == '[' || raw[start] == '{');
                bool handled = false;
                if (looksComplex) {
                    const std::string exprSource = "__avahost_state_init__ = (" + raw + ")";
                    char* exprCompileError = nullptr;
                    AvaModule* exprModule = ava_compile(vm_, exprSource.c_str(),
                                                         "<avahost-state-init>", &exprCompileError);
                    if (exprModule) {
                        ava_value_t exprResult{};
                        char* exprRunError = nullptr;
                        ava_run(vm_, exprModule, &exprResult, &exprRunError);
                        ava_module_destroy(exprModule);
                        if (!exprRunError) {
                            // ava_get_global hands back an owned, extra
                            // reference (see its doc comment in
                            // vm_core.cpp) on top of whatever the
                            // globals table itself holds -- tracked via
                            // valueOwnedFromGetGlobal below so it gets
                            // released once `value` has been handed off
                            // (ava_dict_set takes the reference as-is;
                            // ava_set_global/SetGlobal Retains its own
                            // copy instead of adopting this one, so
                            // without the release this leaks one
                            // refcount per List/Dict state key on every
                            // request).
                            value = ava_get_global(vm_, "__avahost_state_init__");
                            valueOwnedFromGetGlobal = true;
                            handled = true;
                        } else {
                            ava_string_free(exprRunError);
                        }
                    } else if (exprCompileError) {
                        ava_string_free(exprCompileError);
                    }
                }
                if (!handled) {
                    value = ava_string_create(vm_, raw.data(), raw.size());
                }
                break;
            }
        }
#else
        if (raw == "true" || raw == "false") {
            value.type = AVA_BOOL;
            value.as.b = (raw == "true") ? 1 : 0;
        } else {
            value = ava_string_create(vm_, raw.data(), raw.size());
        }
#endif
        std::string ns, field;
        if (SplitNamespacedKey(it.key(), ns, field)) {
            // A key like "dialog.confirmDialogOpen" (from an aliased
            // `import ... as dialog` -- see ComponentResolver.cpp's
            // BuildRenameMap): bind it as a field on a real Dict global
            // named "dialog" instead of a flat global, so a page's own
            // `code` block can read/write it as ordinary AvaLang
            // attribute syntax (`dialog.confirmDialogOpen = true`),
            // which the VM already resolves generically for Dict values
            // (OpGetAttr/OpSetAttr in vm_classes.cpp) -- no VM/compiler
            // changes needed.
            ava_value_t dict = GetOrCreateDictGlobal(ns);
            // ava_dict_set stores `value` as-is without retaining it
            // (same as ui_vm_event_bridge.cpp's BindComponentRefsNative
            // relies on) -- the dict now owns the only reference to
            // `value`, so it must NOT also be released here.
            ava_dict_set(vm_, dict, field.c_str(), value);
            ava_value_release(vm_, dict);
        } else {
            ava_set_global(vm_, it.key().c_str(), value);
            // SetGlobal Retains its own copy rather than adopting this
            // one (unlike ava_dict_set above) -- see
            // valueOwnedFromGetGlobal's comment where it's set.
            if (valueOwnedFromGetGlobal) ava_value_release(vm_, value);
        }
    }
}

bool RuntimeHost::BindCodeBehind(const std::string& methodsText, std::string* outError) {
    if (outError) outError->clear();
    if (!vm_ || methodsText.empty()) return true;

    char* compileError = nullptr;
    AvaModule* module = ava_compile(vm_, methodsText.c_str(), "<avaui-code>", &compileError);
    if (!module) {
        if (compileError) {
            if (outError) *outError = compileError;
            ava_string_free(compileError);
        } else if (outError) {
            *outError = "unknown compile error in code block";
        }
        return false;
    }

    ava_value_t result{};
    char* runError = nullptr;
    ava_run(vm_, module, &result, &runError);
    ava_module_destroy(module);
    if (runError) {
        if (outError) *outError = runError;
        ava_string_free(runError);
        return false;
    }
    return true;
}

bool RuntimeHost::InvokeHandler(const std::string& handlerName, std::string& outError) {
    if (!vm_ || handlerName.empty()) return false;









    const std::string source = "__avahost_invoke__ = " + handlerName +
#ifdef AVAHOST_HAS_UI_PIPELINE
                                (LooksLikeCall(handlerName) ? "" : "()");
#else
                                "()";
#endif

    // Diagnostic pre-check: resolve just the callee name (the part
    // before '(', or the whole string if it's a bare name) as a VM
    // global *before* attempting the call. A closure/func defined by
    // BindCodeBehind should show up here as AVA_FUNCTION -- if it
    // instead comes back AVA_NIL, the handler's `func` declaration
    // never ran (or ran against a different VM instance), and if it
    // comes back some other type, something else clobbered that name
    // between BindCodeBehind and this call. Either way this turns the
    // VM's generic "attempt to call a non-callable value" -- which by
    // itself doesn't say whether the problem is "never defined" vs
    // "defined as the wrong thing" vs "a call-mechanics bug in the VM
    // itself" -- into a message that actually distinguishes those.
    {
        std::string calleeName = handlerName;
        auto paren = calleeName.find('(');
        if (paren != std::string::npos) calleeName = calleeName.substr(0, paren);
        ava_value_t calleeVal = ava_get_global(vm_, calleeName.c_str());
        bool calleeIsCallable = calleeVal.type == AVA_FUNCTION || calleeVal.type == AVA_NATIVE ||
                                 calleeVal.type == AVA_BOUND || calleeVal.type == AVA_CLASS;
        AvaValueType calleeType = calleeVal.type;
        ava_value_release(vm_, calleeVal);
        if (!calleeIsCallable) {
            outError = "handler '" + handlerName + "' resolves '" + calleeName +
                        "' to a non-callable global (type=" + std::to_string(static_cast<int>(calleeType)) +
                        ", 0=nil/1=bool/2=number/3=string/4=list/5=dict/6=function/7=instance/8=class/9=coroutine/10=native/11=bound/12=exception/13=module)"
                        " -- it was never bound as a function, or something reassigned that name, before this handler ran."
                        " Check that the `code`/`methods` block actually declares `func " + calleeName + "(...)` and that"
                        " BindCodeBehind is being called (and succeeding) before this request's handler dispatch.";
            return false;
        }
    }

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




    ava_value_t invokeResult = ava_get_global(vm_, "__avahost_invoke__");
    if (invokeResult.type == AVA_STRING) ava_value_release(vm_, invokeResult);
    return true;
}

bool RuntimeHost::InvokeHandlerIfDefined(const std::string& handlerName, std::string& outError) {
    if (!vm_ || handlerName.empty()) return true;





    ava_value_t existing = ava_get_global(vm_, handlerName.c_str());
    bool isFunction = (existing.type == AVA_FUNCTION);
    ava_value_release(vm_, existing);
    if (!isFunction) return true;

    return InvokeHandler(handlerName, outError);
}

std::string RuntimeHost::EvalPropertyExpr(const std::string& rawValue) {
    if (!vm_ || rawValue.empty()) return rawValue;






    const std::string source = "__avahost_eval__ = (" + rawValue + ")";

    char* compileError = nullptr;
    AvaModule* module = ava_compile(vm_, source.c_str(), "<avaui-prop>", &compileError);
    if (!module) {



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
#ifdef AVAHOST_HAS_UI_PIPELINE
            return NumberToDisplayString(value.as.n);
#else
            {
                std::ostringstream oss;
                oss << value.as.n;
                return oss.str();
            }
#endif
        case AVA_STRING: {
            size_t len = 0;
            const char* data = ava_string_data(vm_, value, &len);
            std::string display(data, len);
            ava_value_release(vm_, value);
            return display;
        }
        case AVA_NIL:


            return rawValue;
        default:


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
        return templateStateJson;
    }
    if (!parsed.is_object()) return templateStateJson;

    json out = json::object();
    for (auto it = parsed.begin(); it != parsed.end(); ++it) {
        std::string ns, field;
        ava_value_t value{};
        bool isDictField = false;

        if (SplitNamespacedKey(it.key(), ns, field)) {
            // Mirror of BindState's write side: "dialog.confirmDialogOpen"
            // reads the "confirmDialogOpen" entry out of the "dialog"
            // Dict global instead of a flat global of that (invalid,
            // dotted) name.
            ava_value_t nsVal = ava_get_global(vm_, ns.c_str());  // owned, must release
            if (nsVal.type == AVA_DICT) {
                // ava_dict_get returns a BORROWED reference straight into
                // the dict's own entries (unlike ava_get_global, it does
                // not Retain -- see c_api.cpp) -- `value` must NOT be
                // released below, only `nsVal` is ours to release.
                value = ava_dict_get(vm_, nsVal, field.c_str());
                isDictField = true;
            }
            ava_value_release(vm_, nsVal);
        } else {
            value = ava_get_global(vm_, it.key().c_str());
        }

        switch (value.type) {
            case AVA_BOOL:
                out[it.key()] = value.as.b ? "true" : "false";
                break;
            case AVA_NUMBER:
#ifdef AVAHOST_HAS_UI_PIPELINE
                out[it.key()] = NumberToDisplayString(value.as.n);
#else
                {
                    std::ostringstream oss;
                    oss << value.as.n;
                    out[it.key()] = oss.str();
                }
#endif
                break;
            case AVA_STRING: {
                size_t len = 0;
                const char* data = ava_string_data(vm_, value, &len);
                out[it.key()] = std::string(data, len);
                if (!isDictField) ava_value_release(vm_, value);
                break;
            }
            case AVA_LIST:
            case AVA_DICT:
                // Previously this fell into `default:` below, which
                // just echoed back whatever text was already in the
                // incoming template/cached JSON -- meaning a List/Dict
                // state var's real, current contents (e.g. `cart`
                // after `cart.append(...)`) were silently discarded on
                // every export, and the *next* request re-bound the
                // stale echoed text (see BindState's matching fix).
                // Serialize the live value back to AvaLang literal
                // source text instead, so it actually round-trips.
                out[it.key()] = SerializeAvaValueToLiteral(vm_, value);
                if (!isDictField) ava_value_release(vm_, value);
                break;
            default:




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

}