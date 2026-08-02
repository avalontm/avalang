#include "ILocalizationProvider.h"
#include <unordered_map>
#include <memory>

namespace avalang::ui {

/**
 * Default no-op localization provider (Phase 15 design-only).
 * 
 * All keys resolve to themselves; no actual language switching.
 * Real implementation would load JSON/YAML string tables per language.
 */
class DefaultLocalizationProvider : public ILocalizationProvider {
public:
    DefaultLocalizationProvider() 
        : currentLang_("en"), fallbackLang_("en") {}

    std::string Resolve(const std::string& key, const std::string& fallback) override {
        // Phase 15: no-op — just return the key itself or fallback
        if (!fallback.empty()) return fallback;
        return key;
    }

    bool LoadLanguage(const std::string& langCode) override {
        if (langCode.empty()) return false;
        currentLang_ = langCode;
        return true;
    }

    std::string CurrentLanguage() const override {
        return currentLang_;
    }

    void SetFallbackLanguage(const std::string& langCode) override {
        if (!langCode.empty()) {
            fallbackLang_ = langCode;
        }
    }

    uint32_t AbiVersion() const override {
        return 15;
    }

private:
    std::string currentLang_;
    std::string fallbackLang_;
};

ILocalizationProvider* CreateDefaultLocalizationProvider() {
    return new DefaultLocalizationProvider();
}

} // namespace avalang::ui
