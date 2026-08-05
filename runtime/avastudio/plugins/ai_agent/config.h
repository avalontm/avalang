#pragma once

#include <string>

// Qué backend usa el plugin para hablar con el modelo. "openrouter" es el
// comportamiento de siempre (api_key/last_model de abajo). "custom" habilita
// un endpoint compatible con OpenAI (base_url propia, api_key opcional --
// muchos servidores locales tipo LM Studio/Ollama/vLLM no la piden -- y un
// nombre de modelo escrito a mano, porque /v1/models puede no existir o no
// listar lo que el usuario quiere usar).
enum class AgentProvider {
    OpenRouter,
    Custom,
};

struct AgentConfig {
    std::string api_key;
    std::string last_model;

    // Proveedor activo. Guardado como string en disco ("openrouter" /
    // "custom") para no atarse a los valores numéricos del enum.
    AgentProvider provider = AgentProvider::OpenRouter;

    // Config del proveedor "custom", separada de api_key/last_model de
    // arriba a propósito: cambiar de proveedor y volver no debe perder ni
    // la key de OpenRouter ni la del custom.
    std::string custom_base_url;   // p.ej. "http://localhost:1234/v1" (sin /chat/completions al final)
    std::string custom_api_key;    // opcional -- puede quedar vacío
    std::string custom_model;      // id de modelo escrito a mano, p.ej. "llama-3.1-8b-instruct"

    bool IsCustom() const { return provider == AgentProvider::Custom; }
};

AgentConfig LoadAgentConfig();
void SaveAgentConfig(const AgentConfig& config);
