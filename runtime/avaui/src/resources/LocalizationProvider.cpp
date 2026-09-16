#include "ILocalizationProvider.h"
#include "IResourceProvider.h"

#include <unordered_map>
#include <memory>
#include <sstream>

namespace avalang {
namespace ui {

class DefaultLocalizationProvider : public ILocalizationProvider {
public:
    DefaultLocalizationProvider()
        : currentLang_("en"), fallbackLang_("en"),
          provider_(CreateDefaultResourceProvider()) {}

    std::string Resolve(const std::string& key, const std::string& fallback) override {
        std::string value;
        if (LookUp(currentLang_, key, value)) return value;
        if (currentLang_ != fallbackLang_ && LookUp(fallbackLang_, key, value)) return value;
        if (!fallback.empty()) return fallback;
        return key;
    }

    bool LoadLanguage(const std::string& langCode) override {
        if (langCode.empty()) return false;
        currentLang_ = langCode;

        if (dictionaries_.find(langCode) != dictionaries_.end()) {
            return true;
        }

        return LoadDictionaryFromResources(langCode);
    }

    std::string CurrentLanguage() const override {
        return currentLang_;
    }

    void SetFallbackLanguage(const std::string& langCode) override {
        if (!langCode.empty()) {
            fallbackLang_ = langCode;
        }
    }

    void RegisterTranslation(const std::string& langCode,
                             const std::string& key,
                             const std::string& value) override {
        if (langCode.empty() || key.empty()) return;
        dictionaries_[langCode][key] = value;
    }

    uint32_t AbiVersion() const override {
        return 16;
    }

private:
    bool LookUp(const std::string& langCode, const std::string& key, std::string& outValue) const {
        auto langIt = dictionaries_.find(langCode);
        if (langIt == dictionaries_.end()) return false;

        auto keyIt = langIt->second.find(key);
        if (keyIt == langIt->second.end()) return false;

        outValue = keyIt->second;
        return true;
    }

    bool LoadDictionaryFromResources(const std::string& langCode) {
        if (!provider_) return false;

        Resource res = provider_->Load("@local/lang/" + langCode, ResourceType::Localization);
        if (!res.data || res.metadata.dataSize == 0) return false;

        std::string content(reinterpret_cast<const char*>(res.data), res.metadata.dataSize);
        std::istringstream stream(content);
        std::string line;
        auto& dict = dictionaries_[langCode];

        while (std::getline(stream, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty() || line[0] == '#') continue;

            size_t eqPos = line.find('=');
            if (eqPos == std::string::npos) continue;

            std::string key = line.substr(0, eqPos);
            std::string value = line.substr(eqPos + 1);
            if (!key.empty()) {
                dict[key] = value;
            }
        }

        return !dict.empty();
    }

    std::string currentLang_;
    std::string fallbackLang_;
    std::unique_ptr<IResourceProvider> provider_;
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> dictionaries_;
};

ILocalizationProvider* CreateDefaultLocalizationProvider() {
    return new DefaultLocalizationProvider();
}

}
}
