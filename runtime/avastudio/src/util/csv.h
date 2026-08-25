#pragma once

#include <string>
#include <vector>

namespace studio::util {

std::vector<std::vector<std::string>> ParseCsv(const std::string& text);

std::string WriteCsvRow(const std::vector<std::string>& fields);

std::string UnescapeCell(const std::string& raw);
std::string EscapeCell(const std::string& value);

std::vector<std::string> SplitOn(const std::string& text, const std::string& separator);
std::string JoinOn(const std::vector<std::string>& parts, const std::string& separator);

}
