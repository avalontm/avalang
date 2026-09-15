#pragma once

#include <stddef.h>

#include "avalang.h"

#if defined(_WIN32)
  #define AVAPACK_LIB_API extern "C" __declspec(dllexport)
#else
  #define AVAPACK_LIB_API extern "C" __attribute__((visibility("default")))
#endif

AVAPACK_LIB_API int avapack_run(int argc, char** argv);

AVAPACK_LIB_API const char* avapack_abi_version(void);

AVAPACK_LIB_API int avapack_init(int argc, char** argv, char** out_error);

AVAPACK_LIB_API int avapack_is_loaded(void);

AVAPACK_LIB_API int avapack_call(
    const char* fn_name,
    const ava_value_t* args,
    size_t arg_count,
    ava_value_t* out_result,
    char** out_error
);

AVAPACK_LIB_API ava_value_t avapack_get_global(const char* name);

AVAPACK_LIB_API void avapack_set_global(const char* name, ava_value_t value);

AVAPACK_LIB_API AvaVM* avapack_vm(void);

AVAPACK_LIB_API void avapack_free_error(char* error);

AVAPACK_LIB_API void avapack_shutdown(void);
