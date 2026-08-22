# BareKernel PAL backend

BareKernel is the kernel-agnostic PAL adapter for custom kernels and OS
runtimes. The adapter depends only on `ckm_contract.h` and the compile-time
capabilities from `BareKernelCaps.h`.

A concrete target belongs under:

```text
bindings/<target>/
```

and must provide at least:

```text
binding.cmake
BareKernelBindingCaps.h
ckm_binding.cpp
```

Architecture-specific syscall stubs are kept inside the binding.

## Build selection

```text
AVA_TARGET_BAREKERNEL=ON
AVA_BAREKERNEL_TARGET=<target>
```

The target's CMake toolchain is responsible for selecting the cross compiler,
assembler, linker and architecture flags.

## Important

This adapter is not a substitute for the C++ runtime required by AvaLang. The
current PAL interfaces use STL types and the runtime uses C++ facilities such
as smart pointers, callbacks and exceptions. A target must provide a compatible
C++ runtime/standard library, or the AvaLang runtime must first be refactored
for that environment.
