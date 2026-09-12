#pragma once

#include <optional>
#include <string>

namespace studio {

struct AvaProjUserFile {
    std::string repo_root;
    std::string ava_cli_path;
    std::string key_file;
    std::string vcpkg_root;
    std::string compiler_path_desktop;
    std::string compiler_path_barekernel;
    bool force_so = false;
    bool force_runtime = false;
};

std::optional<AvaProjUserFile> LoadAvaProjUserFile(const std::string& path);

bool SaveAvaProjUserFile(const std::string& path, const AvaProjUserFile& data);

}
