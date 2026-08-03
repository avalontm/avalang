#include "config.h"

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {

fs::path ConfigDir() {
    fs::path dir;
#if defined(_WIN32)
    if (const char* appdata = std::getenv("APPDATA")) {
        dir = fs::path(appdata) / "AvaStudio" / "plugins" / "ai_agent";
    } else {
        dir = fs::path("AvaStudio") / "plugins" / "ai_agent";
    }
#else
    if (const char* home = std::getenv("HOME")) {
        dir = fs::path(home) / ".config" / "AvaStudio" / "plugins" / "ai_agent";
    } else {
        dir = fs::path("AvaStudio") / "plugins" / "ai_agent";
    }
#endif
    return dir;
}

fs::path ConfigPath() {
    return ConfigDir() / "config.json";
}

} // namespace

AgentConfig LoadAgentConfig() {
    AgentConfig config;
    std::ifstream file(ConfigPath());
    if (!file) return config;

    try {
        json parsed;
        file >> parsed;
        config.api_key = parsed.value("api_key", "");
        config.last_model = parsed.value("last_model", "");
    } catch (...) {
    }
    return config;
}

void SaveAgentConfig(const AgentConfig& config) {
    std::error_code ec;
    fs::create_directories(ConfigDir(), ec);
    if (ec) return;

    json j;
    j["api_key"] = config.api_key;
    j["last_model"] = config.last_model;

    std::ofstream file(ConfigPath(), std::ios::trunc);
    if (!file) return;
    file << j.dump(2);
}
