@echo off
setlocal enabledelayedexpansion

REM Este script vive en scripts\ (ver AVALAND_STRUCT.md); nos
REM movemos a la raiz del repo (un nivel arriba) para que las rutas
REM relativas (CMakeLists.txt, third_party\, build\, vcpkg\) sigan
REM funcionando sin importar desde donde se invoque este .bat.
cd /d "%~dp0.."

REM =====================================================================
REM Ava Studio build script (Windows)
REM
REM Configures + builds the whole project with AVA_BUILD_STUDIO=ON, so
REM you get avalang.dll, ava_cli, ava_studio.exe AND its plugins
REM (plugins\ai_agent.dll, plugins\hello_world.dll -- these are .dll's
REM loaded at runtime via LoadLibrary, never linked into ava_studio.exe,
REM so they need to be requested as their own --target alongside
REM ava_studio or they silently don't get built). Uses its own build
REM directory (build_studio\) so it never touches or reconfigures your
REM regular build\ from build.bat.
REM
REM Usage:
REM   build_studio.bat                build Release with the default generator
REM   build_studio.bat debug           build Debug instead
REM   build_studio.bat clean            delete build_studio\ and exit -- nothing
REM                                     else, no configure/build (see below)
REM   build_studio.bat ninja            use Ninja instead of Visual Studio/MSBuild
REM                                     (requires ninja.exe on PATH)
REM   build_studio.bat run              after a successful build, launch ava_studio.exe
REM
REM Flags can be combined, e.g.:  build_studio.bat clean debug run
REM "clean" alone (no other flag) just wipes build_studio\ and stops there --
REM handy when you only want to reclaim disk space or force a from-scratch
REM CMake reconfigure later. Combine it with another flag (debug/ninja/run)
REM to wipe build_studio\ first and then continue into a normal build, same
REM as before.
REM
REM The first time you configure with AVA_BUILD_STUDIO=ON, CMake fetches
REM GLFW and Dear ImGui (docking branch) via FetchContent -- needs
REM internet and git on PATH that one time, then they're cached in
REM build_studio\_deps\.
REM
REM If VCPKG_ROOT is set (i.e. you already ran install.bat), its
REM toolchain is picked up automatically so the real ANTLR4 frontend
REM builds too -- otherwise ava_studio still builds and runs fine, it
REM just falls back to the stub frontend when you press Run (F5) on a
REM script (see README.md).
REM =====================================================================

set "BUILD_DIR=build_studio"
set "BUILD_TYPE=Release"
set "CLEAN=0"
set "GENERATOR=Visual Studio 17 2022"
set "USE_NINJA=0"
set "RUN_AFTER=0"
REM Set when debug/ninja/run is explicitly passed, so a bare "clean" (no
REM other flag) can be told apart from "clean" combined with one of them --
REM see the clean-only branch right after argument parsing below.
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

set "CMAKE_CONFIGURE_ARGS=-DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DAVA_BUILD_STUDIO=ON"

if defined VCPKG_ROOT (
    echo Using vcpkg toolchain: %VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
    REM NOTE: inside a parenthesized block, %VAR% expands ONCE at block
    REM entry, not after each "set". Must use !VAR! (delayed expansion,
    REM already enabled via setlocal above) so each append actually sees
    REM the previous one's result.
    set "CMAKE_CONFIGURE_ARGS=!CMAKE_CONFIGURE_ARGS! -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"
    if defined AVA_VCPKG_TRIPLET (
        echo Using vcpkg triplet: %AVA_VCPKG_TRIPLET%
        set "CMAKE_CONFIGURE_ARGS=!CMAKE_CONFIGURE_ARGS! -DVCPKG_TARGET_TRIPLET=%AVA_VCPKG_TRIPLET%"
    )

    REM --- libcurl for the ai_agent plugin ---------------------------------
    REM Self-healing, same reasoning as install_studio.bat: si VCPKG_ROOT
    REM quedo seteado por una corrida vieja de install.bat (de antes de
    REM que el plugin ai_agent existiera), curl nunca se instalo -- y
    REM como este es el script que la gente corre directo (sin pasar por
    REM install_studio.bat), el chequeo tiene que estar ACA tambien, no
    REM solo ahi, o el configure de CMake sigue fallando con "Could NOT
    REM find CURL" antes de que nadie vea ese otro mensaje.
    if exist "%VCPKG_ROOT%\vcpkg.exe" (
        set "AVA_CURL_TRIPLET=%AVA_VCPKG_TRIPLET%"
        if not defined AVA_CURL_TRIPLET set "AVA_CURL_TRIPLET=x64-windows-static-md"
        echo Checking curl:!AVA_CURL_TRIPLET! via vcpkg ^(needed by ai_agent^) ...
        "%VCPKG_ROOT%\vcpkg.exe" install curl:!AVA_CURL_TRIPLET!
        if errorlevel 1 (
            echo [ERROR] "vcpkg install curl:!AVA_CURL_TRIPLET!" failed. See output above.
            exit /b 1
        )
    )
) else (
    echo [INFO] VCPKG_ROOT is not set. ava_studio.exe still builds fine, but
    echo        Run ^(F5^) inside it will hit the stub-frontend error instead
    echo        of really parsing your script, and the ai_agent plugin will
    echo        fail to configure ^(it needs curl from vcpkg^). Run install.bat
    echo        once to fix both, or just keep using the demo Preview tree
    echo        for now.
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
REM ava_studio.exe no depende de los plugins en CMake (son .dll/.so
REM cargados en runtime via LoadLibrary/dlopen, ver plugin_host.cpp --
REM nunca se linkean) asi que "--target ava_studio" solo por si mismo
REM NUNCA los compila, aunque su add_subdirectory este mas abajo en el
REM mismo CMakeLists.txt. Hay que pedirlos explicitamente ademas del
REM exe, o el panel del agente de IA (ai_agent.dll) simplemente no
REM aparece -- ni error ni warning, Ava Studio arranca igual sin esa
REM pestaña (ver PLUGIN_SYSTEM_FASE0.md, "Si el .dll no esta en
REM plugins/ ... arranca igual, sin esa pestaña").
cmake --build "%BUILD_DIR%" --config %BUILD_TYPE% --target ava_studio --target ai_agent_plugin --target hello_world_plugin --parallel
if errorlevel 1 (
    echo [ERROR] Build failed. See output above.
    exit /b 1
)

REM Multi-config generators (Visual Studio) put the exe under
REM <build>\studio\<Config>\; single-config ones (Ninja) put it under
REM <build>\studio\ directly.
set "STUDIO_EXE=%BUILD_DIR%\runtime\avastudio\%BUILD_TYPE%\ava_studio.exe"
if not exist "%STUDIO_EXE%" set "STUDIO_EXE=%BUILD_DIR%\runtime\avastudio\ava_studio.exe"

for %%F in ("%STUDIO_EXE%") do set "STUDIO_EXE_DIR=%%~dpF"
set "PLUGINS_DIR=%STUDIO_EXE_DIR%plugins"

REM ava_studio.exe LINKS avalang/avalang_ui directly (unlike the
REM plugins above, which are dlopen'd by path at runtime, not linked)
REM -- when AVA_BUILD_SHARED=ON (the default) those are actual .dlls
REM the .exe needs sitting right next to it to even start, the same
REM way plugins\*.dll needs to sit next to it to be found by
REM PluginHost::LoadAll. CMake doesn't do this copy on its own (only
REM the plugins' own CMakeLists.txt has a POST_BUILD step for that),
REM so it's done here instead, same "<build>\<target-subdir>\<Config>\
REM vs <build>\<target-subdir>\" fallback as STUDIO_EXE above, since
REM avalang/avalang_ui land in their own subdirectory of build_studio\,
REM not next to ava_studio.exe.
set "AVALANG_DLL=%BUILD_DIR%\runtime\avalang\%BUILD_TYPE%\avalang.dll"
if not exist "%AVALANG_DLL%" set "AVALANG_DLL=%BUILD_DIR%\runtime\avalang\avalang.dll"

set "AVAUI_DLL=%BUILD_DIR%\runtime\avaui\%BUILD_TYPE%\avalang_ui.dll"
if not exist "%AVAUI_DLL%" set "AVAUI_DLL=%BUILD_DIR%\runtime\avaui\avalang_ui.dll"

echo.
echo Copying avalang.dll / avalang_ui.dll next to ava_studio.exe ...
if exist "%AVALANG_DLL%" (
    copy /Y "%AVALANG_DLL%" "%STUDIO_EXE_DIR%" >nul
) else (
    echo [WARN] avalang.dll not found at %AVALANG_DLL% -- was AVA_BUILD_SHARED left ON?
    echo        If it's OFF, avalang is linked straight into ava_studio.exe and this is expected.
)
if exist "%AVAUI_DLL%" (
    copy /Y "%AVAUI_DLL%" "%STUDIO_EXE_DIR%" >nul
) else (
    echo [WARN] avalang_ui.dll not found at %AVAUI_DLL%
)

set "LIBRARIES_DIR=%~dp0..\libraries"
set "MODULES_DIR=%STUDIO_EXE_DIR%modules"
if exist "%LIBRARIES_DIR%" (
    echo.
    echo Copying libraries\ to %MODULES_DIR% ...
    if not exist "%MODULES_DIR%" mkdir "%MODULES_DIR%"
    robocopy "%LIBRARIES_DIR%" "%MODULES_DIR%" /MIR /NFL /NDL /NJH /NJS >nul
)

echo.
echo =====================================================================
echo Build succeeded.
echo ava_studio.exe: %STUDIO_EXE%
echo runtime dlls:   %STUDIO_EXE_DIR%avalang.dll
echo                 %STUDIO_EXE_DIR%avalang_ui.dll
echo plugins:        %PLUGINS_DIR%\ai_agent.dll
echo                 %PLUGINS_DIR%\hello_world.dll
echo modules:        %MODULES_DIR%
echo =====================================================================

if "%RUN_AFTER%"=="1" (
    if exist "%STUDIO_EXE%" (
        echo.
        echo Launching ava_studio.exe ...
        REM Run from the repo root (not build_studio\) so the Explorer
        REM panel's default "scripts" path (relative to the working
        REM directory) resolves to .\scripts\ like ava_cli's examples.
        pushd "%~dp0.."
        start "" "%STUDIO_EXE%"
        popd
    ) else (
        echo [WARN] Expected ava_studio.exe at %STUDIO_EXE% but it's not there.
    )
)

endlocal
exit /b 0

:show_help
echo Usage: build_studio.bat [clean] [debug] [ninja] [run]
echo   clean   alone: delete build_studio\ and exit, nothing else
echo           combined with debug/ninja/run: wipe build_studio\ first, then build
echo   debug   build Debug instead of Release
echo   ninja   use the Ninja generator instead of Visual Studio/MSBuild
echo   run     launch ava_studio.exe after a successful build
exit /b 0