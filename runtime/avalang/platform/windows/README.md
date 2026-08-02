# PAL - Windows Backend (Phase 3)

Implements every `core/platform/interfaces/I*.h` interface against Win32.

## Files

| Interface        | Implementation                          |
|-------------------|------------------------------------------|
| IFileSystem       | WinFileSystem.h/.cpp                     |
| IThread / IThreadFactory | WinThread.h/.cpp                  |
| IMutex            | WinMutex.h/.cpp                          |
| IClock            | WinClock.h/.cpp                          |
| ILibraryHandle / ILibraryLoader | WinLibrary.h/.cpp           |
| IConsole          | WinConsole.h/.cpp                        |
| IEnvironment      | WinEnvironment.h/.cpp                    |
| IProcess          | WinProcess.h/.cpp                        |
| IPlatform         | WinPlatform.h/.cpp (aggregator)          |

`WinPlatform.cpp` also defines `ava::platform::Platform::Create()` (declared
in `core/platform/Platform.h`). This is the only place that symbol is
defined for Windows builds -- do not add another Platform::Create()
definition when Linux/macOS backends land, or the link will fail with a
duplicate symbol.

## Link requirements

Add to the target's linked libraries (vcpkg triplet `x64-windows-static-md`
already links most of these transitively via the CRT, but be explicit):

- `Shell32.lib` (WinEnvironment.cpp uses `CommandLineToArgvW`)

Kernel32/User32 are pulled in automatically via `<Windows.h>` on MSVC.

## Notes

- All string APIs used are the `...A` (ANSI/UTF-8-as-codepage) variants
  except command-line parsing, which goes through `...W` + `WideCharToMultiByte`
  because `CommandLineToArgvW` has no ANSI counterpart.
- `WinConsole` sets both input and output console codepage to `CP_UTF8` on
  construction so `IConsole::Write`/`ReadLine` can pass UTF-8 bytes through
  unmodified.
- `WinProcess::Execute` blocks until the child exits and captures stdout/stderr
  fully via anonymous pipes; no streaming/async execution yet.
- Nothing here references AvaLang's compiler/VM/AST types (dependency rule
  from the PAL doc: platform never depends upward).
