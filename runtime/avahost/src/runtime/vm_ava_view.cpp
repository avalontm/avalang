#include "runtime/vm_ava_view.h"

namespace avahost {

namespace {

using avalang::ui::PropertyRecord;
using avalang::ui::PropertyType;
using avalang::ui::PropertyValue;

const char* AvaViewClassSource() {
    return
        "class AvaView\n"
        "    func onLoad()\n"
        "    end\n"
        "    func onUnload()\n"
        "    end\n"
        "end\n";
}

bool IsRefCounted(ava_value_t value) {
    return value.type == AVA_STRING || value.type == AVA_LIST || value.type == AVA_DICT ||
           value.type == AVA_INSTANCE;
}

ava_value_t PropertyValueToAva(AvaVM* vm, const PropertyValue& value) {
    switch (value.Type()) {
        case PropertyType::Bool: {
            ava_value_t v;
            v.type = AVA_BOOL;
            v.as.b = value.AsBool() ? 1 : 0;
            return v;
        }
        case PropertyType::Number: {
            ava_value_t v;
            v.type = AVA_NUMBER;
            v.as.n = value.AsNumber();
            return v;
        }
        case PropertyType::String:
            return ava_string_create(vm, value.AsString().c_str(), value.AsString().size());
        case PropertyType::List: {
            ava_value_t list = ava_list_create(vm);
            for (const PropertyRecord& item : value.AsList()) {
                ava_value_t dict = ava_dict_create(vm);
                for (const auto& [key, fieldValue] : item) {
                    ava_value_t converted = PropertyValueToAva(vm, fieldValue);
                    ava_dict_set(vm, dict, key.c_str(), converted);
                    if (IsRefCounted(converted)) ava_value_release(vm, converted);
                }
                ava_list_append(vm, list, dict);
                ava_value_release(vm, dict);
            }
            return list;
        }
        default:
            return ava_value_t{};
    }
}

PropertyValue AvaScalarToPropertyValue(AvaVM* vm, ava_value_t value) {
    switch (value.type) {
        case AVA_BOOL:
            return PropertyValue(value.as.b != 0);
        case AVA_NUMBER:
            return PropertyValue(value.as.n);
        case AVA_STRING: {
            size_t len = 0;
            const char* data = ava_string_data(vm, value, &len);
            return PropertyValue(std::string(data, len));
        }
        default:
            return PropertyValue();
    }
}

ava_value_t PropertyRecordToAvaDict(AvaVM* vm, const PropertyRecord& props) {
    ava_value_t dict = ava_dict_create(vm);
    for (const auto& [key, value] : props) {
        ava_value_t converted = PropertyValueToAva(vm, value);
        ava_dict_set(vm, dict, key.c_str(), converted);
        if (IsRefCounted(converted)) ava_value_release(vm, converted);
    }
    return dict;
}

PropertyRecord AvaDictToPropertyRecord(AvaVM* vm, ava_value_t dict) {
    PropertyRecord record;
    if (dict.type != AVA_DICT) return record;

    void* entriesPtr = nullptr;
    size_t count = ava_dict_entries(vm, dict, &entriesPtr);
    if (!entriesPtr) return record;

    auto* pairs = static_cast<ava_dict_pair_t*>(entriesPtr);
    for (size_t i = 0; i < count; ++i) {
        std::string key(pairs[i].key, pairs[i].key_len);
        record[key] = AvaScalarToPropertyValue(vm, pairs[i].value);
    }
    return record;
}

}  // namespace

VmAvaView::VmAvaView(AvaVM* vm) : vm_(vm), instance_(ava_value_t{}) {}

VmAvaView::~VmAvaView() {
    if (vm_ && instance_.type == AVA_INSTANCE) {
        ava_value_release(vm_, instance_);
    }
}

bool VmAvaView::Load(const std::string& viewName, const std::string& codeBehind,
                      avalang::ui::IComponent* root, std::string& outError) {
    outError.clear();

    if (!vm_) {
        outError = "cannot load view '" + viewName + "' -- no VM attached";
        return false;
    }
    if (viewName.empty()) {
        outError = "cannot load a view with an empty name";
        return false;
    }

    if (instance_.type == AVA_INSTANCE) {
        ava_value_release(vm_, instance_);
        instance_ = ava_value_t{};
    }
    loaded_ = false;

    const std::string source = std::string(AvaViewClassSource()) +
        "class " + viewName + " : AvaView\n" + codeBehind + "\nend\n";
    const std::string sourceName = "<avaui-view:" + viewName + ">";

    char* compileError = nullptr;
    AvaModule* module = ava_compile(vm_, source.c_str(), sourceName.c_str(), &compileError);
    if (!module) {
        outError = compileError ? compileError : "unknown compile error in view '" + viewName + "'";
        if (compileError) ava_string_free(compileError);
        return false;
    }

    ava_value_t runResult{};
    char* runError = nullptr;
    ava_run(vm_, module, &runResult, &runError);
    ava_module_destroy(module);
    if (runError) {
        outError = runError;
        ava_string_free(runError);
        return false;
    }

    ava_value_t classValue = ava_get_global(vm_, viewName.c_str());
    if (classValue.type != AVA_CLASS) {
        ava_value_release(vm_, classValue);
        outError = "class '" + viewName + "' was not defined by its own code block";
        return false;
    }

    char* instanceError = nullptr;
    ava_value_t instance = ava_new_instance(vm_, classValue, nullptr, 0, &instanceError);
    ava_value_release(vm_, classValue);
    if (instanceError) {
        outError = instanceError;
        ava_string_free(instanceError);
        return false;
    }

    instance_ = instance;
    viewName_ = viewName;
    loaded_ = true;

    if (root) {
        std::unordered_set<std::string> seenIds;
        BindComponentRefsRecursive(root, seenIds);
    }

    return true;
}

void VmAvaView::BindComponentRefsRecursive(avalang::ui::IComponent* node,
                                            std::unordered_set<std::string>& seenIds) {
    if (!node) return;

    const PropertyValue* idProp = node->GetProperty("id");
    if (idProp && idProp->Type() == PropertyType::String && !idProp->AsString().empty()) {
        const std::string compId = idProp->AsString();
        if (seenIds.insert(compId).second) {
            PropertyRecord props;
            for (const auto& key : node->PropertyNames()) {
                if (key == "id") continue;
                if (const PropertyValue* value = node->GetProperty(key)) {
                    props[key] = *value;
                }
            }
            BindComponentRef(compId, props);
        }
    }

    for (avalang::ui::IComponent* child : node->Children()) {
        BindComponentRefsRecursive(child, seenIds);
    }
}

bool VmAvaView::OnLoad(std::string& outError) {
    return InvokeHandler("onLoad", outError);
}

bool VmAvaView::OnUnload(std::string& outError) {
    return InvokeHandler("onUnload", outError);
}

bool VmAvaView::InvokeHandler(const std::string& handlerName, std::string& outError) {
    outError.clear();
    if (!vm_ || instance_.type != AVA_INSTANCE) {
        outError = "view '" + viewName_ + "' has not been loaded";
        return false;
    }

    const bool hasArgs = !handlerName.empty() && handlerName.back() == ')' &&
                          handlerName.find('(') != std::string::npos;

    if (!hasArgs) {
        char* callError = nullptr;
        ava_value_t result = ava_call_method(vm_, instance_, handlerName.c_str(), nullptr, 0, &callError);
        if (callError) {
            outError = callError;
            ava_string_free(callError);
            return false;
        }
        if (IsRefCounted(result)) ava_value_release(vm_, result);
        return true;
    }

    const char* invokeTarget = "__ava_view_invoke_target__";
    ava_set_global(vm_, invokeTarget, instance_);

    const std::string source = std::string(invokeTarget) + "." + handlerName;
    const std::string sourceName = "<avaui-handler:" + viewName_ + ">";

    char* compileError = nullptr;
    AvaModule* module = ava_compile(vm_, source.c_str(), sourceName.c_str(), &compileError);
    if (!module) {
        outError = compileError ? compileError : "unknown compile error invoking handler '" + handlerName + "'";
        if (compileError) ava_string_free(compileError);
        ava_set_global(vm_, invokeTarget, ava_value_t{});
        return false;
    }

    ava_value_t result{};
    char* runError = nullptr;
    ava_run(vm_, module, &result, &runError);
    ava_module_destroy(module);
    ava_set_global(vm_, invokeTarget, ava_value_t{});

    if (runError) {
        outError = runError;
        ava_string_free(runError);
        return false;
    }

    if (IsRefCounted(result)) ava_value_release(vm_, result);
    return true;
}

void VmAvaView::BindComponentRef(const std::string& id, const PropertyRecord& props) {
    if (!vm_ || instance_.type != AVA_INSTANCE) return;

    ava_value_t dict = PropertyRecordToAvaDict(vm_, props);
    ava_set_attr(vm_, instance_, id.c_str(), dict);
    ava_value_release(vm_, dict);
}

PropertyRecord VmAvaView::ExportComponentRef(const std::string& id) {
    if (!vm_ || instance_.type != AVA_INSTANCE) return PropertyRecord();

    ava_value_t dict = ava_get_attr(vm_, instance_, id.c_str());
    PropertyRecord record = AvaDictToPropertyRecord(vm_, dict);
    if (dict.type == AVA_DICT) ava_value_release(vm_, dict);
    return record;
}

PropertyValue VmAvaView::GetAttr(const std::string& name) {
    if (!vm_ || instance_.type != AVA_INSTANCE) return PropertyValue();

    ava_value_t value = ava_get_attr(vm_, instance_, name.c_str());
    PropertyValue result = AvaScalarToPropertyValue(vm_, value);
    if (IsRefCounted(value)) ava_value_release(vm_, value);
    return result;
}

void VmAvaView::SetAttr(const std::string& name, PropertyValue value) {
    if (!vm_ || instance_.type != AVA_INSTANCE) return;

    ava_value_t converted = PropertyValueToAva(vm_, value);
    ava_set_attr(vm_, instance_, name.c_str(), converted);
    if (IsRefCounted(converted)) ava_value_release(vm_, converted);
}

}  // namespace avahost
