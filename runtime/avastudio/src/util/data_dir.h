#pragma once

#include <string>

namespace studio::util {

std::string ResolveDataDir();

std::string ResolveDefaultModulesDir();

bool ReadFileToString(const std::string& path, std::string& out);

}
