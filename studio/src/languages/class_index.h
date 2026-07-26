#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "languages/function_index.h"

namespace studio {

// A method declared directly in a class body, together with the
// `static`/`private` modifiers scanned for it (see DISENO_visibilidad_clases_avalang.md
// §4/§5 and Fase E of TODO_autocompletado_miembros.md). Wraps
// FunctionSignature instead of adding these flags to it directly, since
// FunctionSignature is shared with plain module-level functions
// (FunctionIndex) which have no such concept.
struct ClassMethodInfo {
    FunctionSignature signature;
    bool is_static = false;   // `static func NAME(...)` -- shared, no `this`, called as `Clase.NAME(...)`
    bool is_private = false;  // `private func NAME(...)` -- not visible from outside the class
};

// An attribute declared directly in a class body -- either as a bare
// `NAME = expr` / `static NAME = expr` / `private NAME = expr` class-body
// statement (the `attrDeclaration` from the design doc), or inferred from a
// `this.NAME = ...` assignment inside one of the class's own methods
// (AvaLang only has `this` -- there is no `self`). Best-effort, editor-side
// only: this mirrors the compiler's `is_static`/`is_private` AST flags
// (`core/src/ast/ast.h`), not a runtime guarantee.
struct ClassAttributeInfo {
    bool is_static = false;   // static -> shared, lives only on the class object (`Clase.NAME`)
    bool is_private = false;  // private -> not visible from outside the class
};

// A `class NAME [: base] ... end` block found in the current buffer or in a
// file reachable via `import` (see ClassIndex::Rebuild -- unlike
// FunctionIndex::ScanImports, imports here are followed TRANSITIVELY).
struct ClassInfo {
    std::string name;
    std::string base_class_name;  // "" if no `classHeritage` (`class Dog: Animal` -> "Animal")
    std::string source_file;      // "" = current buffer/unsaved

    // Methods declared directly in this class's body (does NOT include
    // inherited ones -- see ClassIndex::FlattenedMembers for that).
    std::unordered_map<std::string, ClassMethodInfo> methods;

    // Attributes declared or inferred directly in this class's body (does
    // NOT include inherited ones -- see ClassIndex::FlattenedMembers).
    std::unordered_map<std::string, ClassAttributeInfo> attributes;
};

// One member surfaced to the autocomplete popup -- either a method (has a
// non-null `signature`) or an attribute (name only).
struct ClassMember {
    std::string name;
    bool is_method = false;
    bool is_static = false;
    bool is_private = false;
    const FunctionSignature* signature = nullptr;  // null for attributes
    std::string declared_in;  // class where the member actually lives (may be an ancestor)
};

// Which side of a `.` the popup is being asked to fill in for -- drives the
// three filtering rules from DISENO_visibilidad_clases_avalang.md §5 / §9.
enum class MemberAccessKind {
    kInstance,   // `variable.` where `variable` is an inferred instance, from outside the class
    kThis,       // `this.` written inside a method of `viewer_class`
    kClassName,  // `NombreDeClase.` -- direct access to the class object
};

// Editor-side, best-effort index of `class` declarations in AvaLang. Mirrors
// FunctionIndex in spirit: NOT the compiler's AST, just a text scan good
// enough to drive autocomplete and member lookup ("dog." -> members of
// class dog). Does not validate syntax and silently ignores anything it
// can't parse with confidence.
class ClassIndex {
public:
    // Re-scans `text` (the buffer currently open in the editor) and follows
    // every `import a.b.c [as alias]` it finds, TRANSITIVELY -- if the
    // current file imports `a` and `a` imports `b`, classes defined in `b`
    // are indexed too, same as a real IDE's project-wide symbol search (not
    // just one level, unlike FunctionIndex::ScanImports). Import cycles are
    // guarded against via a visited-files set.
    void Rebuild(const std::string& text, const std::string& current_file_dir);

    const std::unordered_map<std::string, ClassInfo>& Classes() const { return classes_; }

    const ClassInfo* Find(const std::string& class_name) const {
        auto it = classes_.find(class_name);
        return it == classes_.end() ? nullptr : &it->second;
    }

    // All members of `class_name`, INCLUDING everything inherited through
    // its base-class chain. A member declared locally wins over one with
    // the same name inherited from a base class (override shadows parent),
    // matching normal OOP lookup order. Returns an empty vector if
    // `class_name` isn't in the index, or if a base-class name in the chain
    // can't be resolved (the chain simply stops there -- no error).
    std::vector<ClassMember> FlattenedMembers(const std::string& class_name) const;

    // Applies the three popup-filtering rules from
    // DISENO_visibilidad_clases_avalang.md §5/§9 to a member list already
    // produced by FlattenedMembers. Does not itself figure out `kind` or
    // `viewer_class` -- that's the editor-context-detection job tracked as
    // Fase 3 of TODO_autocompletado_miembros.md; this is just the filter.
    //
    //   kInstance   -> keep members with is_private == false (static or not).
    //   kThis       -> keep members that are not private, OR that ARE
    //                  private but declared_in == viewer_class (a class's
    //                  own private members are visible from its own
    //                  methods; a private member inherited from a base
    //                  class is not -- see §3.3/§9's table).
    //   kClassName  -> keep only is_static == true members. Among those,
    //                  a private static is only kept if declared_in ==
    //                  viewer_class (e.g. `Contador.validarLimite` is only
    //                  offered while completing from inside Contador
    //                  itself); public statics are always kept.
    //
    // `viewer_class` is the class whose method body the cursor is currently
    // inside, or "" if it isn't inside any method (e.g. top-level script
    // code) -- in that case every private member is excluded, since there
    // is no class to "be inside of".
    static std::vector<ClassMember> FilterForAccess(const std::vector<ClassMember>& members,
                                                     MemberAccessKind kind,
                                                     const std::string& viewer_class);

private:
    std::unordered_map<std::string, ClassInfo> classes_;

    // Parses every `class NAME [: base] ... end` block in `text` and stores
    // it in classes_ with `source_file`. If the name already exists, does
    // NOT overwrite -- like FunctionIndex, Rebuild() always scans the local
    // buffer first, so "local wins" is automatic.
    void ScanText(const std::string& text, const std::string& source_file);

    // Finds every `import a.b.c [as x]` in `text` and, for each one,
    // resolves + reads + scans the corresponding .ava file -- then recurses
    // into THAT file's own imports too (see Rebuild's comment on transitive
    // resolution).
    void ScanImports(const std::string& text, const std::string& current_file_dir,
                      std::unordered_set<std::string>& visited);

    // Mirrors FunctionIndex::ResolveImportPath (same module search rule:
    // <dir>/a/b/c.ava, then <dir>/a/b/c/index.ava).
    static std::string ResolveImportPath(const std::vector<std::string>& module_path,
                                          const std::string& current_file_dir);
};

} // namespace studio
