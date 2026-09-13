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
        } else if (key == "last_project_dir") {
            settings.last_project_dir = value;
        } else if (key == "show_minimap") {
            settings.show_minimap = (value != "0");
        } else if (key == "show_inlay_hints") {
            settings.show_inlay_hints = (value != "0");
        } else if (key == "format_on_save") {
            settings.format_on_save = (value != "0");
        } else if (key == "format_on_type") {
            settings.format_on_type = (value != "0");
        } else if (key == "disabled_plugin") {
            if (!value.empty()) settings.disabled_plugins.push_back(value);
        } else if (key == "closed_panel") {
            if (!value.empty()) settings.closed_panels.push_back(value);
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
    file << "last_project_dir=" << settings.last_project_dir << "\n";
    file << "show_minimap=" << (settings.show_minimap ? "1" : "0") << "\n";
    file << "show_inlay_hints=" << (settings.show_inlay_hints ? "1" : "0") << "\n";
    file << "format_on_save=" << (settings.format_on_save ? "1" : "0") << "\n";
    file << "format_on_type=" << (settings.format_on_type ? "1" : "0") << "\n";
    for (const std::string& name : settings.disabled_plugins) {
        if (name.empty()) continue;
        file << "disabled_plugin=" << name << "\n";
    }
    for (const std::string& name : settings.closed_panels) {
        if (name.empty()) continue;
        file << "closed_panel=" << name << "\n";
    }
}

}
