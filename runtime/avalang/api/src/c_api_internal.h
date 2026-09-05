#ifndef AVA_API_C_API_INTERNAL_H
#define AVA_API_C_API_INTERNAL_H

#include "../../platform/barekernel/stdcompat/ava_stdcompat.h"
#include "../../platform/AvaMemory.h"

inline char* DupString(const avastd::string& s) {
    char* out = static_cast<char*>(ava_alloc(s.size() + 1));
    avastd::memcpy(out, s.c_str(), s.size() + 1);
    return out;
}

#endif // AVA_API_C_API_INTERNAL_H
