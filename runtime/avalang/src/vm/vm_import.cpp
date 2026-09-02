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
    // Bug nuevo (encontrado en esta pasada -- ver la fila correspondiente
    // en AvaLang_Bugs_Encontrados.md): esta función SIEMPRE ignoraba el
    // nivel real de anidamiento y operaba sobre el mismo `globals` de
    // nivel superior en cada llamada recursiva, tanto para crear el
    // namespace intermedio como para el caso base (guardar
    // `module_dict`) -- nunca descendía de verdad al `DictObj` recién
    // creado/reusado para un segmento intermedio. Para `import pkg.sub`
    // (2 segmentos) eso significaba: el namespace `pkg` se creaba bien
    // como `globals["pkg"] = {}` (Dict vacío), pero el módulo `sub` en
    // sí terminaba guardado en un SEGUNDO global de nivel superior,
    // `globals["sub"]`, nunca dentro de `pkg`. Reescrita: ahora resuelve
    // el primer segmento contra `globals` (nivel superior, único
    // contenedor que no es un `DictObj`), y desde ahí camina/crea cada
    // segmento intermedio como un `DictObj` anidado real, guardando
    // `module_dict` en el último segmento del DictObj correspondiente
    // (el namespace inmediato que lo contiene) -- no en `globals` de
    // nuevo. `part_idx` se quita del contrato público (siempre arrancaba
    // en 0 de todas formas, ver único call-site en PlaceModuleInScope);
    // internamente sigue existiendo como índice de loop.
    Value module_dict) {
    // parts.size() > 1 siempre (ver el único call-site, PlaceModuleInScope,
    // que ya separa el caso de un solo segmento antes de llamar acá).

    // Paso 1: resolver/crear el primer segmento contra `globals` --
    // el único nivel que vive en el unordered_map plano de la VM en vez
    // de en un DictObj anidado.
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

    // Paso 2: caminar cada segmento intermedio (si hay 3+ segmentos,
    // ej. "a.b.c") como un DictObj anidado real dentro del anterior --
    // creando o reusando cada uno.
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

    // Paso 3: guardar `module_dict` en el último segmento, DENTRO del
    // DictObj que lo contiene de verdad (no en `globals` de nuevo).
    const avastd::string& leaf = parts.back();
    auto leaf_it = current->index.find(leaf);
    if (leaf_it != current->index.end()) {
        current->entries[leaf_it->second].second = module_dict;
    } else {
        current->index[leaf] = current->entries.size();
        current->entries.emplace_back(leaf, module_dict);
    }
}

// Phase 1 of AVALANG_IMPORT_SYSTEM_PLAN.md. Places an already-built
// module Value (either the Dict assembled from a compiled module's
// globals_, or one returned by a native module factory) into scope,
// following the exact same three rules DoImport always used: an
// explicit `as alias` sets that one name; a dotted module_path builds
// (or extends) a nested namespace via SetNestedNamespace; a single
// bare segment dumps the module's own entries straight into scope
// (the "from X import *" behavior imports already relied on). Factored
// out so a native module -- which skips file resolution/compilation
// entirely -- lands in scope through the identical rules a file-backed
// import uses, instead of a second, divergent copy of this logic.
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

// Ver declaracion en vm.h. `symbol` se busca solo entre las entradas de
// nivel superior de cada Dict construido por una factory registrada --
// alcanza para el caso real que motiva esto ("Console" vive en el nivel
// superior de "system", ver builtins/system_module.cpp) sin necesitar
// bajar recursivamente por sub-namespaces.
avastd::string VM::FindNativeModuleExporting(const avastd::string& symbol) const {
    for (auto& entry : native_modules_) {
        // const_cast: la factory pide VM& (para poder registrar
        // builtins/leer VmPlatformAccessor si hiciera falta), pero este
        // metodo es const desde afuera -- no muta ningun estado del VM
        // propiamente dicho, solo construye un Dict transitorio que se
        // descarta al salir del scope (ver comentario en vm.h).
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
    // Phase 1 of AVALANG_IMPORT_SYSTEM_PLAN.md: a registered native
    // module short-circuits everything below -- no ModuleResolver
    // lookup, no IFileSystem read, no CompileSource/ExecuteFrame. The
    // factory builds the module Dict directly in C++ (see
    // builtins/system_module.cpp) and it's placed in scope the same
    // way a file-backed import's Dict would be.
    auto native_it = native_modules_.find(module_path);
    if (native_it != native_modules_.end()) {
        Value module_dict = native_it->second(*this);
        PlaceModuleInScope(*this, module_path, alias, module_dict);
        return Value::Nil();
    }

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
    
    CloseUpvalues(frames_.back());
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
    
    // Placement rules (alias / dotted namespace / single-segment dump
    // straight into scope -- see PlaceModuleInScope's own comment above
    // for the reasoning behind each, in particular why a bare `import
    // Dog` must dump Dog's own entries into scope instead of nesting
    // them under a "Dog" dict: a Dict isn't callable, so `Dog()` would
    // otherwise fail with "attempt to call a non-callable value").
    globals_ = avastd::move(outer_globals);
    PlaceModuleInScope(*this, module_path, alias, module_dict);
    
    return Value::Nil();
}

} // namespace ava
