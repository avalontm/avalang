#ifndef AVA_PLATFORM_PAL_ABI_H
#define AVA_PLATFORM_PAL_ABI_H

namespace ava {
namespace platform {

// ---------------------------------------------------------------------------
// PAL stability contract (Fase 3 — Platform_Foundation.md / PAL_PROGRESS.md)
// ---------------------------------------------------------------------------
//
// What "stable" means for the interfaces below (IFileSystem, IThread,
// IMutex, IClock, ILibrary, IConsole, IEnvironment, IProcess, IPlatform):
//
//   1. Signature freeze: once an interface is tagged STABLE, no existing
//      virtual method may change its name, parameter list, return type or
//      ordering. Adding a brand-new virtual method to a STABLE interface is
//      also forbidden (it would break every existing override, e.g. Win/Lin/
//      Mac backends) -- new capabilities go in a new interface instead.
//
//   2. Deprecation, not removal: if a method genuinely needs to go away,
//      mark it `// [deprecated since ABI vN]` in the interface comment,
//      keep it functional for at least one full ABI version, and only
//      delete it on the next AVA_PAL_ABI_VERSION bump.
//
//   3. Version bump required for: adding/removing/reordering virtual
//      methods on any STABLE interface, or changing IPlatform's set of
//      accessors. NOT required for: fixing a doc comment, adding a
//      concrete (non-virtual) helper, or touching a backend .cpp without
//      touching the interface header.
//
//   4. Scope: this contract covers the Windows backend only (see "Alcance
//      actual" in PAL_PROGRESS.md). Linux/macOS backends are 📚 in-study
//      stubs and are not required to track ABI bumps until their own work
//      resumes.
//
//   5. Consumers (Extern/FFI in vm_extern.cpp, the future Async runtime,
//      and eventually avalang.ui.dll) are expected to code against a single
//      AVA_PAL_ABI_VERSION and may assume the STABLE interfaces will not
//      shift under them within that version.
//
// The UI service interfaces under interfaces/services/ui/ (IWindow, IMouse,
// IKeyboard, ICursor, IClipboard, IRenderSurface, ITimer, IDisplay,
// IPlatformServices) are explicitly OUT of this contract -- they are Phase 0
// stubs, not yet wired to IPlatform, and remain free to change until Phase 6
// begins.

// v2 (Fase 5, Async Runtime): IPlatform gana el accessor Timer() ->
// ITimer. Por regla 3 de arriba esto es bump de versión, no un cambio
// silencioso: WinPlatform (unico backend activo) implementa Timer() ya
// en v2. Linux/macOS siguen en estudio (regla 4) y no necesitan seguir
// este bump todavia.
#define AVA_PAL_ABI_VERSION 2

} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_PAL_ABI_H
