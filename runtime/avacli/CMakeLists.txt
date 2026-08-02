# ---------------------------------------------------------------------
# ava_cli -- host de referencia nativo (compila/corre scripts .ava
# desde línea de comandos, sin necesitar un binding en otro lenguaje).
#
# Incluido desde el CMakeLists.txt raíz vía:
#   option(AVA_BUILD_CLI ...)
#   if(AVA_BUILD_CLI) add_subdirectory(runtime/avacli) endif()
#
# main.cpp usa internals del VM directamente (ava::VM*, no sólo
# avalang.h) para poder drenar el event loop async al final del
# script -- por eso necesita los headers PRIVATE de avalang, no sólo
# su API pública instalada.
# ---------------------------------------------------------------------

add_executable(ava_cli src/main.cpp)

target_link_libraries(ava_cli PRIVATE avalang)

target_include_directories(ava_cli PRIVATE
    ${CMAKE_SOURCE_DIR}/runtime/avalang/src
    ${CMAKE_SOURCE_DIR}/runtime/avalang/api/src
    ${CMAKE_SOURCE_DIR}/runtime/avalang/api/include
)

# En Windows no hay RPATH: ava_cli.exe necesita avalang.dll en su misma
# carpeta para poder arrancar. Como avalang y ava_cli ahora viven en
# CMakeLists.txt separados (carpetas de build distintas por defecto),
# forzamos a que ava_cli.exe caiga junto a avalang.dll/.so explícitamente.
# En Linux/macOS no hace falta (CMake ya usa RPATH del build tree), pero
# no hace daño tenerlos juntos igual.
set_target_properties(ava_cli PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/runtime/avalang"
)
