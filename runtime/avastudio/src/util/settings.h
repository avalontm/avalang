#pragma once

#include <string>
#include <vector>

namespace studio {

struct StudioSettings {

    std::string language;

    std::string modules_path;

    std::vector<std::string> disabled_plugins;

    std::vector<std::string> closed_panels;

    std::string build_project_dir;
    std::string build_entry_file;
    std::string build_out_dir;
    std::string build_repo_root;
    std::string build_ava_cli_path;
    std::string build_key_file;
    std::string build_vcpkg_root;

    std::string build_target;

    std::string build_toolchain_dir;

    bool build_obfuscate = false;
    bool build_obfuscate_strings = false;
    bool build_flatten_control_flow = false;
    bool build_zero_disk = false;
    bool build_debug_unencrypted = false;
};

StudioSettings LoadSettings();

void SaveSettings(const StudioSettings& settings);

}
