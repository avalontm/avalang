#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace studio {

struct KeywordDoc {
    std::string name;

    std::vector<std::string> syntax;

    std::string example;

    std::string doc;
};

const std::unordered_map<std::string, KeywordDoc>& KeywordDocs();

const std::unordered_map<std::string, KeywordDoc>& DefaultKeywordDocs();

}
