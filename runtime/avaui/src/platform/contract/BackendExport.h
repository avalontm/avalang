#pragma once

#if defined(AVA_UI_BUILD_SHARED)
#  if defined(_WIN32)
#    if defined(AVA_UI_BACKEND_BUILDING_LIBRARY)
#      define AVA_UI_BACKEND_API __declspec(dllexport)
#    else
#      define AVA_UI_BACKEND_API __declspec(dllimport)
#    endif
#  else
#    define AVA_UI_BACKEND_API __attribute__((visibility("default")))
#  endif
#else
#  define AVA_UI_BACKEND_API
#endif
