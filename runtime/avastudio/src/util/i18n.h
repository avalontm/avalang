#pragma once

#include <initializer_list>
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

// Traduce `key` y reemplaza cada "%s" del template, en orden, por el
// siguiente elemento de `args`. Si el template tiene menos "%s" que
// argumentos, los argumentos sobrantes se ignoran.
std::string TrFormat(const std::string& key, std::initializer_list<std::string> args);

}
