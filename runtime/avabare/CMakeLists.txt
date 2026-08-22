add_library(ava_barekernel_runner STATIC
    src/ava_barekernel_runner.cpp
)

target_include_directories(ava_barekernel_runner PRIVATE
    ${CMAKE_SOURCE_DIR}/runtime/avalang/api/include
    ${CMAKE_SOURCE_DIR}/runtime/avalang/platform/barekernel/bindings/target
)

target_compile_options(ava_barekernel_runner PRIVATE
    -ffreestanding
    -fno-exceptions
    -fno-rtti
    -nostdlib
)

target_compile_definitions(ava_barekernel_runner PRIVATE
    AVA_TARGET_BAREKERNEL
    AVA_BAREKERNEL_TARGET_BINDING
    CKM_CAP_DYNAMIC_LOADING=1
    CKM_CAP_THREADS=0
    CKM_CAP_MUTEX=0
    CKM_CAP_ENVVARS=1
    CKM_CAP_DIR_ENUM=1
    CKM_CAP_COLOR=1
    CKM_CAP_PROCESS_EXEC=0
    CKM_CAP_TIMERS=0
    CKM_CAP_STD_EXCEPTIONS=0
    CKM_CAP_LIBSTDCPP=0
)

# Fase 8 -- Optimizacion: solo dead-code elimination (-ffunction-sections/
# -fdata-sections) + symbol stripping; NO LTO aqui. Este target compila con
# el cross-compiler i686-elf-g++ freestanding (-ffreestanding -nostdlib), un
# toolchain especial que en este entorno ni siquiera esta disponible para
# probar (ver docs/kernel/binding-status.md) -- forzar IPO sobre eso es un
# riesgo que no vale la pena para un target ya de por si experimental. El
# tamano del binario si importa aca (es lo que corre dentro del kernel), por
# eso dead-code elimination se mantiene.
ava_apply_optimization_options(ava_barekernel_runner NO_LTO)
