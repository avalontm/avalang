#pragma once

#include "Export.h"
#include <string>
#include <memory>

namespace avalang::ui {

/**
 * Localization provider for UI text strings.
 * 
 * Design (not fully implemented in Phase 15):
 * 
 * - StringTable format: JSON or YAML per language
 *   {
 *     "en": {
 *       "button.ok": "OK",
 *       "button.cancel": "Cancel",
 *       ...
 *     },
 *     "es": {
 *       "button.ok": "Aceptar",
 *       "button.cancel": "Cancelar",
 *       ...
 *     }
 *   }
 * 
 * - Components reference strings by key, not hardcoded text:
 *   <Button text="@button.ok" />
 *   
 *   Instead of:
 *   <Button text="OK" />
 * 
 * - Dynamic language switching: LoadLanguage(lang) re-resolves all keys
 * 
 * - Fallback chain: missing key in current lang -> fallback lang -> key itself
 *   (e.g., @button.ok -> "button.ok" if not found)
 */
class ILocalizationProvider {
public:
    virtual ~ILocalizationProvider() = default;

    /**
     * Resolve a localization key to its translated string.
     * 
     * @param key Dot-separated key (e.g., "button.ok", "menu.file.open")
     * @param fallback Text to return if key not found (default: the key itself)
     * @return Translated string, or fallback if missing
     */
    virtual std::string Resolve(const std::string& key, 
                                const std::string& fallback = "") = 0;

    /**
     * Load a language (switch active language).
     * 
     * @param langCode ISO 639-1 code (e.g., "en", "es", "fr")
     * @return true if language loaded successfully
     */
    virtual bool LoadLanguage(const std::string& langCode) = 0;

    /**
     * Get currently active language code.
     * 
     * @return Language code (e.g., "en")
     */
    virtual std::string CurrentLanguage() const = 0;

    /**
     * Set fallback language for missing keys.
     * 
     * @param langCode ISO 639-1 code (e.g., "en")
     */
    virtual void SetFallbackLanguage(const std::string& langCode) = 0;

    /**
     * Get ABI version for binary compatibility.
     */
    virtual uint32_t AbiVersion() const = 0;
};

/**
 * Factory function to create a default ILocalizationProvider.
 * On all platforms: no-op provider (all keys resolve to themselves).
 * Real localization tables can be plugged in later.
 * 
 * @return Heap-allocated ILocalizationProvider; caller must delete
 */
AVA_UI_API ILocalizationProvider* CreateDefaultLocalizationProvider();

} // namespace avalang::ui
