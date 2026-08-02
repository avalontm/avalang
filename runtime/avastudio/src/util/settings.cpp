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
}

} // namespace studio
