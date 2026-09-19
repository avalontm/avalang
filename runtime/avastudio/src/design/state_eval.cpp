#include "design/state_eval.h"

#include <unordered_map>

#include "parser/AvauiPropertyCoercion.h"

namespace studio::design {

namespace {
using avalang::ui::parser::NumberToDisplayString;

std::unordered_map<AvaVM*, ava_value_t>& PreviewInstances() {
    static std::unordered_map<AvaVM*, ava_value_t> instances;
    return instances;
}

const char* PreviewViewClassSource() {
    return
        "class AvaView\n"
        "    func onLoad()\n"
        "    end\n"
        "    func onUnload()\n"
        "    end\n"
        "end\n";
}

ava_value_t GetPreviewInstance(AvaVM* vm) {
    auto it = PreviewInstances().find(vm);
    if (it == PreviewInstances().end()) return ava_value_t{};
    return it->second;
}
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

    ava_value_t out_result{};
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
    static const std::unordered_map<std::string, std::string> kDisplayPropertyByType{
        {"text", "text"},
        {"textbox", "text"},
        {"button", "text"},
        {"link", "text"},
        {"checkbox", "label"},
        {"radiobutton", "label"},
    };
    const auto it = kDisplayPropertyByType.find(node_type);
    return it != kDisplayPropertyByType.end() ? it->second : std::string();
}

std::string GetCheckedPropertyKey(const std::string& node_type) {
    static const std::unordered_map<std::string, std::string> kCheckedPropertyByType{
        {"checkbox", "isChecked"},
        {"radiobutton", "isSelected"},
    };
    const auto it = kCheckedPropertyByType.find(node_type);
    return it != kCheckedPropertyByType.end() ? it->second : std::string();
}

void BindCodeBehind(AvaVM* vm, const DesignDocument& doc) {
    if (!vm) return;

    ReleasePreviewInstance(vm);
    if (doc.code_behind.empty()) return;

    const std::string source = std::string(PreviewViewClassSource()) +
        "class DesignPreview : AvaView\n" + doc.code_behind + "\nend\n";

    char* compile_error = nullptr;
    AvaModule* module = ava_compile(vm, source.c_str(), "<avaui-code-behind>", &compile_error);
    if (!module) {
        if (compile_error) ava_string_free(compile_error);
        return;
    }

    ava_value_t out_result;
    char* run_error = nullptr;
    ava_run(vm, module, &out_result, &run_error);
    ava_module_destroy(module);
    if (run_error) {
        ava_string_free(run_error);
        return;
    }

    ava_value_t class_value = ava_get_global(vm, "DesignPreview");
    if (class_value.type != AVA_CLASS) {
        ava_value_release(vm, class_value);
        return;
    }

    char* instance_error = nullptr;
    ava_value_t instance = ava_new_instance(vm, class_value, nullptr, 0, &instance_error);
    ava_value_release(vm, class_value);
    if (instance_error) {
        ava_string_free(instance_error);
        return;
    }

    PreviewInstances()[vm] = instance;
}

bool InvokeHandler(AvaVM* vm, const std::string& handler_name, std::string* out_error) {
    if (!vm || handler_name.empty()) return false;

    ava_value_t instance = GetPreviewInstance(vm);
    if (instance.type != AVA_INSTANCE) return false;

    std::string callee_name = handler_name;
    std::string call_suffix = "()";
    auto paren = callee_name.find('(');
    if (paren != std::string::npos) {
        call_suffix = callee_name.substr(paren);
        callee_name = callee_name.substr(0, paren);
    }

    ava_value_t bound = ava_get_attr(vm, instance, callee_name.c_str());
    if (bound.type != AVA_BOUND) {
        ava_value_release(vm, bound);
        return false;
    }
    ava_set_global(vm, "__avaui_preview_bound__", bound);
    ava_value_release(vm, bound);

    const std::string source = "__avaui_invoke_result__ = __avaui_preview_bound__" + call_suffix;

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

void ReleasePreviewInstance(AvaVM* vm) {
    if (!vm) return;

    auto it = PreviewInstances().find(vm);
    if (it == PreviewInstances().end()) return;

    ava_value_release(vm, it->second);
    PreviewInstances().erase(it);
}

}
