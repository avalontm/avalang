#pragma once

#include <string>
#include <unordered_map>

#include "languages/class_index.h"

namespace studio {

class VariableTypeIndex {
public:
    void Rebuild(const std::string& text, const ClassIndex& class_index);

    std::string TypeOf(const std::string& variable) const;

    void Clear() { variable_types_.clear(); }

private:
    std::unordered_map<std::string, std::string> variable_types_;
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
