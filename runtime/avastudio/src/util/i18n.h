#pragma once

#include <string>

namespace studio::util {

enum class Locale {
    English,
    Spanish,
};

Locale LocaleFromString(const std::string& value, Locale fallback_locale = Locale::English);

std::string LocaleToString(Locale locale);

void SetLocale(Locale locale);
Locale GetLocale();

const std::string& Tr(const std::string& key);

}
