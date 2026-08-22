#include "vm.h"
#include "vm_internal.h"
#include "module.h"
#include "vm_platform_accessor.h"
#include "../frontend/frontend.h"
#include "../../platform/barekernel/stdcompat/ava_stdcompat.h"

namespace ava {

static void SetNestedNamespace(
    avastd::unordered_map<avastd::string, Value>& globals,
    const avastd::vector<avastd::string>& parts,
    size_t part_idx,
    // Sub-fase 3 (Fase 5 GC): `module_dict` llega POR VALOR -- la copia
    // hecha al llamar a esta funcion ya Retiene (sub-fase 2, RAII), asi
    // que este parametro ya es dueño de una referencia propia que su
    // propio destructor libera solo al salir de la funcion. Todo el
    // Retain/Release manual que habia antes sobre `module_dict` y
    // `ns_val` duplicaba eso -- y en el caso de `it->second =
    // module_dict` despues de un `Release(it->second)` manual, el
    // Release() automatico del operator= volvia a liberar el mismo
    // valor viejo por segunda vez (double-release real, no solo leak).
    Value module_dict) {
    if (part_idx == parts.size() - 1) {
        auto it = globals.find(parts[part_idx]);
        if (it != globals.end()) {
            it->second = module_dict;
        } else {
            globals.emplace(parts[part_idx], module_dict);
        }
    } else {
        Value ns_val;
        ns_val.type = ValueType::Dict;
        ns_val.obj = new DictObj();

        Value existing = globals.find(parts[part_idx]) != globals.end() 
            ? globals.at(parts[part_idx]) : Value::Nil();
        if (existing.type == ValueType::Dict) {
            auto* existing_dict = static_cast<DictObj*>(existing.obj);
            avastd::string next_key = parts[part_idx + 1];
            auto next_it = existing_dict->index.find(next_key);
            if (next_it != existing_dict->index.end()) {
                existing_dict->entries[next_it->second].second = ns_val;
            } else {
                existing_dict->index[next_key] = existing_dict->entries.size();
                existing_dict->entries.emplace_back(next_key, ns_val);
            }
        } else {
            globals.emplace(parts[part_idx], ns_val);
        }
        
        SetNestedNamespace(globals, parts, part_idx + 1, module_dict);
    }
}

Value VM::DoImport(const avastd::string& module_path, const avastd::string& alias) {
    avastd::string current_dir = GetCurrentDir();
    
    avastd::string resolved_path = module_resolver_.ResolveModulePath(module_path, current_dir);
    if (resolved_path.empty()) {
        AVA_THROW(avastd::runtime_error("could not find module: " + module_path));
    }
    
    if (!module_cache_.Exists(module_path)) {
        module_cache_.BeginLoading(module_path);

        // Fase 4 (avapack): si hay un hook instalado (ver vm.h), esta es
        // la unica ventana en la que un runtime empacado necesita que
        // resolved_path exista en disco -- se materializa acá y se borra
        // apenas termina el rdbuf() de abajo, no cuando termina de
        // compilar ni cuando termina el programa. Sin hook (caso normal
        // de ava_cli/avahost), before_module_read_hook_/after_module_read_hook_
        // son std::function vacios y este bloque no hace nada distinto a
        // antes de este cambio.
        if (before_module_read_hook_) {
            before_module_read_hook_(resolved_path);
        }

        // Fase 7 (avapack, filesystem virtual en memoria): antes esto leia
        // con std::ifstream crudo, bypaseando el PAL por completo -- un
        // MemoryFileSystem instalado via VmPlatformAccessor::SetOverride
        // quedaba inerte porque nunca se lo llamaba desde aca. Ahora pasa
        // siempre por el IPlatform activo (real o overrideado), igual que
        // ya hacia ModuleResolver::ResolveModulePath (module.cpp).
        avastd::string source;
        bool read_ok = VmPlatformAccessor::Get().FileSystem().ReadFile(resolved_path, source);
        if (!read_ok) {
            if (after_module_read_hook_) after_module_read_hook_(resolved_path);
            module_cache_.EndLoading(module_path);
            AVA_THROW(avastd::runtime_error("could not open module file: " + resolved_path));
        }

        if (after_module_read_hook_) {
            after_module_read_hook_(resolved_path);
        }

        avastd::string prev_dir = GetCurrentDir();
        avastd::string prev_module = current_module_;
        SetCurrentDir(GetFileDir(resolved_path));
        current_module_ = resolved_path;
        
        AVA_TRY {
            auto proto = CompileSource(source, resolved_path);
            module_cache_.Add(module_path, avastd::move(proto), resolved_path);
            SetCurrentDir(prev_dir);
            current_module_ = prev_module;
            module_cache_.EndLoading(module_path);
        } AVA_CATCH(avastd::exception, e) {
            (void)e;
            SetCurrentDir(prev_dir);
            current_module_ = prev_module;
            module_cache_.EndLoading(module_path);
            module_cache_.Remove(module_path);
            AVA_RETHROW();
        }
    }
    
    auto proto = module_cache_.Get(module_path);
    
    avastd::unordered_map<avastd::string, Value> outer_globals = avastd::move(globals_);
    globals_.clear();
    
    auto import_fn = outer_globals.find("__import__");
    if (import_fn != outer_globals.end()) {
        SetGlobal("__import__", import_fn->second);
    }
    
    CallFrame frame;
    frame.registers.resize(proto->num_registers);
    // Usa proto->num_registers antes de mover: `proto` no se lee de
    // nuevo despues de este punto en esta funcion, asi que la copia al
    // shared_ptr del frame se reemplaza por un move (mismo motivo que
    // ModuleCache::Add, ver module.cpp).
    frame.proto = avastd::move(proto);
    frames_.push_back(avastd::move(frame));
    
    ExecuteFrame(frames_.size() - 1);
    
    frames_.pop_back();
    
    // module_dict recien creado: ref_count arranca en 1, propiedad de
    // esta variable local (misma convencion que Value::String() en
    // value.h). Su propio destructor la libera al salir de DoImport.
    Value module_dict;
    module_dict.type = ValueType::Dict;
    module_dict.obj = new DictObj();
    
    auto* dict = static_cast<DictObj*>(module_dict.obj);
    for (auto& entry : globals_) {
        if (entry.first == "__import__") continue;
        dict->index[entry.first] = dict->entries.size();
        // emplace_back copy-construye el Value -> ya Retiene (RAII); el
        // Retain(entry.second) manual que seguia duplicaba esa retencion.
        dict->entries.emplace_back(entry.first, entry.second);
    }
    
    if (alias.empty()) {
        avastd::vector<avastd::string> parts;
        avastd::string temp = module_path;
        size_t start = 0;
        while ((start = temp.find('.')) != avastd::string::npos) {
            parts.push_back(temp.substr(0, start));
            temp = temp.substr(start + 1);
        }
        parts.push_back(temp);
        
        if (parts.size() > 1) {
            avastd::vector<avastd::string> ns_parts(parts.begin(), parts.end() - 1);
            globals_ = avastd::move(outer_globals);
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
            globals_ = avastd::move(outer_globals);
            for (auto& entry : dict->entries) {
                SetGlobal(entry.first, entry.second);
            }
            // module_dict no se guarda en ningun lado en esta rama (sus
            // entries se copiaron una por una via SetGlobal); su propia
            // referencia (ref_count=1 de la creacion, arriba) la libera
            // su destructor automatico al salir de DoImport. El
            // Release(module_dict) manual que habia aca duplicaba esa
            // liberacion (double-release).
        }
    } else {
        globals_ = avastd::move(outer_globals);
        SetGlobal(alias, module_dict);
        // Mismo caso: SetGlobal ya tomo su propia copia retenida
        // (parametro por valor); la referencia local de module_dict la
        // libera su destructor automatico, no un Release() manual.
    }
    
    return Value::Nil();
}

} // namespace ava
