#include "design/state_eval.h"

#include <cctype>
#include <cstdlib>
#include <sstream>

namespace studio::design {

namespace {

// Mirrors core/src/ui/avaui_text.cpp's anonymous-namespace LooksNumeric
// exactly (same digits/one-dot/optional-leading-sign grammar) -- kept
// as a separate copy here (rather than exported from core) since this
// side of the boundary only has the C API's ava_value_t to build with,
// not core's internal Value/StringObj types that function operates on.
bool LooksNumeric(const std::string& v) {
    if (v.empty()) return false;
    size_t i = 0;
    if (v[i] == '+' || v[i] == '-') ++i;
    if (i >= v.size()) return false;
    bool has_digits = false;
    bool has_dot = false;
    for (; i < v.size(); ++i) {
        if (std::isdigit(static_cast<unsigned char>(v[i]))) {
            has_digits = true;
        } else if (v[i] == '.' && !has_dot) {
            has_dot = true;
        } else {
            return false;
        }
    }
    return has_digits;
}

// Same convention as core's ValueToDisplayString (oss << n) for the
// number->text direction, so an evaluated number reads the same way
// a hand-written .avaui literal would.
std::string NumberToDisplayString(double n) {
    std::ostringstream oss;
    oss << n;
    return oss.str();
}

} // namespace

AvaVM* BuildStateVM(const DesignDocument& doc) {
    AvaVM* vm = ava_vm_create();
    if (!vm) return nullptr;

    for (const PropertyRow& row : doc.initial_state) {
        ava_value_t value;
        if (row.value == "true" || row.value == "false") {
            value.type = AVA_BOOL;
            value.as.b = (row.value == "true") ? 1 : 0;
        } else if (LooksNumeric(row.value)) {
            value.type = AVA_NUMBER;
            value.as.n = std::strtod(row.value.c_str(), nullptr);
        } else {
            // Not released after the set below -- same lightweight
            // convention EngineBridge::BuildDemoComponentTree already
            // uses for ava_string_create + ava_ui_set_property (no
            // matching ava_value_release there either). Bounded by
            // this VM's lifetime, which the caller owns and destroys
            // once per DrawDesignerCanvas call -- see state_eval.h.
            value = ava_string_create(vm, row.value.data(), row.value.size());
        }
        ava_set_global(vm, row.key.c_str(), value);
    }

    return vm;
}

std::string EvalPropertyExpr(AvaVM* vm, const std::string& raw_value) {
    if (!vm || raw_value.empty()) return raw_value;

    // Wrapped in parens so an expression like `"Counter: " + counter`
    // (or a plain literal/identifier) always lands as a single RHS,
    // and named `__avaui_eval__` specifically to make collision with a
    // real `state` var name astronomically unlikely (state vars are
    // ordinary AvaLang identifiers, none of which would plausibly
    // start with double underscores by convention).
    const std::string source = "__avaui_eval__ = (" + raw_value + ")";

    char* compile_error = nullptr;
    AvaModule* module = ava_compile(vm, source.c_str(), "<avaui-prop>", &compile_error);
    if (!module) {
        // Common case: raw_value is actually a plain string literal
        // ("Guardar todo", with a space) that isn't valid as a bare
        // AvaLang expression at all -- fall back to showing it as-is,
        // exactly what the canvas already did before this pass.
        if (compile_error) ava_string_free(compile_error);
        return raw_value;
    }

    ava_value_t out_result;
    char* run_error = nullptr;
    ava_run(vm, module, &out_result, &run_error);
    ava_module_destroy(module);
    if (run_error) {
        ava_string_free(run_error);
        return raw_value;
    }

    ava_value_t result = ava_get_global(vm, "__avaui_eval__");
    switch (result.type) {
        case AVA_BOOL:
            return result.as.b ? "true" : "false";
        case AVA_NUMBER:
            return NumberToDisplayString(result.as.n);
        case AVA_STRING: {
            size_t len = 0;
            const char* data = ava_string_data(vm, result, &len);
            std::string display(data, len);
            ava_value_release(vm, result);
            return display;
        }
        case AVA_NIL:
            // The common "plain string literal, not a real
            // identifier" case -- see state_eval.h's header comment on
            // why VM::GetGlobal returning Nil for an undefined name is
            // exactly the signal this needs, without touching the
            // parser to disambiguate literal vs. identifier up front.
            return raw_value;
        default:
            // List/Dict/Function/etc. aren't meaningful as a control's
            // display text -- fall back rather than show "" or a
            // handle-ish placeholder.
            ava_value_release(vm, result);
            return raw_value;
    }
}

std::string GetDisplayPropertyKey(const std::string& node_type) {
    // Every essential/content-bearing component uses `value` for "the
    // text it shows" -- see component_catalog.cpp's comment above the
    // Interactive section for why button/link/checkbox/radiobutton were
    // moved off their old `text`/`label` names onto this one shared name.
    if (node_type == "text" || node_type == "textbox" || node_type == "button" || node_type == "link" ||
        node_type == "checkbox" || node_type == "radiobutton") {
        return "value";
    }
    return "";
}

void BindCodeBehind(AvaVM* vm, const DesignDocument& doc) {
    if (!vm || doc.code_behind.empty()) return;

    // Wrapped in the same `methods ... end` shell EnsureClickHandler
    // writes into `code_behind` (see design_document.cpp / the .avaui
    // format's `methods` block, core/src/ui/avaui_text.h) -- NOT
    // strictly required (a bare sequence of `func ... end` at top
    // level compiles the same either way, `methods`/`end` aren't
    // reserved as a block keyword at the expression-compiler level),
    // but keeping it consistent with how the file itself wraps this
    // exact text means a future syntax change to that block only has
    // one place (avaui_text.cpp) to stay in sync with, not two.
    char* compile_error = nullptr;
    AvaModule* module = ava_compile(vm, doc.code_behind.c_str(), "<avaui-code-behind>", &compile_error);
    if (!module) {
        if (compile_error) ava_string_free(compile_error);
        return;
    }

    ava_value_t out_result;
    char* run_error = nullptr;
    ava_run(vm, module, &out_result, &run_error);
    ava_module_destroy(module);
    if (run_error) ava_string_free(run_error);
}

bool InvokeHandler(AvaVM* vm, const std::string& handler_name, std::string* out_error) {
    if (!vm || handler_name.empty()) return false;

    // No parens/args support (Fase 5's generated stubs are always
    // `func <name>(sender, e)`, but the .NET reference model's own
    // `click = increment` binding calls plain zero-arg handlers too --
    // see 08_DESIGNER_VIEW_PLAN.md section 0.1's example. AvaLang's
    // EmitDefaultsPrologue (core/src/compiler/compiler.cpp) fills
    // missing params with their declared defaults, or nil if none --
    // it doesn't error on an arity mismatch, so calling either shape
    // with zero args here is safe).
    const std::string source = "__avaui_invoke_result__ = " + handler_name + "()";

    char* compile_error = nullptr;
    AvaModule* module = ava_compile(vm, source.c_str(), "<avaui-handler-call>", &compile_error);
    if (!module) {
        if (compile_error) {
            if (out_error) *out_error = compile_error;
            ava_string_free(compile_error);
        }
        return false;
    }

    ava_value_t out_result;
    char* run_error = nullptr;
    ava_run(vm, module, &out_result, &run_error);
    ava_module_destroy(module);
    if (run_error) {
        if (out_error) *out_error = run_error;
        ava_string_free(run_error);
        return false;
    }

    ava_value_t result = ava_get_global(vm, "__avaui_invoke_result__");
    if (result.type == AVA_STRING) {
        // Only reference-counted result type __avaui_invoke_result__
        // could hold that isn't released elsewhere -- mirrors
        // EvalPropertyExpr's AVA_STRING case. Everything else
        // (number/bool/nil/list/dict/etc.) either isn't ref-counted or
        // isn't meaningful to release through the C API the way this
        // function uses it (the caller never reads the value itself).
        ava_value_release(vm, result);
    }
    return true;
}

} // namespace studio::design
