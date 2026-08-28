#pragma once

#include <cstddef>
#include <deque>
#include <string>
#include <unordered_map>

#include "languages/class_index.h"
#include "languages/function_index.h"

namespace studio {

// Tracks the inferred/declared type of local variables, scoped per function
// body (top-level funcs and class methods) instead of a single flat map for
// the whole file. `cursor_offset` (a byte offset into the text passed to
// Rebuild) selects which function's scope applies; variables assigned
// outside any function body fall into the module-level scope.
//
// Resolves, in addition to the original `var = KnownClass(...)` pattern:
//   - `x as Tipo` / `x as Tipo = expr` (declared type wins over inference)
//   - `var = funcion(...)` via FunctionIndex::EffectiveReturnType()
//   - `var = obj.metodo(...)` (one dot level only -- chained access
//     `a.b.c` stays out of scope, same documented limit as before)
class VariableTypeIndex {
public:
    void Rebuild(const std::string& text, const ClassIndex& class_index,
                 const FunctionIndex& function_index);

    std::string TypeOf(const std::string& variable, size_t cursor_offset) const;

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

    // std::deque so pointers/references into already-pushed scopes stay
    // valid while we recurse into nested function bodies and keep pushing
    // more scopes (a std::vector could reallocate and dangle them).
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

}
