#pragma once

#include <string>

struct AgentConfig {
    std::string api_key;
    std::string last_model;
};

AgentConfig LoadAgentConfig();
void SaveAgentConfig(const AgentConfig& config);
