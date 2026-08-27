#include "util/settings.h"

#include "util/data_dir.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace studio {

namespace {
namespace fs = std::filesystem;

fs::path ConfigDir() {
    fs::path dir;
#if defined(_WIN32)
    if (const char* appdata = std::getenv("APPDATA")) {
        dir = fs::path(appdata) / "AvaStudio";
    } else {
        dir = "AvaStudio";
    }
#else
    if (const char* home = std::getenv("HOME")) {
        dir = fs::path(home) / ".config" / "AvaStudio";
    } else {
        dir = "AvaStudio";
    }
#endif
    return dir;
}

fs::path SettingsPath() {
    return ConfigDir() / "settings.ini";
}

std::string Trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

}

StudioSettings LoadSettings() {

    StudioSettings settings;

    std::ifstream file(SettingsPath());
    if (!file) return settings;

    std::string line;
    while (std::getline(file, line)) {
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = Trim(line.substr(0, eq));
        std::string value = Trim(line.substr(eq + 1));
        if (key == "language") {
            settings.language = value;
        } else if (key == "modules_path") {
            settings.modules_path = value;
        } else if (key == "disabled_plugin") {

            if (!value.empty()) settings.disabled_plugins.push_back(value);
        } else if (key == "closed_panel") {

            if (!value.empty()) settings.closed_panels.push_back(value);
        } else if (key == "build_project_dir") {
            settings.build_project_dir = value;
        } else if (key == "build_entry_file") {
            settings.build_entry_file = value;
        } else if (key == "build_out_dir") {
            settings.build_out_dir = value;
        } else if (key == "build_repo_root") {
            settings.build_repo_root = value;
        } else if (key == "build_ava_cli_path") {
            settings.build_ava_cli_path = value;
        } else if (key == "build_key_file") {
            settings.build_key_file = value;
        } else if (key == "build_vcpkg_root") {
            settings.build_vcpkg_root = value;
        } else if (key == "build_target") {
            settings.build_target = value;
        } else if (key == "build_compiler_path_desktop") {
            settings.build_compiler_path_desktop = value;
        } else if (key == "build_toolchain_dir" || key == "build_compiler_path" ||
                   key == "build_compiler_path_barekernel") {

            settings.build_compiler_path_barekernel = value;
        } else if (key == "build_obfuscate") {
            settings.build_obfuscate = (value == "1");
        } else if (key == "build_obfuscate_strings") {
            settings.build_obfuscate_strings = (value == "1");
        } else if (key == "build_flatten_control_flow") {
            settings.build_flatten_control_flow = (value == "1");
        } else if (key == "build_zero_disk") {
            settings.build_zero_disk = (value == "1");
        } else if (key == "build_debug_unencrypted") {
            settings.build_debug_unencrypted = (value == "1");
        }
    }
    return settings;
}

void SaveSettings(const StudioSettings& settings) {
    std::error_code ec;
    fs::create_directories(ConfigDir(), ec);
    if (ec) return;

    std::ofstream file(SettingsPath(), std::ios::trunc);
    if (!file) return;
    file << "language=" << settings.language << "\n";
    file << "modules_path=" << settings.modules_path << "\n";
    for (const std::string& name : settings.disabled_plugins) {
        if (name.empty()) continue;
        file << "disabled_plugin=" << name << "\n";
    }
    for (const std::string& name : settings.closed_panels) {
        if (name.empty()) continue;
        file << "closed_panel=" << name << "\n";
    }
    file << "build_project_dir=" << settings.build_project_dir << "\n";
    file << "build_entry_file=" << settings.build_entry_file << "\n";
    file << "build_out_dir=" << settings.build_out_dir << "\n";
    file << "build_repo_root=" << settings.build_repo_root << "\n";
    file << "build_ava_cli_path=" << settings.build_ava_cli_path << "\n";
    file << "build_key_file=" << settings.build_key_file << "\n";
    file << "build_vcpkg_root=" << settings.build_vcpkg_root << "\n";
    file << "build_target=" << settings.build_target << "\n";
    file << "build_compiler_path_desktop=" << settings.build_compiler_path_desktop << "\n";
    file << "build_compiler_path_barekernel=" << settings.build_compiler_path_barekernel << "\n";
    file << "build_obfuscate=" << (settings.build_obfuscate ? "1" : "0") << "\n";
    file << "build_obfuscate_strings=" << (settings.build_obfuscate_strings ? "1" : "0") << "\n";
    file << "build_flatten_control_flow=" << (settings.build_flatten_control_flow ? "1" : "0") << "\n";
    file << "build_zero_disk=" << (settings.build_zero_disk ? "1" : "0") << "\n";
    file << "build_debug_unencrypted=" << (settings.build_debug_unencrypted ? "1" : "0") << "\n";
}

}
