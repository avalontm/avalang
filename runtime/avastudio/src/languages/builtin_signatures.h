#pragma once

#include <string>
#include <unordered_map>

#include "languages/function_index.h"

namespace studio {

const std::unordered_map<std::string, FunctionSignature>& BuiltinSignatures();

const std::unordered_map<std::string, FunctionSignature>& DefaultBuiltinSignatures();

}
