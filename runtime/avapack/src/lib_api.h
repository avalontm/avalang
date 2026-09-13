#pragma once

#if defined(_WIN32)
  #define AVAPACK_LIB_API extern "C" __declspec(dllexport)
#else
  #define AVAPACK_LIB_API extern "C" __attribute__((visibility("default")))
#endif

AVAPACK_LIB_API int avapack_run(int argc, char** argv);

AVAPACK_LIB_API const char* avapack_abi_version(void);
