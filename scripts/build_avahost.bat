@echo off
setlocal enabledelayedexpansion

REM Este script vive en scripts\ (ver AVALAND_STRUCT.md); nos
REM movemos a la raiz del repo (un nivel arriba) para que las rutas
REM relativas (CMakeLists.txt, third_party\, build\, vcpkg\) sigan
REM funcionando sin importar desde donde se invoque este .bat.
cd /d "%~dp0.."

REM =====================================================================
REM AvaHost build script (Windows)
REM
REM Configures + builds the whole project with AVA_BUILD_AVAHOST=ON, so
REM you get avalang.dll, ava_cli AND avahost.exe. Uses its own build
REM directory (build_avahost\) so it never touches build\ or build_studio\.
REM
REM Usage:
REM   build_avahost.bat                 build Release with the default generator
REM   build_avahost.bat debug           build Debug instead
REM   build_avahost.bat clean           delete build_avahost\ and exit
REM   build_avahost.bat ninja           use Ninja instead of Visual Studio/MSBuild
REM   build_avahost.bat run             after a successful build, launch avahost.exe (run)
REM
REM Flags can be combined, e.g.:  build_avahost.bat clean debug run
REM
REM The first time you configure with AVA_BUILD_AVAHOST=ON, CMake fetches
REM nlohmann/json via FetchContent -- needs internet and git on PATH that
REM one time, then it's cached in build_avahost\_deps\.
REM
REM If VCPKG_ROOT is set (i.e. you already ran install.bat), its toolchain
REM is picked up automatically so the real ANTLR4 frontend builds too --
REM otherwise avahost.exe still builds and runs fine, it just falls back
REM to the stub frontend for ava_compile()/ava_run() (see README.md).
REM =====================================================================

set "BUILD_DIR=build_avahost"
set "BUILD_TYPE=Release"
set "CLEAN=0"
set "GENERATOR=Visual Studio 17 2022"
set "USE_NINJA=0"
set "RUN_AFTER=0"
set "OTHER_FLAG=0"

:parse_args
if "%~1"=="" goto args_done
if /I "%~1"=="clean"  set "CLEAN=1"
if /I "%~1"=="debug"  (set "BUILD_TYPE=Debug" & set "OTHER_FLAG=1")
if /I "%~1"=="ninja"  (set "USE_NINJA=1" & set "OTHER_FLAG=1")
if /I "%~1"=="run"    (set "RUN_AFTER=1" & set "OTHER_FLAG=1")
if /I "%~1"=="help"   goto show_help
if /I "%~1"=="/?"     goto show_help
shift
goto parse_args
:args_done

if "%CLEAN%"=="1" if "%OTHER_FLAG%"=="0" (
    if exist "%BUILD_DIR%" (
        echo Cleaning %BUILD_DIR% ...
        rmdir /s /q "%BUILD_DIR%"
        echo Done -- %BUILD_DIR%\ removed.
    ) else (
        echo Nothing to clean -- %BUILD_DIR%\ doesn't exist.
    )
    endlocal
    exit /b 0
)

where cmake >nul 2>nul
if errorlevel 1 (
    echo [ERROR] cmake was not found on PATH.
    echo         Install it from https://cmake.org/download/ and re-run.
    exit /b 1
)

if "%CLEAN%"=="1" (
    if exist "%BUILD_DIR%" (
        echo Cleaning %BUILD_DIR% ...
        rmdir /s /q "%BUILD_DIR%"
    )
)

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

set "CMAKE_CONFIGURE_ARGS=-DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DAVA_BUILD_AVAHOST=ON"

if defined VCPKG_ROOT (
    echo Using vcpkg toolchain: %VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
    set "CMAKE_CONFIGURE_ARGS=!CMAKE_CONFIGURE_ARGS! -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"
    if defined AVA_VCPKG_TRIPLET (
        echo Using vcpkg triplet: %AVA_VCPKG_TRIPLET%
        set "CMAKE_CONFIGURE_ARGS=!CMAKE_CONFIGURE_ARGS! -DVCPKG_TARGET_TRIPLET=%AVA_VCPKG_TRIPLET%"
    )
) else (
    echo [INFO] VCPKG_ROOT is not set. avahost.exe still builds fine, but
    echo        ava_compile^(^)/ava_run^(^) will hit the stub-frontend error
    echo        instead of really parsing your .ava/.avaui files. Run
    echo        install.bat once to fix that.
)

echo.
if "%USE_NINJA%"=="1" (
    where ninja >nul 2>nul
    if errorlevel 1 (
        echo [ERROR] ninja.exe not found on PATH but "ninja" was requested.
        exit /b 1
    )
    echo Configuring with Ninja ^(%BUILD_TYPE%^) ...
    cmake -S . -B "%BUILD_DIR%" -G "Ninja" %CMAKE_CONFIGURE_ARGS%
) else (
    echo Configuring with "%GENERATOR%" ...
    cmake -S . -B "%BUILD_DIR%" -G "%GENERATOR%" -A x64 %CMAKE_CONFIGURE_ARGS%
)

if errorlevel 1 (
    echo [ERROR] CMake configure step failed. See output above.
    exit /b 1
)

echo.
echo Building ^(%BUILD_TYPE%^) ...
cmake --build "%BUILD_DIR%" --config %BUILD_TYPE% --target avahost --parallel
if errorlevel 1 (
    echo [ERROR] Build failed. See output above.
    exit /b 1
)

REM Multi-config generators (Visual Studio) put the exe under
REM <build>\avahost\<Config>\; single-config ones (Ninja) put it under
REM <build>\avahost\ directly.
set "AVAHOST_EXE=%BUILD_DIR%\runtime\avahost\%BUILD_TYPE%\avahost.exe"
if not exist "%AVAHOST_EXE%" set "AVAHOST_EXE=%BUILD_DIR%\runtime\avahost\avahost.exe"

for %%F in ("%AVAHOST_EXE%") do set "AVAHOST_EXE_DIR=%%~dpF"

REM avahost.exe LINKS avalang/avalang_ui directly (Stable C API), so
REM when AVA_BUILD_SHARED=ON (the default) those are actual .dlls it
REM needs sitting right next to it to even start -- same reasoning as
REM build_studio.bat's copy step for ava_studio.exe. CMake doesn't do
REM this copy on its own, so it's done here instead, with the same
REM "<build>\<target-subdir>\<Config>\ vs <build>\<target-subdir>\"
REM fallback as AVAHOST_EXE above, since avalang/avalang_ui land in
REM their own subdirectory of build_avahost\, not next to avahost.exe.
set "AVALANG_DLL=%BUILD_DIR%\runtime\avalang\%BUILD_TYPE%\avalang.dll"
if not exist "%AVALANG_DLL%" set "AVALANG_DLL=%BUILD_DIR%\runtime\avalang\avalang.dll"

REM avalang_ui's own CMakeLists.txt (runtime\avaui\CMakeLists.txt)
REM deliberately redirects its RUNTIME_OUTPUT_DIRECTORY to
REM runtime\avalang\<Config>\ -- the SAME folder as avalang.dll, not
REM runtime\avaui\<Config>\ -- specifically so avalang.dll's own load-time
REM dependency on avalang_ui.dll resolves without RPATH on Windows. Look
REM for it there (matching AVALANG_DLL above), not under runtime\avaui\.
set "AVAUI_DLL=%BUILD_DIR%\runtime\avalang\%BUILD_TYPE%\avalang_ui.dll"
if not exist "%AVAUI_DLL%" set "AVAUI_DLL=%BUILD_DIR%\runtime\avalang\avalang_ui.dll"

echo.
echo Copying avalang.dll / avalang_ui.dll next to avahost.exe ...
if exist "%AVALANG_DLL%" (
    copy /Y "%AVALANG_DLL%" "%AVAHOST_EXE_DIR%" >nul
) else (
    echo [WARN] avalang.dll not found at %AVALANG_DLL% -- was AVA_BUILD_SHARED left ON?
    echo        If it's OFF, avalang is linked straight into avahost.exe and this is expected.
)
if exist "%AVAUI_DLL%" (
    copy /Y "%AVAUI_DLL%" "%AVAHOST_EXE_DIR%" >nul
) else (
    echo [WARN] avalang_ui.dll not found at %AVAUI_DLL%
)

echo.
echo =====================================================================
echo Build succeeded.
echo avahost.exe: %AVAHOST_EXE%
echo runtime dlls: %AVAHOST_EXE_DIR%avalang.dll
echo               %AVAHOST_EXE_DIR%avalang_ui.dll
echo =====================================================================

if "%RUN_AFTER%"=="1" (
    if exist "%AVAHOST_EXE%" (
        echo.
        echo Launching avahost.exe run ...
        pushd "%~dp0.."
        "%AVAHOST_EXE%" run
        popd
    ) else (
        echo [WARN] Expected avahost.exe at %AVAHOST_EXE% but it's not there.
    )
)

endlocal
exit /b 0

:show_help
echo Usage: build_avahost.bat [clean] [debug] [ninja] [run]
echo   clean   alone: delete build_avahost\ and exit, nothing else
echo           combined with debug/ninja/run: wipe build_avahost\ first, then build
echo   debug   build Debug instead of Release
echo   ninja   use the Ninja generator instead of Visual Studio/MSBuild
echo   run     launch "avahost.exe run" after a successful build
exit /b 0
