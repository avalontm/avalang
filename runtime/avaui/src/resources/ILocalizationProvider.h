#pragma once

#include "Export.h"
#include <cstdint>
#include <string>
#include <memory>

namespace avalang {
namespace ui {

class ILocalizationProvider {
public:
    virtual ~ILocalizationProvider() = default;

    virtual std::string Resolve(const std::string& key,
                                const std::string& fallback = "") = 0;

    virtual bool LoadLanguage(const std::string& langCode) = 0;

    virtual std::string CurrentLanguage() const = 0;

    virtual void SetFallbackLanguage(const std::string& langCode) = 0;

    virtual void RegisterTranslation(const std::string& langCode,
                                     const std::string& key,
                                     const std::string& value) = 0;

    virtual uint32_t AbiVersion() const = 0;
};

AVA_UI_API ILocalizationProvider* CreateDefaultLocalizationProvider();

}
}