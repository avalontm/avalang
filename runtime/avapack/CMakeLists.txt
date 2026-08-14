# ---------------------------------------------------------------------
# avapack -- Fase 3 (contenido cifrado) + Fase 5 (integridad / modo
# debug). Ver runtime/avapack/README.md.
#
# Dos piezas:
#   avapack_gen         -- herramienta de host, standalone (no linkea
#                           avalang): lee --project/--entry[/--key-file],
#                           cifra con AES-256-CTR (tiny-AES-c, ver
#                           third_party/tiny-aes-c/VENDOR.md) y genera
#                           embedded_project.cpp.
#   ${AVAPACK_OUT_NAME}  -- el .exe "empacado" final: src/main.cpp +
#                           el embedded_project.cpp generado, linkeado
#                           dinamico contra avalang (mismo patron que
#                           ava_cli, ver runtime/avacli/CMakeLists.txt).
#                           Tambien linkea tiny-AES-c para descifrar en
#                           memoria al arrancar.
#
# Que proyecto se empaca se controla con variables de CMake
# (AVAPACK_PROJECT_DIR / AVAPACK_ENTRY_FILE / AVAPACK_OUT_NAME /
# AVAPACK_KEY_FILE) -- `ava_cli build` (Fase 2) las fija dinamicamente al
# invocar `cmake -S/-B`. Tambien sirve para probar el flujo end-to-end a
# mano:
#
#   cmake -S . -B build_pack -DAVA_BUILD_PACK=ON
#   cmake --build build_pack --target avapack_testproj
#
# Incluido desde el CMakeLists.txt raiz via:
#   option(AVA_BUILD_PACK ...)
#   if(AVA_BUILD_PACK) add_subdirectory(runtime/avapack) endif()
# ---------------------------------------------------------------------

set(AVAPACK_TINY_AES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/tiny-aes-c")
set(AVAPACK_TINY_AES_SRC "${AVAPACK_TINY_AES_DIR}/aes.c")
# Solo se compila el modo CTR (AES-256) -- ver VENDOR.md sobre por que no
# GCM y por que no hace falta ECB/CBC para este empacador.
set(AVAPACK_TINY_AES_DEFS AES256=1 CTR=1 CBC=0 ECB=0)

# Fase 5: SHA-256/HMAC-SHA256 propios (no vendorizados -- ver
# src/checksum/sha256.h), usados tanto por avapack_gen (calcular
# kIntegrityMac) como por el .exe empacado (recalcularlo y verificarlo).
set(AVAPACK_CHECKSUM_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/checksum/sha256.cpp")

# --- avapack_gen: necesita cifrar (tiny-AES-c) y hashear
#     (checksum/sha256.cpp, Fase 5). Desde Fase 6 tambien linkea avalang
#     -- SOLO via la API publica (avalang.h), para --obfuscate: compilar
#     el entry a Proto, correr ObfuscateProto y serializarlo a .avbc antes
#     de embeberlo. Sin --obfuscate, el flujo de Fases 1-5 (texto plano
#     cifrado) sigue igual y no toca avalang para nada -- ver comentario
#     de cabecera de generator/main.cpp. ---
add_executable(avapack_gen src/generator/main.cpp "${AVAPACK_TINY_AES_SRC}" "${AVAPACK_CHECKSUM_SRC}")
target_include_directories(avapack_gen PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${AVAPACK_TINY_AES_DIR}
)
target_compile_definitions(avapack_gen PRIVATE ${AVAPACK_TINY_AES_DEFS})
target_link_libraries(avapack_gen PRIVATE avalang)
set_target_properties(avapack_gen PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/runtime/avalang"
)

# Fase 5: --debug (ver runtime/avacli/src/build_command.cpp, flag --debug
# de `ava_cli build`) se traduce a esta opcion de CMake, que a su vez
# agrega --debug a AVAPACK_GEN_ARGS mas abajo.
option(AVAPACK_DEBUG_UNENCRYPTED
    "Fase 5: genera embedded_project.cpp con el contenido SIN CIFRAR (modo debug empacado, no usar para distribuir)"
    OFF)

# Fase 6 (ver runtime/avacli/src/build_command.cpp, flags --obfuscate/
# --obfuscate-strings/--flatten-control-flow de `ava_cli build`).
option(AVAPACK_OBFUSCATE
    "Fase 6: compila+serializa el entry a bytecode .avbc en vez de embeber su .ava en texto"
    OFF)
option(AVAPACK_OBFUSCATE_STRINGS
    "Fase 6: (requiere AVAPACK_OBFUSCATE) ademas ofusca los strings de la constant pool del entry"
    OFF)
option(AVAPACK_FLATTEN_CONTROL_FLOW
    "Fase 6: (requiere AVAPACK_OBFUSCATE) ademas aplana el control-flow del entry"
    OFF)

# Fase 7 (ver runtime/avacli/src/build_command.cpp, flag --zero-disk de
# `ava_cli build`): compila src/main_zerodisk.cpp en vez de src/main.cpp.
# Filesystem virtual en memoria (runtime/avalang/platform/memory/) en vez
# del esquema de hooks + temp dir de Fase 4 -- ningun .ava/.avbc del
# proyecto toca disco, ni siquiera por milisegundos. Mutuamente excluyente
# con el flujo de Fase 4 (no hay temp dir en absoluto en este modo).
option(AVAPACK_ZERO_DISK
    "Fase 7: usa MemoryFileSystem (cero disco) en vez del esquema de temp dir + hooks de Fase 4"
    OFF)

# --- proyecto de prueba a empacar (override con -DAVAPACK_PROJECT_DIR=...) ---
set(AVAPACK_PROJECT_DIR "${CMAKE_SOURCE_DIR}/samples/web/testproj" CACHE PATH
    "Carpeta del proyecto AvaLang a empacar (Fase 2 la fija dinamicamente via ava_cli build)")
set(AVAPACK_ENTRY_FILE "app.ava" CACHE STRING
    "Archivo .ava de entrada, relativo a AVAPACK_PROJECT_DIR")
set(AVAPACK_OUT_NAME "avapack_testproj" CACHE STRING
    "Nombre del target/ejecutable empacado resultante (sin extension)")
set(AVAPACK_KEY_FILE "" CACHE FILEPATH
    "(Fase 3, opcional) ruta a 32 bytes crudos con la clave AES-256 a usar. Vacio = clave aleatoria por build.")
set(AVAPACK_EXTRA_MODULES_DIR "" CACHE PATH
    "(opcional) carpeta con modulos nativos (libraries/) para embeber si el proyecto los importa")

set(AVAPACK_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(AVAPACK_GENERATED_CPP "${AVAPACK_GENERATED_DIR}/embedded_project.cpp")

set(AVAPACK_GEN_ARGS
    --project "${AVAPACK_PROJECT_DIR}"
    --entry "${AVAPACK_ENTRY_FILE}"
    --out "${AVAPACK_GENERATED_CPP}"
)
if(NOT "${AVAPACK_KEY_FILE}" STREQUAL "")
    list(APPEND AVAPACK_GEN_ARGS --key-file "${AVAPACK_KEY_FILE}")
endif()
if(NOT "${AVAPACK_EXTRA_MODULES_DIR}" STREQUAL "")
    list(APPEND AVAPACK_GEN_ARGS --extra-modules-dir "${AVAPACK_EXTRA_MODULES_DIR}")
endif()
if(AVAPACK_OBFUSCATE)
    list(APPEND AVAPACK_GEN_ARGS --obfuscate)
    if(AVAPACK_OBFUSCATE_STRINGS)
        list(APPEND AVAPACK_GEN_ARGS --obfuscate-strings)
    endif()
    if(AVAPACK_FLATTEN_CONTROL_FLOW)
        list(APPEND AVAPACK_GEN_ARGS --flatten-control-flow)
    endif()
endif()
if(AVAPACK_DEBUG_UNENCRYPTED)
    list(APPEND AVAPACK_GEN_ARGS --debug)
endif()

add_custom_command(
    OUTPUT "${AVAPACK_GENERATED_CPP}"
    COMMAND avapack_gen ${AVAPACK_GEN_ARGS}
    DEPENDS avapack_gen
    COMMENT "avapack: generando + cifrando embedded_project.cpp desde ${AVAPACK_PROJECT_DIR} (entry: ${AVAPACK_ENTRY_FILE})"
    VERBATIM
)
add_custom_target(avapack_generate DEPENDS "${AVAPACK_GENERATED_CPP}")

# --- el .exe empacado final ---
# Fase 7: AVAPACK_ZERO_DISK=ON cambia la plantilla de runtime completa
# (main_zerodisk.cpp) en vez de solo agregar codigo condicional dentro de
# main.cpp -- los dos flujos (temp dir + hooks vs. filesystem virtual) son
# suficientemente distintos como para no valer la pena entrelazarlos en un
# solo archivo con #ifdef. Ambos comparten embedded_crypto.h/embedded_project.h.
if(AVAPACK_ZERO_DISK)
    set(AVAPACK_MAIN_SRC src/main_zerodisk.cpp)
    message(STATUS "avapack: AVAPACK_ZERO_DISK=ON -- usando main_zerodisk.cpp (Fase 7, filesystem virtual en memoria)")
else()
    set(AVAPACK_MAIN_SRC src/main.cpp)
endif()

add_executable(${AVAPACK_OUT_NAME} "${AVAPACK_MAIN_SRC}" "${AVAPACK_GENERATED_CPP}"
    "${AVAPACK_TINY_AES_SRC}" "${AVAPACK_CHECKSUM_SRC}")
add_dependencies(${AVAPACK_OUT_NAME} avapack_generate)

target_link_libraries(${AVAPACK_OUT_NAME} PRIVATE avalang)

target_include_directories(${AVAPACK_OUT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${AVAPACK_TINY_AES_DIR}
    ${CMAKE_SOURCE_DIR}/runtime/avalang
    ${CMAKE_SOURCE_DIR}/runtime/avalang/src
    ${CMAKE_SOURCE_DIR}/runtime/avalang/api/src
    ${CMAKE_SOURCE_DIR}/runtime/avalang/api/include
)
target_compile_definitions(${AVAPACK_OUT_NAME} PRIVATE ${AVAPACK_TINY_AES_DEFS})

# Igual que ava_cli: en Windows no hay RPATH, asi que el .exe empacado
# necesita avalang.dll en su misma carpeta para poder arrancar.
set_target_properties(${AVAPACK_OUT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/runtime/avalang"
)

# Cuando avalang/avalang_ui son IMPORTED (AVA_PACK_USE_PREBUILT_AVALANG=ON,
# ver CMakeLists.txt raiz), no hay ningun paso de compilacion que las deje
# en RUNTIME_OUTPUT_DIRECTORY -- hay que copiarlas a mano junto a avapack_gen
# y al .exe empacado para que ambos puedan arrancar. En el flujo normal
# (avalang/avalang_ui compiladas desde fuente en este mismo build_pack) esto
# es un no-op: ya estan en la misma carpeta gracias a su propio
# RUNTIME_OUTPUT_DIRECTORY/ARCHIVE_OUTPUT_DIRECTORY, copy_if_different no
# hace nada.
if(AVA_PACK_USE_PREBUILT_AVALANG)
    add_custom_command(TARGET avapack_gen POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:avalang> $<TARGET_FILE:avalang_ui>
                $<TARGET_FILE_DIR:avapack_gen>
        COMMENT "avapack: copiando avalang.dll/avalang_ui.dll (prebuilt) junto a avapack_gen.exe"
    )
    add_custom_command(TARGET ${AVAPACK_OUT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:avalang> $<TARGET_FILE:avalang_ui>
                $<TARGET_FILE_DIR:${AVAPACK_OUT_NAME}>
        COMMENT "avapack: copiando avalang.dll/avalang_ui.dll (prebuilt) junto al ejecutable empacado"
    )
endif()
