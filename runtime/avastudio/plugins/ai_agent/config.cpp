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

std::string ProviderToString(AgentProvider provider) {
    return provider == AgentProvider::Custom ? "custom" : "openrouter";
}

AgentProvider ProviderFromString(const std::string& s) {
    return s == "custom" ? AgentProvider::Custom : AgentProvider::OpenRouter;
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
        config.provider = ProviderFromString(parsed.value("provider", "openrouter"));
        config.custom_base_url = parsed.value("custom_base_url", "");
        config.custom_api_key = parsed.value("custom_api_key", "");
        config.custom_model = parsed.value("custom_model", "");
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
    j["provider"] = ProviderToString(config.provider);
    j["custom_base_url"] = config.custom_base_url;
    j["custom_api_key"] = config.custom_api_key;
    j["custom_model"] = config.custom_model;

    std::ofstream file(ConfigPath(), std::ios::trunc);
    if (!file) return;
    file << j.dump(2);
}
