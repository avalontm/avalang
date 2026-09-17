#include "designer/property_editor.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <regex>
#include <sstream>

#include "common/ColorParse.h"

namespace studio::designer {

namespace {

std::string ToLower(const std::string& value) {
    std::string result = value;
    std::transform(result.begin(), result.end(), result.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

}

std::string PropertyTypeName(PropertyType type) {
    switch (type) {
        case PropertyType::Bool:
            return "bool";
        case PropertyType::Number:
            return "number";
        case PropertyType::String:
            return "string";
        case PropertyType::List:
            return "list";
        case PropertyType::Nil:
        default:
            return "any";
    }
}

PropertyType PropertyTypeFromName(const std::string& name) {
    if (name == "bool") {
        return PropertyType::Bool;
    }
    if (name == "number") {
        return PropertyType::Number;
    }
    if (name == "list") {
        return PropertyType::List;
    }
    if (name == "any") {
        return PropertyType::Nil;
    }
    return PropertyType::String;
}

std::string FormatPropertyValue(const PropertyValue& value) {
    switch (value.Type()) {
        case PropertyType::Bool:
            return value.AsBool() ? "true" : "false";
        case PropertyType::Number: {
            double number = value.AsNumber();
            double rounded = std::round(number);
            if (std::fabs(number - rounded) < 1e-9) {
                return std::to_string(static_cast<long long>(rounded));
            }
            std::ostringstream out;
            out << number;
            return out.str();
        }
        case PropertyType::String:
            return value.AsString();
        case PropertyType::List:
        case PropertyType::Nil:
        default:
            return "";
    }
}

bool TryParsePropertyValue(const std::string& text, PropertyType type, PropertyValue& out) {
    switch (type) {
        case PropertyType::Bool: {
            std::string lowered = ToLower(text);
            if (lowered == "true" || lowered == "1") {
                out = PropertyValue(true);
                return true;
            }
            if (lowered == "false" || lowered == "0") {
                out = PropertyValue(false);
                return true;
            }
            return false;
        }
        case PropertyType::Number: {
            if (text.empty()) {
                return false;
            }
            size_t consumed = 0;
            double parsed = 0.0;
            try {
                parsed = std::stod(text, &consumed);
            } catch (...) {
                return false;
            }
            if (consumed != text.size()) {
                return false;
            }
            out = PropertyValue(parsed);
            return true;
        }
        case PropertyType::String:
            out = PropertyValue(text);
            return true;
        case PropertyType::List:
        case PropertyType::Nil:
        default:
            return false;
    }
}

bool ValidatePropertyEdit(const PropertyMetadata& metadata, const std::string& text) {
    if (metadata.readOnly) {
        return false;
    }

    PropertyValue parsed;
    if (!TryParsePropertyValue(text, PropertyTypeFromName(metadata.type), parsed)) {
        return false;
    }

    if (!metadata.validation.empty()) {
        try {
            std::regex pattern(metadata.validation);
            if (!std::regex_match(text, pattern)) {
                return false;
            }
        } catch (const std::regex_error&) {
            return false;
        }
    }

    return true;
}

bool TryParseColorValue(const std::string& text, float out_rgba[4]) {
    const std::string digits = (!text.empty() && text[0] == '#') ? text.substr(1) : text;
    if (digits.empty()) {
        return false;
    }
    for (char c : digits) {
        if (!std::isxdigit(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    if (digits.size() != 3 && digits.size() != 6 && digits.size() != 8) {
        return false;
    }

    const avalang::ui::Color parsed = avalang::ui::common::ParseColor(digits);
    out_rgba[0] = static_cast<float>(parsed.r) / 255.0f;
    out_rgba[1] = static_cast<float>(parsed.g) / 255.0f;
    out_rgba[2] = static_cast<float>(parsed.b) / 255.0f;
    out_rgba[3] = static_cast<float>(parsed.a) / 255.0f;
    return true;
}

std::string FormatColorValue(const float rgba[4]) {
    const auto toByte = [](float channel) {
        const float clamped = std::clamp(channel, 0.0f, 1.0f);
        return static_cast<unsigned>(std::lround(clamped * 255.0f));
    };
    const unsigned r = toByte(rgba[0]);
    const unsigned g = toByte(rgba[1]);
    const unsigned b = toByte(rgba[2]);
    const unsigned a = toByte(rgba[3]);

    char buffer[10];
    if (a == 255) {
        std::snprintf(buffer, sizeof(buffer), "#%02X%02X%02X", r, g, b);
    } else {
        std::snprintf(buffer, sizeof(buffer), "#%02X%02X%02X%02X", r, g, b, a);
    }
    return std::string(buffer);
}

}
