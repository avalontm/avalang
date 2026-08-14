#pragma once

#include <string>

#include "avalang.h"
#include "design/design_document.h"

namespace studio::design {













































AvaVM* BuildStateVM(const DesignDocument& doc);

std::string EvalPropertyExpr(AvaVM* vm, const std::string& raw_value);

std::string GetDisplayPropertyKey(const std::string& node_type);

void BindCodeBehind(AvaVM* vm, const DesignDocument& doc);

bool InvokeHandler(AvaVM* vm, const std::string& handler_name, std::string* out_error = nullptr);

}