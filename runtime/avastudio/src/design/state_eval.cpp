#include "design/state_eval.h"

#include "parser/AvauiPropertyCoercion.h"

namespace studio::design {

namespace {
using avalang::ui::parser::LooksLikeCall;
using avalang::ui::parser::NumberToDisplayString;
}

AvaVM* BuildStateVM(const DesignDocument& doc) {
    AvaVM* vm = ava_vm_create();
    if (!vm) return nullptr;

    for (const PropertyRow& row : doc.initial_state) {
        auto pv = avalang::ui::parser::InferValue(row.value);
        ava_value_t value;
        switch (pv.Type()) {
            case avalang::ui::PropertyType::Bool:
                value.type = AVA_BOOL;
                value.as.b = pv.AsBool() ? 1 : 0;
                break;
            case avalang::ui::PropertyType::Number:
                value.type = AVA_NUMBER;
                value.as.n = pv.AsNumber();
                break;
            default:
                value = ava_string_create(vm, row.value.data(), row.value.size());
                break;
        }
        ava_set_global(vm, row.key.c_str(), value);
    }

    return vm;
}

std::string EvalPropertyExpr(AvaVM* vm, const std::string& raw_value) {
    if (!vm || raw_value.empty()) return raw_value;







    const std::string source = "__avaui_eval__ = (" + raw_value + ")";

    char* compile_error = nullptr;
    AvaModule* module = ava_compile(vm, source.c_str(), "<avaui-prop>", &compile_error);
    if (!module) {




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





            return raw_value;
        default:



            ava_value_release(vm, result);
            return raw_value;
    }
}

std::string GetDisplayPropertyKey(const std::string& node_type) {




    if (node_type == "text" || node_type == "textbox" || node_type == "button" || node_type == "link" ||
        node_type == "checkbox" || node_type == "radiobutton") {
        return "value";
    }
    return "";
}

void BindCodeBehind(AvaVM* vm, const DesignDocument& doc) {
    if (!vm || doc.code_behind.empty()) return;










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













    const std::string source = "__avaui_invoke_result__ = " + handler_name +
                                (LooksLikeCall(handler_name) ? "" : "()");

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






        ava_value_release(vm, result);
    }
    return true;
}

}