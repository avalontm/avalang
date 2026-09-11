#pragma once

#include <cstddef>
#include <string>

namespace studio {

bool IsBlockKeyword(const std::string& word);

bool FindMatchingEnd(const std::string& text, size_t& i, size_t& body_end, bool is_interface_body = false);

// 0-based line number of `offset` within `text` (counts '\n' in [0, offset)).
// Shared by ClassIndex/FunctionIndex so declarations can carry a jump target
// for "Go to Definition" without each scanner reimplementing the count.
int LineAt(const std::string& text, size_t offset);

}
