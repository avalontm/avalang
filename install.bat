@echo off
setlocal enabledelayedexpansion

REM =====================================================================
REM AvaLang install.bat
REM
REM Installs everything needed for build.bat to compile the FULL frontend
REM (real ANTLR4 parser, not the stub):
REM   1. Clones + bootstraps vcpkg (unless already present)
REM   2. vcpkg install antlr4 using the x64-windows-static-md triplet
REM   3. Downloads the ANTLR4 tool jar into third_party\ (only needed at
REM      CMake-configure time, to regenerate the C++ parser from the .g4)
REM   4. Persists VCPKG_ROOT / AVA_VCPKG_TRIPLET via setx, then runs
REM      build.bat
REM
REM Usage:
REM   install.bat                  install vcpkg into .\vcpkg, then build
REM   install.bat D:\tools\vcpkg   install vcpkg into a custom path instead
REM   install.bat skipbuild        install only, don't run build.bat after
REM   (both can be combined: install.bat D:\tools\vcpkg skipbuild)
REM
REM WHY x64-windows-static-md and not the plain "x64-windows-static"
REM triplet or the dynamic default ("x64-windows"):
REM   - "x64-windows" (default) builds antlr4-runtime as its OWN .dll.
REM     avalang.dll would then depend on antlr4-runtime.dll being present
REM     at runtime too -- one more file to redistribute alongside it.
REM   - "x64-windows-static" bakes antlr4 into avalang.dll as a single
REM     file (good), but links the STATIC MSVC CRT (/MT). This project's
REM     CMakeLists.txt does not override CMAKE_MSVC_RUNTIME_LIBRARY, so it
REM     builds with the default DYNAMIC CRT (/MD) -- mixing /MT and /MD
REM     libraries in one link step fails with LNK2038 "mismatch detected
REM     for 'RuntimeLibrary'".
REM   - "x64-windows-static-md" bakes antlr4 into avalang.dll as a single
REM     file AND links the dynamic CRT (/MD), matching this project's
REM     default. This is the one that "just works" here.
REM =====================================================================

set "SKIP_BUILD=0"
set "VCPKG_DIR="
for %%A in (%*) do (
    if /I "%%A"=="skipbuild" (
        set "SKIP_BUILD=1"
    ) else (
        set "VCPKG_DIR=%%~A"
    )
)
if not defined VCPKG_DIR set "VCPKG_DIR=%~dp0vcpkg"

set "AVA_TRIPLET=x64-windows-static-md"
set "ANTLR_VERSION=4.13.2"
set "ANTLR_JAR_NAME=antlr-%ANTLR_VERSION%-complete.jar"
set "ANTLR_JAR_URL=https://www.antlr.org/download/%ANTLR_JAR_NAME%"
set "THIRD_PARTY_DIR=%~dp0third_party"

echo =====================================================================
echo AvaLang dependency installer
echo   vcpkg target dir : %VCPKG_DIR%
echo   vcpkg triplet     : %AVA_TRIPLET%
echo   antlr4 jar        : %ANTLR_JAR_NAME%
echo =====================================================================
echo.

REM --- 1. git ------------------------------------------------------------
where git >nul 2>nul
if errorlevel 1 (
    echo [ERROR] git not found on PATH. Install it from https://git-scm.com/ and re-run.
    exit /b 1
)

REM --- 2. Java (only needed to regenerate the parser at configure time) --
where java >nul 2>nul
if errorlevel 1 (
    echo [WARN] java not found on PATH.
    echo        The ANTLR4 tool jar needs a JRE/JDK to run once at CMake
    echo        configure time ^(it only generates C++ parser source files;
    echo        it is NOT a runtime dependency of avalang.dll^).
    echo        Install a JDK ^(e.g. https://adoptium.net/^), make sure
    echo        "java" is on PATH, and re-run this script. Continuing for
    echo        now -- CMake will fall back to the stub frontend if java
    echo        or the jar cannot be found.
    echo.
)

REM --- 3. vcpkg ------------------------------------------------------------
if exist "%VCPKG_DIR%\vcpkg.exe" (
    echo [OK] vcpkg already present at %VCPKG_DIR%
) else (
    echo Cloning vcpkg into %VCPKG_DIR% ...
    git clone https://github.com/microsoft/vcpkg "%VCPKG_DIR%"
    if errorlevel 1 (
        echo [ERROR] git clone of vcpkg failed. See output above.
        exit /b 1
    )
    echo Bootstrapping vcpkg ...
    call "%VCPKG_DIR%\bootstrap-vcpkg.bat" -disableMetrics
    if errorlevel 1 (
        echo [ERROR] vcpkg bootstrap failed. See output above.
        exit /b 1
    )
)

REM --- 4. antlr4 C++ runtime, baked statically into avalang.dll -----------
echo.
echo Installing antlr4:%AVA_TRIPLET% via vcpkg ...
echo ^(builds antlr4-runtime from source the first time -- can take a
echo  few minutes^)
"%VCPKG_DIR%\vcpkg.exe" install antlr4:%AVA_TRIPLET%
if errorlevel 1 (
    echo [ERROR] "vcpkg install antlr4:%AVA_TRIPLET%" failed. See output above.
    exit /b 1
)

REM --- 5. ANTLR4 tool jar (build-time only, regenerates the C++ parser) ---
if not exist "%THIRD_PARTY_DIR%" mkdir "%THIRD_PARTY_DIR%"
set "JAR_FOUND=0"
for %%F in ("%THIRD_PARTY_DIR%\antlr*-complete.jar") do set "JAR_FOUND=1"
if "%JAR_FOUND%"=="1" (
    echo [OK] ANTLR4 tool jar already present in third_party\
) else (
    echo Downloading %ANTLR_JAR_NAME% into third_party\ ...
    powershell -NoProfile -Command ^
        "try { Invoke-WebRequest -Uri '%ANTLR_JAR_URL%' -OutFile '%THIRD_PARTY_DIR%\%ANTLR_JAR_NAME%' } catch { exit 1 }"
    if errorlevel 1 (
        echo [WARN] Could not download the ANTLR4 jar automatically.
        echo        Download it by hand from https://www.antlr.org/download.html
        echo        and place the "...-complete.jar" file in third_party\
        echo        ^(see third_party\README.md^). The build still works
        echo        with the stub frontend without it, just not the real one.
    ) else (
        echo [OK] Saved to third_party\%ANTLR_JAR_NAME%
    )
)

REM --- 6. Persist env vars for future shells, export for this one too ----
setx VCPKG_ROOT "%VCPKG_DIR%" >nul
setx AVA_VCPKG_TRIPLET "%AVA_TRIPLET%" >nul
set "VCPKG_ROOT=%VCPKG_DIR%"
set "AVA_VCPKG_TRIPLET=%AVA_TRIPLET%"

echo.
echo =====================================================================
echo Done.
echo   VCPKG_ROOT set to        : %VCPKG_DIR%
echo   AVA_VCPKG_TRIPLET set to : %AVA_TRIPLET%
echo   ^(setx persists both for NEW terminal windows; this window already
echo    has them exported for the build about to run^)
echo =====================================================================

if "%SKIP_BUILD%"=="1" (
    echo.
    echo Skipping build ^(skipbuild passed^). Run build.bat manually when ready.
    endlocal
    exit /b 0
)

echo.
echo Running build.bat ...
call "%~dp0build.bat"
set "BUILD_RESULT=%errorlevel%"
endlocal
exit /b %BUILD_RESULT%
