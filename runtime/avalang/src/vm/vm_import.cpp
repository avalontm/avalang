#include "vm.h"
#include "vm_internal.h"
#include "module.h"
#include "../frontend/frontend.h"
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace ava {

static void SetNestedNamespace(
    std::unordered_map<std::string, Value>& globals,
    const std::vector<std::string>& parts,
    size_t part_idx,
    Value module_dict) {
    if (part_idx == parts.size() - 1) {
        Retain(module_dict);
        auto it = globals.find(parts[part_idx]);
        if (it != globals.end()) {
            Release(it->second);
            it->second = module_dict;
        } else {
            globals.emplace(parts[part_idx], module_dict);
        }
        Release(module_dict);
    } else {
        Value ns_val;
        ns_val.type = ValueType::Dict;
        ns_val.obj = new DictObj();
        Retain(ns_val);
        
        Value existing = globals.find(parts[part_idx]) != globals.end() 
            ? globals.at(parts[part_idx]) : Value::Nil();
        if (existing.type == ValueType::Dict) {
            auto* existing_dict = static_cast<DictObj*>(existing.obj);
            std::string next_key = parts[part_idx + 1];
            auto next_it = existing_dict->index.find(next_key);
            if (next_it != existing_dict->index.end()) {
                Release(existing_dict->entries[next_it->second].second);
                existing_dict->entries[next_it->second].second = ns_val;
            } else {
                existing_dict->index[next_key] = existing_dict->entries.size();
                existing_dict->entries.emplace_back(next_key, ns_val);
            }
            Retain(ns_val);
        } else {
            globals.emplace(parts[part_idx], ns_val);
        }
        
        SetNestedNamespace(globals, parts, part_idx + 1, module_dict);
        Release(ns_val);
    }
}

Value VM::DoImport(const std::string& module_path, const std::string& alias) {
    std::string current_dir = GetCurrentDir();
    
    std::string resolved_path = module_resolver_.ResolveModulePath(module_path, current_dir);
    if (resolved_path.empty()) {
        throw std::runtime_error("could not find module: " + module_path);
    }
    
    if (!module_cache_.Exists(module_path)) {
        module_cache_.BeginLoading(module_path);
        
        std::ifstream file(resolved_path);
        if (!file.is_open()) {
            module_cache_.EndLoading(module_path);
            throw std::runtime_error("could not open module file: " + resolved_path);
        }
        
        std::stringstream ss;
        ss << file.rdbuf();
        std::string source = ss.str();
        file.close();
        
        std::string prev_dir = GetCurrentDir();
        std::string prev_module = current_module_;
        SetCurrentDir(GetFileDir(resolved_path));
        current_module_ = resolved_path;
        
        try {
            auto proto = CompileSource(source, resolved_path);
            module_cache_.Add(module_path, proto, resolved_path);
            SetCurrentDir(prev_dir);
            current_module_ = prev_module;
            module_cache_.EndLoading(module_path);
        } catch (...) {
            SetCurrentDir(prev_dir);
            current_module_ = prev_module;
            module_cache_.EndLoading(module_path);
            module_cache_.Remove(module_path);
            throw;
        }
    }
    
    auto proto = module_cache_.Get(module_path);
    
    std::unordered_map<std::string, Value> outer_globals = std::move(globals_);
    globals_.clear();
    
    auto import_fn = outer_globals.find("__import__");
    if (import_fn != outer_globals.end()) {
        SetGlobal("__import__", import_fn->second);
    }
    
    CallFrame frame;
    frame.proto = proto;
    frame.registers.resize(proto->num_registers);
    frames_.push_back(std::move(frame));
    
    ExecuteFrame(frames_.size() - 1);
    
    frames_.pop_back();
    
    Value module_dict;
    module_dict.type = ValueType::Dict;
    module_dict.obj = new DictObj();
    Retain(module_dict);
    
    auto* dict = static_cast<DictObj*>(module_dict.obj);
    for (auto& entry : globals_) {
        if (entry.first == "__import__") continue;
        dict->index[entry.first] = dict->entries.size();
        dict->entries.emplace_back(entry.first, entry.second);
        Retain(entry.second);
    }
    
    if (alias.empty()) {
        std::vector<std::string> parts;
        std::string temp = module_path;
        size_t start = 0;
        while ((start = temp.find('.')) != std::string::npos) {
            parts.push_back(temp.substr(0, start));
            temp = temp.substr(start + 1);
        }
        parts.push_back(temp);
        
        if (parts.size() > 1) {
            std::vector<std::string> ns_parts(parts.begin(), parts.end() - 1);
            globals_ = std::move(outer_globals);
            SetNestedNamespace(globals_, ns_parts, 0, module_dict);
        } else {
            // Import de un solo segmento sin alias -- el caso común,
            // `import Dog` para usar la clase `Dog` definida en
            // dog.ava/Dog.ava -- exactamente como ya asume el editor
            // (ClassIndex::ScanImports en
            // studio/src/languages/class_index.cpp sigue los imports y
            // vuelca sus clases DIRECTO al índice, sin ningún prefijo
            // de namespace). Antes, acá se hacía
            // `SetGlobal(module_path, module_dict)`: el global "Dog"
            // terminaba apuntando al DICCIONARIO del módulo entero, no
            // a la clase -- así que `Dog()` fallaba en runtime con
            // "attempt to call a non-callable value" (un Dict no es
            // invocable) aunque el editor lo autocompletara y coloreara
            // como si fuera perfectamente válido (ver
            // test.ava/scripts/dog.ava, y la Fase 5 de
            // TODO_autocompletado_miembros.md que asume lo mismo).
            //
            // Fix: en vez de exponer el módulo como un único namespace
            // bajo su propio nombre, cada definición de nivel superior
            // del módulo (`class Dog`, cualquier `func` suelta, etc.) se
            // vuelca directo al scope del importador -- mismo efecto
            // que un "from Dog import *" -- así que `Dog()` referencia
            // la clase de verdad. Los imports con punto (`import
            // a.b.c`, arriba) y los que usan `as alias` (abajo) NO se
            // tocan: ahí el nombre elegido a propósito por quien
            // escribe el import sí tiene sentido como namespace
            // explícito, en vez de volcarse a ciegas al scope de quien
            // importa.
            globals_ = std::move(outer_globals);
            for (auto& entry : dict->entries) {
                SetGlobal(entry.first, entry.second);
            }
            Release(module_dict);
        }
    } else {
        globals_ = std::move(outer_globals);
        SetGlobal(alias, module_dict);
        Release(module_dict);
    }
    
    return Value::Nil();
}

} // namespace ava
