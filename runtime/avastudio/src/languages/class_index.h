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
};

struct ClassAttributeInfo {
    bool is_static = false;
    bool is_private = false;
    std::string declared_type;

    // 0-based line of the first declaration/assignment seen for this
    // attribute (class-body declaration wins over a later `this.x = ...`
    // in a constructor, since RecordAttribute only sets this once). Used by
    // "Go to Definition" -- see ClassMember::line below.
    int line = 0;
};

struct ClassInfo {
    std::string name;
    std::string base_class_name;
    std::string source_file;

    // 0-based line of the `class Name` declaration in source_file (or the
    // current buffer when source_file is empty).
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

    // 0-based declaration line, copied from ClassMethodInfo::signature.line
    // or ClassAttributeInfo::line by FlattenedMembers -- lets editor_panel's
    // "Go to Definition" jump straight to this member without a second
    // lookup into ClassIndex.
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
                 ImportFileCache* shared_cache = nullptr);

    const std::unordered_map<std::string, ClassInfo>& Classes() const { return classes_; }

    const ClassInfo* Find(const std::string& class_name) const {
        auto it = classes_.find(class_name);
        return it == classes_.end() ? nullptr : &it->second;
    }

    std::vector<ClassMember> FlattenedMembers(const std::string& class_name) const;

    static std::vector<ClassMember> FilterForAccess(const std::vector<ClassMember>& members,
                                                     MemberAccessKind kind,
                                                     const std::string& viewer_class);

private:
    std::unordered_map<std::string, ClassInfo> classes_;

    void ScanText(const std::string& text, const std::string& source_file);

    void ScanImports(const std::string& text, const std::string& current_file_dir,
                      std::unordered_set<std::string>& visited, ImportFileCache& cache);

    static std::string ResolveImportPath(const std::vector<std::string>& module_path,
                                          const std::string& current_file_dir);
};

}
