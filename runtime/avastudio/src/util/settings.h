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

    // Two separate paths -- one per target -- so switching the Target combo
    // automatically uses the right one instead of overwriting a single
    // shared field. ava_cli build itself still only takes one
    // --compiler-path per invocation (see build_command.cpp); the panel
    // picks which of these two to send based on build_target.
    std::string build_compiler_path_desktop;
    std::string build_compiler_path_barekernel;

    bool build_obfuscate = false;
    bool build_obfuscate_strings = false;
    bool build_flatten_control_flow = false;
    bool build_zero_disk = false;
    bool build_debug_unencrypted = false;

    // --target barekernel only (see BAREKERNEL_PLATFORM_LAYER.md): force a
    // rebuild of the cached generic pieces (avalang/libavalang.so and the
    // avapack_barekernel_runtime .a) instead of reusing what's already
    // sitting next to --out / in build_pack_barekernel/. Persisted like the
    // other checkboxes above, so remember to turn these back off after the
    // build that needed them -- left on, every subsequent build force-
    // rebuilds those pieces again and loses the caching speedup.
    bool build_force_so = false;
    bool build_force_runtime = false;
};

StudioSettings LoadSettings();

void SaveSettings(const StudioSettings& settings);

}
