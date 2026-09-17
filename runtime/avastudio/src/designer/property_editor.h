#pragma once

#include <string>

#include "designer/property_metadata.h"
#include "designer/types.h"

namespace studio::designer {

std::string PropertyTypeName(PropertyType type);
PropertyType PropertyTypeFromName(const std::string& name);

std::string FormatPropertyValue(const PropertyValue& value);
bool TryParsePropertyValue(const std::string& text, PropertyType type, PropertyValue& out);

bool ValidatePropertyEdit(const PropertyMetadata& metadata, const std::string& text);

bool TryParseColorValue(const std::string& text, float out_rgba[4]);
std::string FormatColorValue(const float rgba[4]);

}
