# third_party/

Drop an ANTLR4 "complete" jar here (e.g. `antlr-4.13.2-complete.jar`,
downloadable from https://www.antlr.org/download.html) if it is not
already installed system-wide. CMakeLists.txt looks for it here
automatically (in addition to /usr/local/lib and /usr/share/java on
Unix), on both Windows and Linux/macOS.

This jar is only needed at BUILD time, to regenerate the C++ parser from
grammar/AvaLang.g4. It is not a runtime dependency of the compiled
library.

For the actual ANTLR4 C++ runtime library (linked at build time), see
README.md at the project root -- on Windows the simplest path is vcpkg:

    vcpkg install antlr4
    set VCPKG_ROOT=C:\path\to\vcpkg
    build.bat
