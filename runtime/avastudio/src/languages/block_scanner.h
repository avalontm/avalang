#pragma once

#include <cstddef>
#include <string>

namespace studio {

bool IsBlockKeyword(const std::string& word);

bool FindMatchingEnd(const std::string& text, size_t& i, size_t& body_end);

}
