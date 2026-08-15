# ---------------------------------------------------------------------
# avalang -- core library (VM + compiler frontend + C API) y ava_cli.
#
# Incluido desde el CMakeLists.txt raíz vía add_subdirectory(runtime/avalang).
# La detección de ANTLR4 y libffi vive en cmake/FindAntlr4Jar.cmake y
# cmake/FindLibFFI.cmake (incluidos desde la raíz antes de este add_subdirectory,
# así que sus variables ya están disponibles aquí).
# ---------------------------------------------------------------------

set(GRAMMAR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/grammar)
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
# Core library sources (PRIVATE - do not export)
# ---------------------------------------------------------------------
set(CORE_SOURCES
    src/vm/value.cpp
    src/vm/vm_helpers.cpp
    src/vm/vm.cpp
    src/vm/vm_core.cpp
    src/vm/vm_call.cpp
    src/vm/vm_file.cpp
    src/vm/vm_import.cpp
    src/vm/vm_arith.cpp
    src/vm/vm_compare.cpp
    src/vm/vm_containers.cpp
    src/vm/vm_classes.cpp
    src/vm/vm_call_op.cpp
    src/vm/vm_slice.cpp
    src/vm/vm_exceptions.cpp
    src/vm/vm_errors.cpp
    src/vm/vm_extern.cpp
    src/vm/vm_platform_accessor.cpp
    src/vm/vm_async.cpp
    src/vm/module.cpp
    src/vm/coroutine.cpp
    platform/memory/MemoryFileSystem.cpp
    src/compiler/proto_io.cpp
    src/compiler/obfuscate.cpp
    src/builtins/builtin_strings.cpp
    src/builtins/builtin_lists.cpp
    src/builtins/builtin_dicts.cpp
    src/builtins/builtin_coroutine.cpp
    src/builtins/builtin_async.cpp
    src/builtins/builtin_registry.cpp
    src/builtins/builtin_natives.cpp
    src/builtins/builtin_mem.cpp
    src/builtins/builtin_init.cpp
    src/ui/builtins.cpp
    ${FRONTEND_SOURCES}
)

# ---------------------------------------------------------------------
# Platform Abstraction Layer backend (compile-time selection, no runtime
# OS detection -- see docs/PAL_PROGRESS.md, "Alcance actual: solo Windows").
#   Windows -> platform/windows (functional, only backend in active development)
#   Linux   -> platform/linux   (STUB, en estudio / pausado a propósito)
#   macOS   -> platform/macos   (STUB, en estudio / pausado a propósito)
# ---------------------------------------------------------------------
if(WIN32)
    set(PLATFORM_SOURCES
        platform/windows/WinFileSystem.cpp
        platform/windows/WinThread.cpp
        platform/windows/WinMutex.cpp
        platform/windows/WinClock.cpp
        platform/windows/WinLibrary.cpp
        platform/windows/WinConsole.cpp
        platform/windows/WinEnvironment.cpp
        platform/windows/WinProcess.cpp
        platform/windows/WinTimer.cpp
        platform/windows/WinPlatform.cpp
    )
elseif(APPLE)
    set(PLATFORM_SOURCES
        platform/macos/MacFileSystem.cpp
        platform/macos/MacThread.cpp
        platform/macos/MacMutex.cpp
        platform/macos/MacClock.cpp
        platform/macos/MacLibrary.cpp
        platform/macos/MacConsole.cpp
        platform/macos/MacEnvironment.cpp
        platform/macos/MacProcess.cpp
        platform/macos/MacTimer.cpp
        platform/macos/MacPlatform.cpp
    )
    message(STATUS "avalang: macOS PAL backend is a STUB (Phase 6 not implemented yet)")
elseif(UNIX)
    set(PLATFORM_SOURCES
        platform/linux/LinFileSystem.cpp
        platform/linux/LinThread.cpp
        platform/linux/LinMutex.cpp
        platform/linux/LinClock.cpp
        platform/linux/LinLibrary.cpp
        platform/linux/LinConsole.cpp
        platform/linux/LinEnvironment.cpp
        platform/linux/LinProcess.cpp
        platform/linux/LinTimer.cpp
        platform/linux/LinPlatform.cpp
    )
    message(STATUS "avalang: Linux PAL backend functional (POSIX + pthread + dl)")
else()
    message(FATAL_ERROR "avalang: unsupported target platform (no PAL backend available)")
endif()

list(APPEND CORE_SOURCES ${PLATFORM_SOURCES})

# ---------------------------------------------------------------------
# Public library (core + C API)
# ---------------------------------------------------------------------
set(PUBLIC_SOURCES
    ${CORE_SOURCES}
    api/src/c_api.cpp
)

if(AVA_BUILD_SHARED)
    add_library(avalang SHARED ${PUBLIC_SOURCES})
else()
    add_library(avalang STATIC ${PUBLIC_SOURCES})
endif()

target_include_directories(avalang
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/api/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/api/src
        ${CMAKE_CURRENT_SOURCE_DIR}/api/include
        ${GENERATED_DIR}
        ${CMAKE_SOURCE_DIR}/runtime/avaui/src
)

target_link_libraries(avalang PRIVATE avalang_ui)

if(AVA_HAVE_ANTLR)
    # El paquete de vcpkg "antlr4-runtime" NO expone un target llamado
    # antlr4-runtime; expone antlr4_static (triplets estaticos, p.ej.
    # x64-windows-static-md) o antlr4_shared (triplets dinamicos).
    if(TARGET antlr4_static)
        set(_ava_antlr_target antlr4_static)
    elseif(TARGET antlr4_shared)
        set(_ava_antlr_target antlr4_shared)
    else()
        # Debian/Ubuntu: no hay target de vcpkg, pero FindAntlr4Jar detecto
        # los headers y la lib via find_path/find_library y seteo
        # antlr4-runtime_FOUND a mano. Usamos esas variables directamente.
        set(_ava_antlr_target "")
        if(ANTLR4_RUNTIME_INCLUDE_DIR)
            target_include_directories(avalang PRIVATE ${ANTLR4_RUNTIME_INCLUDE_DIR})
        endif()
        if(ANTLR4_RUNTIME_LIBRARY)
            target_link_libraries(avalang PRIVATE ${ANTLR4_RUNTIME_LIBRARY})
        endif()
    endif()
    if(_ava_antlr_target)
        message(STATUS "AvaLang debug: usando target ANTLR = '${_ava_antlr_target}'")
        target_link_libraries(avalang PRIVATE ${_ava_antlr_target})
        if(_ava_antlr_target STREQUAL "antlr4_static")
            # Requerido para que antlr4-runtime.h no marque sus clases como
            # dllimport (lo cual rompe el link contra una lib estatica).
            target_compile_definitions(avalang PRIVATE ANTLR4CPP_STATIC)
        endif()
    else()
        message(STATUS "AvaLang debug: usando antlr4-runtime del sistema (Debian/Ubuntu)")
    endif()
    # Debian/Ubuntu: el config package define ANTLR4_INCLUDE_DIR apuntando
    # a /usr/include/antlr4-runtime (los headers viven en un subdirectorio).
    # El target antlr4_static del sistema NO propaga el include dir, asi que
    # lo añadimos a mano para que #include "antlr4-runtime.h" resuelva.
    if(ANTLR4_INCLUDE_DIR)
        target_include_directories(avalang PRIVATE ${ANTLR4_INCLUDE_DIR})
    endif()
    target_compile_definitions(avalang PRIVATE AVA_HAVE_ANTLR)
endif()

target_compile_definitions(avalang PRIVATE AVA_BUILDING_LIBRARY)

if(AVA_HAVE_LIBFFI)
    target_include_directories(avalang PRIVATE ${LIBFFI_INCLUDE_DIR})
    target_link_libraries(avalang PRIVATE ${LIBFFI_LIBRARY})
    target_compile_definitions(avalang PRIVATE AVA_HAVE_LIBFFI)
endif()
if(UNIX AND NOT APPLE)
    # dlopen/dlsym en glibc viejas requiere -ldl explicito (algunas
    # toolchains modernas ya lo integran en libc, pero enlazarlo no hace
    # daño y evita romper en distros mas viejas).
    target_link_libraries(avalang PRIVATE dl)
    # pthread: std::thread/std::mutex/std::condition_variable (usados por
    # LinThread/LinMutex/LinTimer y el async runtime del VM) envuelven
    # pthread en glibc; -lpthread es necesario en toolchains viejos y no
    # hace daño en modernos.
    target_link_libraries(avalang PRIVATE pthread)
    # clock_gettime/nanosleep (LinClock) vivian en librt en glibc vieja;
    # en modernas estan en libc pero enlazar rt no hace daño.
    find_library(RT_LIBRARY rt)
    if(RT_LIBRARY)
        target_link_libraries(avalang PRIVATE ${RT_LIBRARY})
    endif()
endif()

# NOTE: symbol visibility is intentionally left at the compiler default
# (not hidden) so internal headers (src/vm/*.h) remain linkable from
# tests/ and from ava_cli, which both use VM internals directly rather
# than going through the C API. External bindings should still only rely
# on public/include/avalang.h -- the internal headers are not a stable contract.

# No tests - use scripts to test
#
# NOTA: ava_cli ya NO se define aquí -- vive en runtime/avacli/ (ver
# root CMakeLists.txt: if(AVA_BUILD_CLI) add_subdirectory(runtime/avacli)).
# Este archivo sólo define la librería `avalang`.

install(TARGETS avalang
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)
install(FILES api/include/avalang.h DESTINATION include)
