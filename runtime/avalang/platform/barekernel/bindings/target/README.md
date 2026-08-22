# BareKernel target binding

This directory is the target-specific adapter between the generic BareKernel
PAL and the supplied kernel ABI.

It intentionally does **not** use the kernel project's name in source file
names or in the generic PAL.

## Current state

The supplied kernel source establishes a complete 32-bit x86 user syscall
entry path:

- `int 0x80`
- syscall number in `EAX`
- arguments in `EBX`, `ECX`, `EDX`, `ESI`, `EDI`
- result in `EAX`

The source also exposes handlers for memory, I/O, process, time, environment,
and dynamic linking.

The kernel project's own `include/syscall/syscall_types.hpp` (`enum
SyscallNumber`) was consulted and `ckm_syscall_numbers.h` now carries the
real values from that enum. If the kernel's enum changes, update
`ckm_syscall_numbers.h` to match -- it is the single place this binding
depends on for numbers.

The following facilities are intentionally disabled because the supplied
kernel does not currently provide a complete implementation:

- kernel threads
- kernel mutexes
- waitpid-backed process execution (`sys_waitpid` is registered but
  hard-coded to return `ENOSYS` in the kernel; there is no `SYS_WAITPID`
  number to call even if the flag were flipped on)
- timer queue
- confirmed C++ exception runtime
- libstdc++

Dynamic loading is enabled because `sys_dlopen`, `sys_dlsym`, `sys_dlclose`
and `sys_dlerror` are registered by the kernel.

## Known semantic differences absorbed by this binding

- **`ckm_readdir` polarity.** The kernel's `sys_readdir` returns `1` for
  "entry read", `0` for "end of directory", and a negative value for error.
  The CKM contract wants the opposite success/end polarity (`0` = entry
  filled, `1` = end). `ckm_syscall.h` translates this so the generic
  BareKernel adapter (Layer B) never has to know about the kernel's
  convention.
- **`sys_spawn` signature.** The kernel's real signature is
  `sys_spawn(path, flags, target_uid)`, not `(path, argv, argc)` as the CKM
  contract expects. `ckm_spawn` currently drops `argv`/`argc` rather than
  silently reinterpreting them as kernel arguments. This is unreachable
  while `CKM_CAP_PROCESS_EXEC=0`; revisit if that flag is ever turned on.

## Build wiring

`binding.cmake` defines `AVA_BAREKERNEL_TARGET_BINDING=1`, which is what
tells `ckm_contract.h` to pull in this binding's `ckm_syscall.h` (the
`static inline` `int 0x80` wrappers) instead of declaring the CKM functions
with no definition anywhere -- omitting that define was the original cause
of an unresolved-symbol link failure.

## Verified

All 9 generic BareKernel adapter files plus this binding's `ckm_syscall.cpp`
were syntax-checked together (`g++ -fsyntax-only`) against the real syscall
numbers, with `AVA_BAREKERNEL_TARGET_BINDING=1` and the capability flags from
`BareKernelCaps_target.h`. Clean, no warnings.

Not yet verified: an actual boot/run against the kernel image, and the
32-bit `int 0x80` code path on real i386 (only cross-checked with
`-D__i386__` on a 64-bit host, since no i686 cross-toolchain was available
in this environment).

## Remaining before this is truly executable

1. Confirm the kernel's syscall-number enum hasn't drifted since this file
   was written (search for `enum SyscallNumber` in the kernel repo).
2. Build with a real i686 cross-compiler (see `docs/kernel/barekernel.md`
   §0) and link against an actual kernel image/QEMU target.
3. Decide the fate of `bindings/litekernel/` (see project discussion) -- its
   capability flags currently disagree with this binding's and it has no
   `ckm_binding`/`ckm_syscall` implementation of its own.
