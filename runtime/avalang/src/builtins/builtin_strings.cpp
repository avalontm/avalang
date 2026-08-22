#include "builtin.h"
#include "vm/value.h"
#include "../../platform/barekernel/stdcompat/ava_stdcompat.h"

namespace {

inline bool IsAsciiSpace(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}
inline char ToUpperAscii(char c) {
    return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
}
inline char ToLowerAscii(char c) {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

avastd::string GetString(AvaVM* vm, const ava_value_t& v) {
    if (v.type != AVA_STRING) return "";
    size_t len = 0;
    const char* data = ava_string_data(vm, v, &len);
    return avastd::string(data, len);
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

ava_value_t MakeString(AvaVM* vm, const avastd::string& s) {
    return ava_string_create(vm, s.c_str(), s.size());
}

ava_value_t MakeList() {
    return ava_list_create(nullptr);
}

}

extern "C" {

ava_value_t builtin_str_upper(AvaVM* vm, const ava_value_t* args, size_t, void*) {
    if (!args) return MakeNil();
    avastd::string s = GetString(vm, args[0]);
    for (avastd::size_t i = 0; i < s.size(); ++i) s[i] = ToUpperAscii(s[i]);
    return MakeString(vm, s);
}

ava_value_t builtin_str_lower(AvaVM* vm, const ava_value_t* args, size_t, void*) {
    if (!args) return MakeNil();
    avastd::string s = GetString(vm, args[0]);
    for (avastd::size_t i = 0; i < s.size(); ++i) s[i] = ToLowerAscii(s[i]);
    return MakeString(vm, s);
}

ava_value_t builtin_str_split(AvaVM* vm, const ava_value_t* args, size_t count, void*) {
    if (!args || count < 2) return MakeNil();
    avastd::string s = GetString(vm, args[0]);
    avastd::string sep = GetString(vm, args[1]);

    ava_value_t result = MakeList();
    avastd::size_t start = 0;
    avastd::size_t pos = s.find(sep);

    while (pos != avastd::string::npos) {
        avastd::string token = s.substr(start, pos - start);
        ava_value_t tok_val = MakeString(vm, token);
        ava_list_append(vm, result, tok_val);
        start = pos + sep.size();
        pos = s.find(sep, start);
    }

    avastd::string token = s.substr(start);
    ava_value_t tok_val = MakeString(vm, token);
    ava_list_append(vm, result, tok_val);

    return result;
}

ava_value_t builtin_str_trim(AvaVM* vm, const ava_value_t* args, size_t, void*) {
    if (!args) return MakeNil();
    avastd::string s = GetString(vm, args[0]);

    avastd::size_t start = 0;
    while (start < s.size() && IsAsciiSpace(static_cast<unsigned char>(s[start]))) ++start;

    avastd::size_t end = s.size();
    while (end > start && IsAsciiSpace(static_cast<unsigned char>(s[end - 1]))) --end;

    return MakeString(vm, s.substr(start, end - start));
}

ava_value_t builtin_str_contains(AvaVM* vm, const ava_value_t* args, size_t count, void*) {
    if (!args || count < 2) return MakeNil();
    avastd::string s = GetString(vm, args[0]);
    avastd::string sub = GetString(vm, args[1]);
    return MakeBool(s.find(sub) != avastd::string::npos);
}

ava_value_t builtin_str_replace(AvaVM* vm, const ava_value_t* args, size_t count, void*) {
    if (!args || count < 3) return MakeNil();
    avastd::string s = GetString(vm, args[0]);
    avastd::string from = GetString(vm, args[1]);
    avastd::string to = GetString(vm, args[2]);

    avastd::size_t pos = 0;
    while ((pos = s.find(from, pos)) != avastd::string::npos) {
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
    avastd::string s = GetString(vm, args[0]);
    avastd::string sub = GetString(vm, args[1]);

    avastd::size_t pos = s.find(sub);
    if (pos == avastd::string::npos) {
        return MakeNumber(-1);
    }
    return MakeNumber(static_cast<double>(pos));
}

ava_value_t builtin_str_startsWith(AvaVM* vm, const ava_value_t* args, size_t count, void*) {
    if (!args || count < 2) return MakeNil();
    avastd::string s = GetString(vm, args[0]);
    avastd::string prefix = GetString(vm, args[1]);

    if (s.length() < prefix.length()) return MakeBool(false);
    return MakeBool(s.compare(0, prefix.length(), prefix) == 0);
}

ava_value_t builtin_str_endsWith(AvaVM* vm, const ava_value_t* args, size_t count, void*) {
    if (!args || count < 2) return MakeNil();
    avastd::string s = GetString(vm, args[0]);
    avastd::string suffix = GetString(vm, args[1]);

    if (s.length() < suffix.length()) return MakeBool(false);
    return MakeBool(s.compare(s.length() - suffix.length(), suffix.length(), suffix) == 0);
}

ava_value_t builtin_str_substring(AvaVM* vm, const ava_value_t* args, size_t count, void*) {
    if (!args || count < 2) return MakeNil();
    avastd::string s = GetString(vm, args[0]);
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
