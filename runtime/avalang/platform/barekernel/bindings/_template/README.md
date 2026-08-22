# BareKernel binding template

Copy this directory to `bindings/<target>/` and replace the placeholders with
the target's real ABI.

Required files for a buildable binding:

- `binding.cmake`
- `BareKernelBindingCaps.h`
- `ckm_binding.cpp`
- architecture-specific syscall/ABI stubs when required

The template is documentation only; it is intentionally not a selectable
build target.

Example `binding.cmake`:

```cmake
set(AVA_BAREKERNEL_BINDING_SOURCES
    "${AVA_BAREKERNEL_BINDING_DIR}/ckm_binding.cpp"
    "${AVA_BAREKERNEL_BINDING_DIR}/<arch>/syscall_stubs.S"
)
```
