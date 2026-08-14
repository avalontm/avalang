#include "util/settings.h"

#include "util/data_dir.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace studio {

namespace {
namespace fs = std::filesystem;

// Mirrors the config_dir resolution in main.cpp's ini_path setup (kept
// separate/duplicated on purpose -- this is a small, self-contained file
// and main.cpp's lambda is tied to ImGui's io.IniFilename lifetime, not
// worth threading a shared helper through for ~10 lines).
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

// Trim helper: '\r' shows up if the file was ever edited/saved on
// Windows with CRLF, and stray whitespace around '=' is harmless to
// tolerate for a hand-editable file.
std::string Trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

} // namespace

StudioSettings LoadSettings() {
    // Blank on a first run (no settings file yet) -- see the comment on
    // StudioSettings::modules_path for what blank means at the point of
    // use. Deliberately NOT util::ResolveDefaultModulesDir() here: that
    // would bake today's exe location into settings.ini as soon as it's
    // saved, defeating the portability blank is meant to give.
    StudioSettings settings;

    std::ifstream file(SettingsPath());
    if (!file) return settings; // no file yet -- first run, keep the default

    std::string line;
    while (std::getline(file, line)) {
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = Trim(line.substr(0, eq));
        std::string value = Trim(line.substr(eq + 1));
        if (key == "modules_path") {
            settings.modules_path = value;
        } else if (key == "disabled_plugin") {
            // One "disabled_plugin=<file_name>" line per disabled
            // plugin, rather than a single comma-joined value -- a
            // file name can't contain '\n', so this never needs
            // escaping, unlike a comma-separated list would if a
            // plugin file name ever had a comma in it.
            if (!value.empty()) settings.disabled_plugins.push_back(value);
        } else if (key == "closed_panel") {
            // One "closed_panel=<panel name>" line per closed plugin
            // panel -- see StudioSettings::closed_panels.
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
    file << "build_obfuscate=" << (settings.build_obfuscate ? "1" : "0") << "\n";
    file << "build_obfuscate_strings=" << (settings.build_obfuscate_strings ? "1" : "0") << "\n";
    file << "build_flatten_control_flow=" << (settings.build_flatten_control_flow ? "1" : "0") << "\n";
    file << "build_zero_disk=" << (settings.build_zero_disk ? "1" : "0") << "\n";
    file << "build_debug_unencrypted=" << (settings.build_debug_unencrypted ? "1" : "0") << "\n";
}

} // namespace studio
