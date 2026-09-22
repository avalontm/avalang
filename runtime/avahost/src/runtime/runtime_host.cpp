#include "runtime/runtime_host.h"

#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <utility>

#include <nlohmann/json.hpp>

#ifdef AVAHOST_HAS_UI_PIPELINE
#include "parser/AvauiPropertyCoercion.h"
#include "runtime/vm_ava_view.h"
using avalang::ui::parser::InferValue;
using avalang::ui::parser::NumberToDisplayString;
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
    DrainAsync();
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
    ava_value_t existing = ava_get_global(vm_, ns.c_str()); 
    if (existing.type == AVA_DICT) {
        return existing; 
    }
    ava_value_release(vm_, existing);  

    ava_value_t created = ava_dict_create(vm_);
    ava_set_global(vm_, ns.c_str(), created);  
    ava_value_release(vm_, created);          
    return ava_get_global(vm_, ns.c_str());   
}

namespace {

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
            ava_value_t dict = GetOrCreateDictGlobal(ns);
            ava_dict_set(vm_, dict, field.c_str(), value);
            ava_value_release(vm_, dict);
        } else {
            ava_set_global(vm_, it.key().c_str(), value);
            if (valueOwnedFromGetGlobal) ava_value_release(vm_, value);
        }
    }
}

void RuntimeHost::BindStorage(const std::string& storageJson) {
    if (!vm_) return;

    ava_value_t storage = ava_dict_create(vm_);

    json parsed;
    bool parsedOk = false;
    try {
        parsed = json::parse(storageJson.empty() ? "{}" : storageJson);
        parsedOk = parsed.is_object();
    } catch (const json::exception&) {
        parsedOk = false;
    }

    if (parsedOk) {
        for (auto it = parsed.begin(); it != parsed.end(); ++it) {
            const std::string value = it.value().is_string() ? it.value().get<std::string>() : it.value().dump();
            ava_dict_set(vm_, storage, it.key().c_str(), ava_string_create(vm_, value.data(), value.size()));
        }
    }

    ava_set_global(vm_, "Storage", storage);
    ava_value_release(vm_, storage);
}

std::string RuntimeHost::ExportStorageJson() {
    if (!vm_) return "{}";

    ava_value_t storage = ava_get_global(vm_, "Storage");
    if (storage.type != AVA_DICT) {
        ava_value_release(vm_, storage);
        return "{}";
    }

    json out = json::object();
    void* rawEntries = nullptr;
    size_t n = ava_dict_entries(vm_, storage, &rawEntries);
    auto* entries = static_cast<ava_dict_pair_t*>(rawEntries);
    for (size_t i = 0; i < n; ++i) {
        const std::string key(entries[i].key, entries[i].key_len);
        const ava_value_t& value = entries[i].value;
        if (value.type == AVA_STRING) {
            size_t len = 0;
            const char* data = ava_string_data(vm_, value, &len);
            out[key] = std::string(data, len);
        } else {
            out[key] = SerializeAvaValueToLiteral(vm_, value);
        }
    }

    ava_value_release(vm_, storage);
    return out.dump();
}

#ifdef AVAHOST_HAS_UI_PIPELINE
bool RuntimeHost::LoadView(const std::string& viewName, const std::string& codeBehind,
                            avalang::ui::IComponent* root, std::string& outError) {
    outError.clear();
    if (!vm_) {
        outError = "cannot load view '" + viewName + "' -- VM not initialized";
        return false;
    }

    if (!avaView_) {
        avaView_ = std::make_unique<VmAvaView>(vm_);
    }

    return avaView_->Load(viewName, codeBehind, root, outError);
}
#endif

bool RuntimeHost::InvokeHandler(const std::string& handlerName, std::string& outError) {
#ifdef AVAHOST_HAS_UI_PIPELINE
    if (avaView_ && avaView_->IsLoaded()) {
        bool ok = avaView_->InvokeHandler(handlerName, outError);
        DrainAsync();
        return ok;
    }
#endif
    outError = "InvokeHandler: no view is loaded for '" + handlerName + "' -- LoadView must succeed first";
    return false;
}

void RuntimeHost::PumpAsyncOnce() {
    if (!vm_) return;
    ava_vm_pump_async(vm_);
}

void RuntimeHost::DrainAsync() {
    if (!vm_) return;
    while (ava_vm_has_pending_async(vm_)) {
        ava_vm_pump_async(vm_);
    }
}

bool RuntimeHost::HasPendingAsync() const {
    if (!vm_) return false;
    return ava_vm_has_pending_async(vm_) != 0;
}

std::string RuntimeHost::EvalSingleExpression(const std::string& rawValue) {
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

std::string RuntimeHost::EvalPropertyExpr(const std::string& rawValue) {
    if (!vm_ || rawValue.empty()) return rawValue;

    // A bare binding like `text = {clicks}` reaches here already stripped
    // of its braces ("clicks" -- see AvauiPropertyCoercion::
    // IsBalancedBraceExpression), so it's a single Avalang expression and
    // EvalSingleExpression handles it directly, same as always.
    //
    // A `$"...{expr}..."` interpolation template is NOT pre-stripped (see
    // BuildInterpolationTemplate): it arrives here as literal text with
    // one or more `{expr}` segments still embedded verbatim, e.g.
    // "clicks: {clicks}". Wrapping that whole string in "(...)" is not
    // valid Avalang, so it used to just fail to compile and fall back to
    // returning the raw template unchanged -- which is why interpolated
    // text rendered as the literal "{clicks}" instead of its value.
    // Detect that case and evaluate only the bracketed portions, splicing
    // their results back into the surrounding literal text.
    bool hasUnescapedBrace = false;
    for (size_t i = 0; i < rawValue.size(); ++i) {
        if (rawValue[i] == '\\' && i + 1 < rawValue.size()) { ++i; continue; }
        if (rawValue[i] == '{') { hasUnescapedBrace = true; break; }
    }
    if (!hasUnescapedBrace) {
        return EvalSingleExpression(rawValue);
    }

    std::string out;
    out.reserve(rawValue.size());
    size_t i = 0;
    while (i < rawValue.size()) {
        char c = rawValue[i];
        if (c == '\\' && i + 1 < rawValue.size()) {
            out += rawValue[i + 1];
            i += 2;
            continue;
        }
        if (c != '{') {
            out += c;
            ++i;
            continue;
        }

        // Scan the balanced `{ ... }` span, same brace/quote-depth
        // tracking AvauiParser::ExtractReferencedIdentifiers uses to walk
        // interpolation templates, so nested braces and quoted strings
        // inside the expression don't confuse the boundary.
        size_t start = i + 1;
        int depth = 1;
        bool inDouble = false;
        size_t j = start;
        for (; j < rawValue.size(); ++j) {
            char cj = rawValue[j];
            if (inDouble) {
                if (cj == '\\' && j + 1 < rawValue.size()) { ++j; continue; }
                if (cj == '"') inDouble = false;
                continue;
            }
            if (cj == '"') { inDouble = true; continue; }
            if (cj == '{') ++depth;
            else if (cj == '}') {
                --depth;
                if (depth == 0) break;
            }
        }

        std::string exprSource = rawValue.substr(start, j - start);
        out += EvalSingleExpression(exprSource);
        i = (j < rawValue.size()) ? j + 1 : rawValue.size();
    }
    return out;
}

std::string RuntimeHost::EvalExprToLiteral(const std::string& expr, bool& ok) {
    ok = false;
    if (!vm_ || expr.empty()) return "";

    const std::string source = "__avahost_literal__ = (" + expr + ")";
    char* compileError = nullptr;
    AvaModule* module = ava_compile(vm_, source.c_str(), "<avahost-literal-eval>", &compileError);
    if (!module) {
        if (compileError) ava_string_free(compileError);
        return "";
    }

    ava_value_t result{};
    char* runError = nullptr;
    ava_run(vm_, module, &result, &runError);
    ava_module_destroy(module);
    if (runError) {
        ava_string_free(runError);
        return "";
    }

    ava_value_t value = ava_get_global(vm_, "__avahost_literal__");
    std::string literal = SerializeAvaValueToLiteral(vm_, value);

    if (value.type != AVA_BOOL && value.type != AVA_NUMBER) {
        ava_value_release(vm_, value);
    }
    ok = true;
    return literal;
}

bool RuntimeHost::EvalAssignGlobal(const std::string& name, const std::string& expr) {
    if (!vm_ || name.empty()) return false;

    const std::string source = name + " = (" + expr + ")";
    char* compileError = nullptr;
    AvaModule* module = ava_compile(vm_, source.c_str(), "<avahost-loop-bind>", &compileError);
    if (!module) {
        if (compileError) ava_string_free(compileError);
        return false;
    }

    ava_value_t result{};
    char* runError = nullptr;
    ava_run(vm_, module, &result, &runError);
    ava_module_destroy(module);
    if (runError) {
        ava_string_free(runError);
        return false;
    }
    return true;
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
            ava_value_t nsVal = ava_get_global(vm_, ns.c_str());
            if (nsVal.type == AVA_DICT) {
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