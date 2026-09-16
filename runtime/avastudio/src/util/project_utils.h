#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace studio {

std::string DetectEntryFile(const std::filesystem::path& project_dir);

bool DetectUsesUi(const std::filesystem::path& project_dir);

std::vector<std::filesystem::path> ListSearchableFiles(const std::filesystem::path& project_dir);

std::string ToProjectRelativePath(const std::string& project_dir, const std::string& absolute_path);

}