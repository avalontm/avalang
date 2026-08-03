#pragma once

#include <string>
#include <vector>

struct AgentMemoryEntry {
    std::string role;    // "user" | "assistant"
    std::string content;
};

struct AgentMemory {
    std::vector<AgentMemoryEntry> history;
    double accumulated_cost_usd = 0.0; // Fase 7.2
};

AgentMemory LoadAgentMemory(const std::string& project_root);
void SaveAgentMemory(const std::string& project_root, const AgentMemory& memory);
void ClearAgentMemory(const std::string& project_root);
