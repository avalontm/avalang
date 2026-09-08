#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "languages/import_file_cache.h"

namespace studio {

struct FunctionSignature {
    std::string name;
    std::vector<std::string> params;
    std::string source_file;
    std::string display;

    // 0-based line of the `func name(...)` declaration in source_file (or in
    // the current buffer when source_file is empty). 0 for builtins/unknown
    // -- callers that care (Go to Definition) also check is_builtin/is_empty
    // source_file first, same convention already used for those.
    int line = 0;

    int min_args = 0;
    bool has_var_args = false;

    std::string doc;

    std::unordered_map<std::string, std::string> param_docs;

    bool is_builtin = false;

    bool overridable = false;

    std::string declared_return_type;
    std::string inferred_return_type;

    std::string EffectiveReturnType() const {
        return !declared_return_type.empty() ? declared_return_type : inferred_return_type;
    }
};

std::string ParamBaseName(const std::string& raw_param);
std::string ParamBaseType(const std::string& raw_param);

class FunctionIndex {
public:

    void Rebuild(const std::string& text, const std::string& current_file_dir,
                 ImportFileCache* shared_cache = nullptr);

    const std::unordered_map<std::string, FunctionSignature>& Signatures() const {
        return signatures_;
    }

    const FunctionSignature* Find(const std::string& name) const {
        auto it = signatures_.find(name);
        return it == signatures_.end() ? nullptr : &it->second;
    }

private:
    std::unordered_map<std::string, FunctionSignature> signatures_;

    void ScanText(const std::string& text, const std::string& source_file);

    void ScanImports(const std::string& text, const std::string& current_file_dir,
                      std::unordered_set<std::string>& visited, ImportFileCache& cache);

    static std::string ResolveImportPath(const std::vector<std::string>& module_path,
                                          const std::string& current_file_dir);
};

}
