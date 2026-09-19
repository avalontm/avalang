#pragma once

#include <string>

namespace studio {

struct ParsedAvaError {
    std::string kind;
    std::string message;
    std::string file;
    int line = 0;
    int col = 0;
};

bool ParseAvaErrorLine(const std::string& line, ParsedAvaError& out);

}
