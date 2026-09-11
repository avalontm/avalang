#pragma once

#include <cstddef>
#include <deque>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "languages/class_index.h"
#include "languages/function_index.h"

namespace studio {

class VariableTypeIndex {
public:
    void Rebuild(const std::string& text, const ClassIndex& class_index,
                 const FunctionIndex& function_index);

    std::string TypeOf(const std::string& variable, size_t cursor_offset) const;

    std::vector<std::string> VisibleVariables(size_t cursor_offset) const;

    std::unordered_set<std::string> AllVariableNames() const;

    void Clear() {
        scopes_.clear();
        module_scope_ = Scope{};
    }

private:
    struct Scope {
        size_t start = 0;
        size_t end = 0;
        std::unordered_map<std::string, std::string> var_types;
    };

    std::deque<Scope> scopes_;
    Scope module_scope_;

    void ScanRange(const std::string& text, size_t start, size_t end,
                   const ClassIndex& class_index, const FunctionIndex& function_index,
                   Scope& current);

    std::string LookupInScope(const Scope& current, const std::string& name) const;
};

struct MemberAccessContext {
    MemberAccessKind kind = MemberAccessKind::kInstance;
    std::string class_name;
    std::string viewer_class;
};

bool ResolveMemberAccess(const std::string& full_text, int cursor_line,
                          const std::string& text_before_cursor_on_line,
                          const ClassIndex& class_index, const VariableTypeIndex& var_types,
                          MemberAccessContext& out);

bool ResolveOwnScopeSuggestions(const std::string& full_text, int cursor_line,
                                 const std::string& text_before_cursor_on_line,
                                 const ClassIndex& class_index, const VariableTypeIndex& var_types,
                                 std::vector<std::string>& out_variable_names,
                                 std::vector<ClassMember>& out_own_members);

}
