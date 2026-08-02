#include "builtin.h"
#include "vm/value.h"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace {

std::string GetString(AvaVM* vm, const ava_value_t& v) {
    if (v.type != AVA_STRING) return "";
    size_t len = 0;
    const char* data = ava_string_data(vm, v, &len);
    return std::string(data, len);
}

ava_value_t MakeNil() {
    ava_value_t v{};
    v.type = AVA_NIL;
    return v;
}

ava_value_t MakeBool(bool b) {
    ava_value_t v{};
    v.type = AVA_BOOL;
    v.as.b = b;
    return v;
}

ava_value_t MakeNumber(double n) {
    ava_value_t v{};
    v.type = AVA_NUMBER;
    v.as.n = n;
    return v;
}

ava_value_t MakeString(AvaVM* vm, const std::string& s) {
    return ava_string_create(vm, s.c_str(), s.size());
}

ava_value_t MakeList() {
    return ava_list_create(nullptr);
}

}

extern "C" {

ava_value_t builtin_str_upper(AvaVM* vm, const ava_value_t* args, size_t, void*) {
    if (!args) return MakeNil();
    std::string s = GetString(vm, args[0]);
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    return MakeString(vm, s);
}

ava_value_t builtin_str_lower(AvaVM* vm, const ava_value_t* args, size_t, void*) {
    if (!args) return MakeNil();
    std::string s = GetString(vm, args[0]);
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return MakeString(vm, s);
}

ava_value_t builtin_str_split(AvaVM* vm, const ava_value_t* args, size_t count, void*) {
    if (!args || count < 2) return MakeNil();
    std::string s = GetString(vm, args[0]);
    std::string sep = GetString(vm, args[1]);
    
    ava_value_t result = MakeList();
    size_t start = 0;
    size_t pos = s.find(sep);
    
    while (pos != std::string::npos) {
        std::string token = s.substr(start, pos - start);
        ava_value_t tok_val = MakeString(vm, token);
        ava_list_append(vm, result, tok_val);
        start = pos + sep.size();
        pos = s.find(sep, start);
    }
    
    std::string token = s.substr(start);
    ava_value_t tok_val = MakeString(vm, token);
    ava_list_append(vm, result, tok_val);
    
    return result;
}

ava_value_t builtin_str_trim(AvaVM* vm, const ava_value_t* args, size_t, void*) {
    if (!args) return MakeNil();
    std::string s = GetString(vm, args[0]);
    
    auto start = std::find_if_not(s.begin(), s.end(), [](unsigned char c) {
        return std::isspace(c);
    });
    auto end = std::find_if_not(s.rbegin(), s.rend(), [](unsigned char c) {
        return std::isspace(c);
    }).base();
    
    return MakeString(vm, std::string(start, end));
}

ava_value_t builtin_str_contains(AvaVM* vm, const ava_value_t* args, size_t count, void*) {
    if (!args || count < 2) return MakeNil();
    std::string s = GetString(vm, args[0]);
    std::string sub = GetString(vm, args[1]);
    return MakeBool(s.find(sub) != std::string::npos);
}

ava_value_t builtin_str_replace(AvaVM* vm, const ava_value_t* args, size_t count, void*) {
    if (!args || count < 3) return MakeNil();
    std::string s = GetString(vm, args[0]);
    std::string from = GetString(vm, args[1]);
    std::string to = GetString(vm, args[2]);
    
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.length(), to);
        pos += to.length();
    }
    
    return MakeString(vm, s);
}

ava_value_t builtin_str_length(AvaVM* vm, const ava_value_t* args, size_t, void*) {
    if (!args) return MakeNil();
    if (args[0].type == AVA_STRING) {
        size_t len = 0;
        ava_string_data(vm, args[0], &len);
        return MakeNumber(static_cast<double>(len));
    }
    return MakeNil();
}

ava_value_t builtin_str_indexOf(AvaVM* vm, const ava_value_t* args, size_t count, void*) {
    if (!args || count < 2) return MakeNil();
    std::string s = GetString(vm, args[0]);
    std::string sub = GetString(vm, args[1]);
    
    size_t pos = s.find(sub);
    if (pos == std::string::npos) {
        return MakeNumber(-1);
    }
    return MakeNumber(static_cast<double>(pos));
}

ava_value_t builtin_str_startsWith(AvaVM* vm, const ava_value_t* args, size_t count, void*) {
    if (!args || count < 2) return MakeNil();
    std::string s = GetString(vm, args[0]);
    std::string prefix = GetString(vm, args[1]);
    
    if (s.length() < prefix.length()) return MakeBool(false);
    return MakeBool(s.compare(0, prefix.length(), prefix) == 0);
}

ava_value_t builtin_str_endsWith(AvaVM* vm, const ava_value_t* args, size_t count, void*) {
    if (!args || count < 2) return MakeNil();
    std::string s = GetString(vm, args[0]);
    std::string suffix = GetString(vm, args[1]);
    
    if (s.length() < suffix.length()) return MakeBool(false);
    return MakeBool(s.compare(s.length() - suffix.length(), suffix.length(), suffix) == 0);
}

ava_value_t builtin_str_substring(AvaVM* vm, const ava_value_t* args, size_t count, void*) {
    if (!args || count < 2) return MakeNil();
    std::string s = GetString(vm, args[0]);
    int start = static_cast<int>(args[1].as.n);
    
    int end = static_cast<int>(s.length());
    if (count >= 3) {
        end = static_cast<int>(args[2].as.n);
    }
    
    if (start < 0) start = 0;
    if (start > static_cast<int>(s.length())) start = s.length();
    if (end < start) end = start;
    if (end > static_cast<int>(s.length())) end = s.length();
    
    return MakeString(vm, s.substr(start, end - start));
}

}