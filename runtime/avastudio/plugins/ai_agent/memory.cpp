#include "memory.h"

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {

const size_t kMaxHistoryEntries = 200;

fs::path MemoryDir() {
    fs::path dir;
#if defined(_WIN32)
    if (const char* appdata = std::getenv("APPDATA")) {
        dir = fs::path(appdata) / "AvaStudio" / "plugins" / "ai_agent" / "memory";
    } else {
        dir = fs::path("AvaStudio") / "plugins" / "ai_agent" / "memory";
    }
#else
    if (const char* home = std::getenv("HOME")) {
        dir = fs::path(home) / ".config" / "AvaStudio" / "plugins" / "ai_agent" / "memory";
    } else {
        dir = fs::path("AvaStudio") / "plugins" / "ai_agent" / "memory";
    }
#endif
    return dir;
}

// El project_root se usa como nombre de archivo -- se hashea en vez de
// sanitizar la ruta a mano (evita lidiar con ':', '/', '\' distintos
// entre plataformas para el mismo path).
std::string ProjectFileStem(const std::string& project_root) {
    if (project_root.empty()) return "default";
    size_t h = std::hash<std::string>{}(project_root);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%016zx", h);
    return buf;
}

fs::path MemoryPath(const std::string& project_root) {
    return MemoryDir() / (ProjectFileStem(project_root) + ".json");
}

} // namespace

AgentMemory LoadAgentMemory(const std::string& project_root) {
    AgentMemory memory;
    std::ifstream file(MemoryPath(project_root));
    if (!file) return memory;

    try {
        json parsed;
        file >> parsed;
        memory.accumulated_cost_usd = parsed.value("accumulated_cost_usd", 0.0);
        for (const auto& entry : parsed.value("history", json::array())) {
            AgentMemoryEntry e;
            e.role = entry.value("role", "");
            e.content = entry.value("content", "");
            if (!e.role.empty()) memory.history.push_back(std::move(e));
        }
        if (memory.history.size() > kMaxHistoryEntries) {
            memory.history.erase(memory.history.begin(),
                                  memory.history.end() - static_cast<long>(kMaxHistoryEntries));
        }
    } catch (...) {
    }
    return memory;
}

void SaveAgentMemory(const std::string& project_root, const AgentMemory& memory) {
    std::error_code ec;
    fs::create_directories(MemoryDir(), ec);
    if (ec) return;

    json j;
    j["accumulated_cost_usd"] = memory.accumulated_cost_usd;
    j["history"] = json::array();
    size_t start = memory.history.size() > kMaxHistoryEntries ? memory.history.size() - kMaxHistoryEntries : 0;
    for (size_t i = start; i < memory.history.size(); ++i) {
        j["history"].push_back({{"role", memory.history[i].role}, {"content", memory.history[i].content}});
    }

    std::ofstream file(MemoryPath(project_root), std::ios::trunc);
    if (!file) return;
    file << j.dump(2);
}

void ClearAgentMemory(const std::string& project_root) {
    std::error_code ec;
    fs::remove(MemoryPath(project_root), ec);
}
