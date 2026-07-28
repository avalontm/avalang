#include "config/app_config.h"

#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using nlohmann::json;

namespace avahost {

bool AppConfig::Load(HostOptions& options, std::string& outError) {
    fs::path path = fs::path(options.projectRoot) / "appsettings.json";
    if (!fs::exists(path)) {
        return true; // no file -- defaults stand, not an error
    }

    std::ifstream file(path);
    if (!file) {
        outError = "could not open " + path.string();
        return false;
    }

    json data;
    try {
        file >> data;
    } catch (const json::parse_error& ex) {
        outError = "malformed appsettings.json: " + std::string(ex.what());
        return false;
    }

    if (data.contains("host") && data["host"].is_string()) options.host = data["host"].get<std::string>();
    if (data.contains("port") && data["port"].is_number_integer()) options.port = data["port"].get<int>();
    if (data.contains("environment") && data["environment"].is_string()) options.environment = data["environment"].get<std::string>();
    if (data.contains("watch") && data["watch"].is_boolean()) options.watch = data["watch"].get<bool>();

    if (data.contains("routesDir") && data["routesDir"].is_string()) options.routesDir = data["routesDir"].get<std::string>();
    if (data.contains("wwwrootDir") && data["wwwrootDir"].is_string()) options.wwwrootDir = data["wwwrootDir"].get<std::string>();
    if (data.contains("layoutsDir") && data["layoutsDir"].is_string()) options.layoutsDir = data["layoutsDir"].get<std::string>();
    if (data.contains("componentsDir") && data["componentsDir"].is_string()) options.componentsDir = data["componentsDir"].get<std::string>();
    if (data.contains("pluginsDir") && data["pluginsDir"].is_string()) options.pluginsDir = data["pluginsDir"].get<std::string>();

    return true;
}

} // namespace avahost
