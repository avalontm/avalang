#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace studio {

// Una declaración `func nombre(params)` encontrada en el buffer actual o en
// un archivo alcanzable vía `import`.
struct FunctionSignature {
    std::string name;
    std::vector<std::string> params;   // texto crudo de cada param: "a", "b=1", "*rest"
    std::string source_file;           // "" = buffer actual/sin guardar
    std::string display;               // "nombre(a, b=1, *rest)" precalculado para UI

    int min_args = 0;      // params sin default y sin *rest
    bool has_var_args = false;

    // Descripción mostrada en el parameter hint. Para builtins viene de
    // BuiltinSignatures() (ver languages/builtin_signatures.h). Para
    // funciones de usuario, FunctionIndex::ScanText la llena a partir de
    // un bloque de comentarios "##" (doble numeral) escrito inmediatamente
    // arriba del `func` -- convención puramente del lado del editor, no
    // requiere cambios en el parser/VM de AvaLang. Vacío si la función no
    // tiene ese bloque encima.
    std::string doc;
    // Descripción por parámetro, extraída de líneas "## @param nombre: ..."
    // dentro del mismo bloque "##" (ver ScanText). Clave = nombre del
    // parámetro tal como aparece en `params` (sin '*' ni '=default').
    // Vacío si el bloque no usa @param, o para builtins (que no lo
    // necesitan -- su `doc` ya cubre todos los argumentos en una línea).
    std::unordered_map<std::string, std::string> param_docs;
    // true si esta entrada vino de la tabla de builtins en vez de un
    // `func` real del usuario/import.
    bool is_builtin = false;
    // Todos los builtins de AvaLang (print, len, ...) se registran como
    // globals normales (VM::RegisterNative -> SetGlobal, ver vm.cpp), en
    // la MISMA tabla que un `func` de nivel de módulo -- así que declarar
    // tu propio `func print(...)` en el script simplemente pisa el
    // builtin. true para todo lo que sale de BuiltinSignatures().
    bool overridable = false;
};

// Nombre "limpio" de un parámetro crudo (ver FunctionSignature::params):
// sin '*' de var-args ni '=default'. "name" -> "name", "*rest" -> "rest",
// "b=1" -> "b". Usado para matchear contra las claves de param_docs.
std::string ParamBaseName(const std::string& raw_param);

// Índice, del lado del editor, de declaraciones de función en AvaLang.
// NO es el AST del compilador -- es un escaneo de texto best-effort,
// suficiente para autocompletado/parameter hints. No valida sintaxis y
// simplemente ignora lo que no puede parsear con confianza.
class FunctionIndex {
public:
    // Re-escanea `text` (el buffer abierto en el editor) y, por cada
    // `import a.b.c [as alias]` que encuentre, intenta resolver y escanear
    // también ese archivo en disco (ver ResolveImportPath). Los imports se
    // siguen UN nivel -- se indexan los símbolos que ese módulo define, no
    // los módulos que ese módulo a su vez importa.
    void Rebuild(const std::string& text, const std::string& current_file_dir);

    const std::unordered_map<std::string, FunctionSignature>& Signatures() const {
        return signatures_;
    }

    const FunctionSignature* Find(const std::string& name) const {
        auto it = signatures_.find(name);
        return it == signatures_.end() ? nullptr : &it->second;
    }

private:
    std::unordered_map<std::string, FunctionSignature> signatures_;

    // Parsea cada `func NOMBRE(...)` en `text` (a nivel de módulo o dentro
    // de una clase) y lo guarda en signatures_ con `source_file`. Si el
    // nombre ya existe, NO se sobreescribe -- Rebuild() siempre escanea el
    // buffer local primero, así que "local gana" es automático.
    void ScanText(const std::string& text, const std::string& source_file);

    // Encuentra cada `import a.b.c [as x]` en `text` y, para cada uno,
    // intenta resolver+leer+escanear el .ava correspondiente.
    void ScanImports(const std::string& text, const std::string& current_file_dir,
                      std::unordered_set<std::string>& visited);

    // module_path = {"a","b","c"} -> ruta de archivo, best-effort. Espeja
    // ModuleResolver::ResolveModulePath (core/src/vm/module.cpp), que es
    // lo que el runtime (__import__, ver builtin_natives.cpp) usa de
    // verdad:
    //   1. <current_file_dir>/a/b/c.ava
    //   2. <current_file_dir>/a/b/c/index.ava (módulos-carpeta)
    // No sigue los demás search_paths_ del resolver (stdlib, etc.) --
    // solo resuelve relativo al archivo abierto, best-effort para el editor.
    static std::string ResolveImportPath(const std::vector<std::string>& module_path,
                                          const std::string& current_file_dir);
};

} // namespace studio
