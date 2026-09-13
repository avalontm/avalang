#ifndef AVA_PLATFORM_BAREKERNEL_EXTERN_THUNK_H
#define AVA_PLATFORM_BAREKERNEL_EXTERN_THUNK_H

#include "stdcompat/ava_stdcompat.h"

namespace ava {
namespace platform {
namespace barekernel {

enum class ExternArgKind : avastd::uint32_t {
    Int32 = 0,
    Int64 = 1,
    Double = 2,
    Pointer = 3,
};

struct ExternArg {
    ExternArgKind kind;
    union {
        avastd::int32_t i32;
        avastd::int64_t i64;
        double          f64;
        void*           ptr;
    };
};

avastd::int32_t CallExternCdecl(void* fn, const avastd::vector<ExternArg>& args);

}
}
}

#endif
