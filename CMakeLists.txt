cmake_minimum_required(VERSION 3.20)
project(avalang LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Force vcpkg to use static-md triplet where antlr4-runtime is installed
set(VCPKG_TARGET_TRIPLET x64-windows-static-md)
set(ENV{VCPKG_TARGET_TRIPLET} x64-windows-static-md)

option(AVA_BUILD_SHARED "Build avalang as a shared library (.dll/.so)" ON)
option(AVA_BUILD_CLI "Build the standalone ava_cli (compile/run scripts)" ON)

# ---------------------------------------------------------------------
# ANTLR4 C++ runtime (optional at configure time)
#
# If antlr4-runtime is not found, the core VM / C API / CLI still build
# and link, using a stub frontend that reports a clear error instead of
# compiling scripts. This lets you build and explore the project today
# without installing ANTLR4 first; install it and reconfigure to enable
# ava_compile() for real.
# ---------------------------------------------------------------------
# Add static-md triplet path BEFORE find_package so it can find the installed antlr4
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows-static-md")

find_program(JAVA_EXECUTABLE java)
find_package(antlr4-runtime QUIET)
message(STATUS "AvaLang debug: VCPKG_TARGET_TRIPLET = '${VCPKG_TARGET_TRIPLET}'")
message(STATUS "AvaLang debug: CMAKE_PREFIX_PATH     = '${CMAKE_PREFIX_PATH}'")
message(STATUS "AvaLang debug: antlr4-runtime_FOUND = '${antlr4-runtime_FOUND}'")
message(STATUS "AvaLang debug: antlr4-runtime_DIR   = '${antlr4-runtime_DIR}'")
find_file(ANTLR4_JAR
    NAMES antlr4-complete.jar antlr-4.13.2-complete.jar antlr-4.13.1-complete.jar antlr-4.11.1-complete.jar
    PATHS /usr/local/lib /usr/share/java ${CMAKE_SOURCE_DIR}/third_party
)

# Fallback: si el nombre exacto no matcheó (p.ej. se subió la version de ANTLR
# en install.bat y no se actualizo la lista NAMES de arriba), buscamos
# cualquier antlr-*-complete.jar en third_party/ para no volver a romper esto.
if(NOT ANTLR4_JAR)
    file(GLOB _ava_antlr_jar_candidates "${CMAKE_SOURCE_DIR}/third_party/antlr-*-complete.jar")
    if(_ava_antlr_jar_candidates)
        list(GET _ava_antlr_jar_candidates 0 ANTLR4_JAR)
        message(STATUS "AvaLang debug: ANTLR4_JAR resuelto por fallback glob -> '${ANTLR4_JAR}'")
    endif()
endif()

set(GRAMMAR_DIR ${CMAKE_SOURCE_DIR}/grammar)
set(GENERATED_DIR ${CMAKE_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_DIR})

if(antlr4-runtime_FOUND AND JAVA_EXECUTABLE AND ANTLR4_JAR)
    set(AVA_HAVE_ANTLR TRUE)
    message(STATUS "avalang: antlr4-runtime found, building full frontend")

    add_custom_command(
        OUTPUT
            ${GENERATED_DIR}/AvaLangLexer.cpp
            ${GENERATED_DIR}/AvaLangParser.cpp
            ${GENERATED_DIR}/AvaLangBaseListener.cpp
        COMMAND ${JAVA_EXECUTABLE} -jar ${ANTLR4_JAR}
                -Dlanguage=Cpp
                -visitor
                -o ${GENERATED_DIR}
                ${GRAMMAR_DIR}/AvaLang.g4
        DEPENDS ${GRAMMAR_DIR}/AvaLang.g4
        COMMENT "Generating ANTLR4 C++ parser from AvaLang.g4"
    )

    set(GENERATED_SOURCES
        ${GENERATED_DIR}/AvaLangLexer.cpp
        ${GENERATED_DIR}/AvaLangParser.cpp
        ${GENERATED_DIR}/AvaLangBaseListener.cpp
    )
    set(FRONTEND_SOURCES
        src/frontend/denter.cpp
        src/frontend/frontend_antlr.cpp
        src/ast/ast_builder.cpp
        src/compiler/compiler.cpp
        ${GENERATED_SOURCES}
    )
else()
    set(AVA_HAVE_ANTLR FALSE)
    message(STATUS "avalang: antlr4-runtime NOT found, building with stub frontend "
                    "(ava_compile() will report a clear error; VM/C-API/CLI still build)")
    set(FRONTEND_SOURCES src/frontend/frontend_stub.cpp)
endif()

# ---------------------------------------------------------------------
# Core library sources
# ---------------------------------------------------------------------
set(CORE_SOURCES
    src/vm/value.cpp
    src/vm/vm.cpp
    src/vm/module.cpp
    src/vm/coroutine.cpp
    src/api/c_api.cpp
    src/builtins/builtin_strings.cpp
    src/builtins/builtin_lists.cpp
    src/builtins/builtin_dicts.cpp
    src/builtins/builtin_coroutine.cpp
    src/builtins/builtin_registry.cpp
    ${FRONTEND_SOURCES}
)

if(AVA_BUILD_SHARED)
    add_library(avalang SHARED ${CORE_SOURCES})
else()
    add_library(avalang STATIC ${CORE_SOURCES})
endif()

target_include_directories(avalang
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${GENERATED_DIR}
)

if(AVA_HAVE_ANTLR)
    # El paquete de vcpkg "antlr4-runtime" NO expone un target llamado
    # antlr4-runtime; expone antlr4_static (triplets estaticos, p.ej.
    # x64-windows-static-md) o antlr4_shared (triplets dinamicos).
    if(TARGET antlr4_static)
        set(_ava_antlr_target antlr4_static)
    elseif(TARGET antlr4_shared)
        set(_ava_antlr_target antlr4_shared)
    else()
        message(FATAL_ERROR "avalang: antlr4-runtime_FOUND es TRUE pero no existe ni "
                             "el target antlr4_static ni antlr4_shared. Revisa la version "
                             "del paquete vcpkg instalado.")
    endif()
    message(STATUS "AvaLang debug: usando target ANTLR = '${_ava_antlr_target}'")

    target_link_libraries(avalang PRIVATE ${_ava_antlr_target})
    target_compile_definitions(avalang PRIVATE AVA_HAVE_ANTLR)
    if(_ava_antlr_target STREQUAL "antlr4_static")
        # Requerido para que antlr4-runtime.h no marque sus clases como
        # dllimport (lo cual rompe el link contra una lib estatica).
        target_compile_definitions(avalang PRIVATE ANTLR4CPP_STATIC)
    endif()
endif()

target_compile_definitions(avalang PRIVATE AVA_BUILDING_LIBRARY)
# NOTE: symbol visibility is intentionally left at the compiler default
# (not hidden) so internal headers (src/vm/*.h) remain linkable from
# tests/ and from ava_cli, which both use VM internals directly rather
# than going through the C API. External bindings should still only rely
# on include/ava.h -- the internal headers are not a stable contract.

# ---------------------------------------------------------------------
# CLI (native reference host, useful for testing scripts without a binding)
# ---------------------------------------------------------------------
if(AVA_BUILD_CLI)
    add_executable(ava_cli src/cli/main.cpp)
    target_link_libraries(ava_cli PRIVATE avalang)
endif()

# No tests - use scripts to test

install(TARGETS avalang
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)
install(FILES include/ava.h DESTINATION include)
