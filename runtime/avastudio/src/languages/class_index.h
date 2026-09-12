#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "languages/function_index.h"
#include "languages/import_file_cache.h"

namespace studio {

struct ClassMethodInfo {
    FunctionSignature signature;
    bool is_static = false;
    bool is_private = false;

    // True for a bodyless `interfaceMethodSignature` (`func Area() as float`
    // with no `end` of its own, only valid inside `interface ... end`). See
    // the `signature_only` heuristic in class_index.cpp. A concrete class
    // method (or an interface's own default method, which does have a body)
    // is always false here.
    bool is_abstract = false;
};

struct ClassAttributeInfo {
    bool is_static = false;
    bool is_private = false;
    std::string declared_type;

    int line = 0;
};

struct ClassInfo {
    std::string name;
    std::string base_class_name;
    std::string source_file;

    // True for `interface Name ... end`, false for `class Name ... end`.
    bool is_interface = false;

    // Full comma-separated heritage list as written (`class C : Base, IA, IB`
    // or `interface IB : IA, IC`), in source order. base_class_name is kept
    // as the first entry for callers that only care about a single base, but
    // FlattenedMembers walks all of these so implemented/extended interfaces
    // contribute their (default) methods too.
    std::vector<std::string> heritage_names;

    int line = 0;

    std::unordered_map<std::string, ClassMethodInfo> methods;

    std::unordered_map<std::string, ClassAttributeInfo> attributes;
};

struct ClassMember {
    std::string name;
    bool is_method = false;
    bool is_static = false;
    bool is_private = false;
    const FunctionSignature* signature = nullptr;
    std::string declared_in;
    std::string declared_type;

    // Mirrors ClassMethodInfo::is_abstract: true when this member, as seen
    // from `class_name` in FlattenedMembers, still resolves to an interface
    // signature with no implementation anywhere in the heritage graph.
    bool is_abstract = false;

    int line = 0;
};

enum class MemberAccessKind {
    kInstance,
    kThis,
    kClassName,
};

class ClassIndex {
public:

    void Rebuild(const std::string& text, const std::string& current_file_dir,
                 ImportFileCache* shared_cache = nullptr, const std::string& stdlib_dir = "");

    void ScanFile(const std::string& text, const std::string& source_file) { ScanText(text, source_file); }

    const std::unordered_map<std::string, ClassInfo>& Classes() const { return classes_; }

    const ClassInfo* Find(const std::string& class_name) const {
        auto it = classes_.find(class_name);
        return it == classes_.end() ? nullptr : &it->second;
    }

    std::vector<ClassMember> FlattenedMembers(const std::string& class_name,
                                               const ClassIndex* fallback = nullptr) const;

    // Subset of FlattenedMembers(class_name) that are still unimplemented
    // interface method signatures (ClassMember::is_abstract) -- i.e. what a
    // "class Foo : ISomething" would need to add for Foo to actually satisfy
    // every interface it declares. Empty for interfaces themselves (an
    // interface is allowed to leave signatures unimplemented) and for
    // classes that already implement everything.
    std::vector<ClassMember> MissingInterfaceMembers(const std::string& class_name) const;

    static std::vector<ClassMember> FilterForAccess(const std::vector<ClassMember>& members,
                                                     MemberAccessKind kind,
                                                     const std::string& viewer_class);

private:
    std::unordered_map<std::string, ClassInfo> classes_;

    void ScanText(const std::string& text, const std::string& source_file);

    void ScanImports(const std::string& text, const std::string& current_file_dir,
                      std::unordered_set<std::string>& visited, ImportFileCache& cache,
                      const std::string& stdlib_dir);

    static std::string ResolveImportPath(const std::vector<std::string>& module_path,
                                          const std::string& current_file_dir,
                                          const std::string& stdlib_dir);
};

}
