#include "vm.h"
#include "vm_internal.h"
#include "module.h"
#include "vm_platform_accessor.h"
#include "../frontend/frontend.h"
#include "../../platform/barekernel/stdcompat/ava_stdcompat.h"

namespace ava {

static avastd::string JoinModulePath(const avastd::string& dir, const avastd::string& name) {
    if (dir.empty()) return name;
    char last = dir.back();
    if (last == '/' || last == '\\') return dir + name;
#ifdef _WIN32
    return dir + "\\" + name;
#else
    return dir + "/" + name;
#endif
}

static void AttachModuleGlobals(const avastd::shared_ptr<Proto>& proto,
                                const avastd::shared_ptr<avastd::unordered_map<avastd::string, Value>>& mod_globals) {
    if (!proto) return;
    proto->module_globals = mod_globals;
    for (auto& child : proto->child_protos) {
        AttachModuleGlobals(child, mod_globals);
    }
}

static void SetNestedNamespace(
    avastd::unordered_map<avastd::string, Value>& globals,
    const avastd::vector<avastd::string>& parts,

    Value module_dict) {

    DictObj* current;
    {
        auto it = globals.find(parts[0]);
        if (it != globals.end() && it->second.type == ValueType::Dict) {
            current = static_cast<DictObj*>(it->second.obj);
        } else {
            Value ns_val;
            ns_val.type = ValueType::Dict;
            ns_val.obj = new DictObj();
            if (it != globals.end()) {
                it->second = ns_val;
            } else {
                globals.emplace(parts[0], ns_val);
            }
            current = static_cast<DictObj*>(ns_val.obj);
        }
    }

    for (size_t i = 1; i + 1 < parts.size(); ++i) {
        auto idx_it = current->index.find(parts[i]);
        if (idx_it != current->index.end() && current->entries[idx_it->second].second.type == ValueType::Dict) {
            current = static_cast<DictObj*>(current->entries[idx_it->second].second.obj);
        } else {
            Value ns_val;
            ns_val.type = ValueType::Dict;
            ns_val.obj = new DictObj();
            if (idx_it != current->index.end()) {
                current->entries[idx_it->second].second = ns_val;
            } else {
                current->index[parts[i]] = current->entries.size();
                current->entries.emplace_back(parts[i], ns_val);
            }
            current = static_cast<DictObj*>(ns_val.obj);
        }
    }

    const avastd::string& leaf = parts.back();
    auto leaf_it = current->index.find(leaf);
    if (leaf_it != current->index.end()) {
        current->entries[leaf_it->second].second = module_dict;
    } else {
        current->index[leaf] = current->entries.size();
        current->entries.emplace_back(leaf, module_dict);
    }
}

static void PlaceModuleInScope(VM& vm, const avastd::string& module_path,
                                const avastd::string& alias, Value module_dict) {
    if (!alias.empty()) {
        vm.SetGlobal(alias, module_dict);
        return;
    }

    avastd::vector<avastd::string> parts;
    avastd::string temp = module_path;
    size_t start = 0;
    while ((start = temp.find('.')) != avastd::string::npos) {
        parts.push_back(temp.substr(0, start));
        temp = temp.substr(start + 1);
    }
    parts.push_back(temp);

    if (parts.size() > 1) {
        SetNestedNamespace(vm.Globals(), parts, module_dict);
    } else {
        auto* dict = static_cast<DictObj*>(module_dict.obj);
        for (auto& entry : dict->entries) {
            vm.SetGlobal(entry.first, entry.second);
        }
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
        AVA_THROW(avastd::runtime_error("could not find module: " + module_path));
    }

    module_cache_.BeginLoading(module_path);

    AVA_TRY {
        if (!module_cache_.Exists(module_path)) {

            bool is_folder_module = VmPlatformAccessor::Get().FileSystem().IsDirectory(resolved_path);
            avastd::vector<avastd::string> source_files =
                is_folder_module ? ModuleResolver::ListLooseAvaFiles(resolved_path)
                                  : avastd::vector<avastd::string>{resolved_path};
            avastd::string compile_source_name =
                is_folder_module ? JoinModulePath(resolved_path, "(namespace).ava") : resolved_path;

            avastd::string source;
            for (const avastd::string& file : source_files) {

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
            SetCurrentDir(GetFileDir(compile_source_name));
            current_module_ = compile_source_name;

            avastd::shared_ptr<Proto> compiled_proto;
            AVA_TRY {
                compiled_proto = CompileSource(source, compile_source_name);
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