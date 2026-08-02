#ifndef AVA_UI_EXPORT_H
#define AVA_UI_EXPORT_H

// Same convention as public/include/avalang.h for avalang.dll: a single
// export macro, no per-symbol special-casing. Only meaningful when
// AVA_UI_BUILD_SHARED is on; static builds get an empty macro.

#if defined(AVA_UI_BUILD_SHARED)
#  if defined(_WIN32)
#    if defined(AVA_UI_BUILDING_LIBRARY)
#      define AVA_UI_API __declspec(dllexport)
#    else
#      define AVA_UI_API __declspec(dllimport)
#    endif
#  else
#    define AVA_UI_API __attribute__((visibility("default")))
#  endif
#else
#  define AVA_UI_API
#endif

#endif // AVA_UI_EXPORT_H
