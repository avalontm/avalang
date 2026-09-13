#include "vm.h"
#include "vm_internal.h"
#include "module.h"
#include "vm_platform_accessor.h"
#include "../frontend/frontend.h"
#include "../../platform/barekernel/stdcompat/ava_stdcompat.h"

namespace ava {

static void AttachModuleGlobals(const avastd::shared_ptr<Proto>& proto,
                                const avastd::shared_ptr<avastd::unordered_map<avastd::string, Value>>& mod_globals) {
    if (!proto) return;
    proto->module_globals = mod_globals;
    for (auto& child : proto->child_protos) {
        AttachModuleGlobals(child, mod_globals);
    }
}

static void PlaceModuleInScope(VM& vm, const avastd::string& /*module_path*/,
                                const avastd::string& alias, Value module_dict) {
    if (!alias.empty()) {
        vm.SetGlobal(alias, module_dict);
        return;
    }

    // Import per individual file: a dotted module path (e.g. "Models.Dog") is
    // just a way to locate the file (Models/Dog.ava) -- it does not create a
    // nested namespace. Every alias-less import flattens the module's
    // top-level symbols directly into the importing scope.
    auto* dict = static_cast<DictObj*>(module_dict.obj);
    for (auto& entry : dict->entries) {
        vm.SetGlobal(entry.first, entry.second);
    }
}

avastd::string VM::FindNativeModuleExporting(const avastd::string& symbol) const {
    for (auto& entry : native_modules_) {

        Value module_dict = entry.second(const_cast<VM&>(*this));
        if (module_dict.type == ValueType::Dict) {
            auto* dict = static_cast<DictObj*>(module_dict.obj);
            if (dict->index.find(symbol) != dict->index.end()) {
                return entry.first;
            }
        }
    }
    return avastd::string();
}

Value VM::DoImport(const avastd::string& module_path, const avastd::string& alias) {

    auto native_it = native_modules_.find(module_path);
    if (native_it != native_modules_.end()) {
        Value module_dict = native_it->second(*this);
        PlaceModuleInScope(*this, module_path, alias, module_dict);
        return Value::Nil();
    }

    if (module_cache_.HasModuleValue(module_path)) {
        Value cached_dict = module_cache_.GetModuleValue(module_path);
        PlaceModuleInScope(*this, module_path, alias, cached_dict);
        return Value::Nil();
    }

    if (module_cache_.IsLoading(module_path)) {
        return Value::Nil();
    }

    avastd::string current_dir = GetCurrentDir();
    
    avastd::string resolved_path = module_resolver_.ResolveModulePath(module_path, current_dir);
    if (resolved_path.empty()) {
        if (module_resolver_.IsModulePathADirectory(module_path, current_dir)) {
            AVA_THROW(avastd::runtime_error(
                "'" + module_path + "' is a folder, not a module -- import the specific file inside it instead, "
                "e.g. 'import " + module_path + ".<FileName>'"));
        }
        AVA_THROW(avastd::runtime_error("could not find module: " + module_path));
    }

    module_cache_.BeginLoading(module_path);

    AVA_TRY {
        if (!module_cache_.Exists(module_path)) {

            avastd::string source;
            {
                const avastd::string& file = resolved_path;

                if (before_module_read_hook_) {
                    before_module_read_hook_(file);
                }

                avastd::string file_source;
                bool read_ok = VmPlatformAccessor::Get().FileSystem().ReadFile(file, file_source);

                if (after_module_read_hook_) {
                    after_module_read_hook_(file);
                }

                if (!read_ok) {
                    AVA_THROW(avastd::runtime_error("could not open module file: " + file));
                }

                source += file_source;
                source += "\n";
            }

            avastd::string prev_dir = GetCurrentDir();
            avastd::string prev_module = current_module_;
            SetCurrentDir(GetFileDir(resolved_path));
            current_module_ = resolved_path;

            avastd::shared_ptr<Proto> compiled_proto;
            AVA_TRY {
                compiled_proto = CompileSource(source, resolved_path);
            } AVA_CATCH(avastd::exception, e) {
                (void)e;
                SetCurrentDir(prev_dir);
                current_module_ = prev_module;
                AVA_RETHROW();
            }
            module_cache_.Add(module_path, avastd::move(compiled_proto), resolved_path);
            SetCurrentDir(prev_dir);
            current_module_ = prev_module;
        }

        auto proto = module_cache_.Get(module_path);

        auto module_globals = avastd::make_shared<avastd::unordered_map<avastd::string, Value>>();
        AttachModuleGlobals(proto, module_globals);

        avastd::unordered_map<avastd::string, Value> outer_globals = avastd::move(globals_);
        globals_.clear();

        auto import_fn = outer_globals.find("__import__");
        if (import_fn != outer_globals.end()) {
            SetGlobal("__import__", import_fn->second);
        }

        CallFrame frame;
        frame.registers.resize(proto->num_registers);
        frame.proto = avastd::move(proto);
        frames_.push_back(avastd::move(frame));

        ExecuteFrame(frames_.size() - 1);

        CloseUpvalues(frames_.back());
        frames_.pop_back();

        for (auto& entry : globals_) {
            if (entry.first != "__import__") {
                (*module_globals)[entry.first] = entry.second;
            }
        }

        Value module_dict;
        module_dict.type = ValueType::Dict;
        module_dict.obj = new DictObj();

        auto* dict = static_cast<DictObj*>(module_dict.obj);
        for (auto& entry : *module_globals) {
            dict->index[entry.first] = dict->entries.size();
            dict->entries.emplace_back(entry.first, entry.second);
        }

        module_cache_.SetModuleValue(module_path, module_dict);
        module_cache_.EndLoading(module_path);

        globals_ = avastd::move(outer_globals);
        PlaceModuleInScope(*this, module_path, alias, module_dict);
    } AVA_CATCH(avastd::exception, e) {
        (void)e;
        module_cache_.EndLoading(module_path);
        module_cache_.Remove(module_path);
        AVA_RETHROW();
    }

    return Value::Nil();
}

} // namespace ava