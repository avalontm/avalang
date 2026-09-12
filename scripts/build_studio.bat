@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0.."

set "BUILD_DIR=build_studio"
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
        taskkill /F /IM ava_studio.exe >nul 2>nul
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
        taskkill /F /IM ava_studio.exe >nul 2>nul
        rmdir /s /q "%BUILD_DIR%"
    )
)

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

set "CMAKE_CONFIGURE_ARGS=-DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DAVA_BUILD_STUDIO=ON -DAVA_BUILD_PACK=ON -DAVA_ENABLE_LTO=OFF"

if defined VCPKG_ROOT (
    echo Using vcpkg toolchain: %VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
    set "CMAKE_CONFIGURE_ARGS=!CMAKE_CONFIGURE_ARGS! -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"
    if defined AVA_VCPKG_TRIPLET (
        echo Using vcpkg triplet: %AVA_VCPKG_TRIPLET%
        set "CMAKE_CONFIGURE_ARGS=!CMAKE_CONFIGURE_ARGS! -DVCPKG_TARGET_TRIPLET=%AVA_VCPKG_TRIPLET%"
    )

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
set "USE_FAST_BUILD=0"
if "%USE_NINJA%"=="0" (
    set "MSBUILD_EXE="
    if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
        for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -prerelease -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do set "MSBUILD_EXE=%%i"
    )
    if not defined MSBUILD_EXE (
        for /f "delims=" %%i in ('where msbuild 2^>nul') do if not defined MSBUILD_EXE set "MSBUILD_EXE=%%i"
    )
    set "SLN_FILE="
    for /f "delims=" %%s in ('dir /b "%BUILD_DIR%\*.sln" 2^>nul') do if not defined SLN_FILE set "SLN_FILE=%BUILD_DIR%\%%s"
    if defined MSBUILD_EXE if defined SLN_FILE set "USE_FAST_BUILD=1"
)

if "%USE_FAST_BUILD%"=="1" (
    "!MSBUILD_EXE!" "!SLN_FILE!" /p:Configuration=%BUILD_TYPE% /m /nodeReuse:false /t:ava_studio;ai_agent_plugin;hello_world_plugin;ava_cli;avapack_gen;avapack_stub
) else (
    cmake --build "%BUILD_DIR%" --config %BUILD_TYPE% --target ava_studio --target ai_agent_plugin --target hello_world_plugin --target ava_cli --target avapack_gen --target avapack_stub --parallel
)
if errorlevel 1 (
    echo [ERROR] Build failed. See output above.
    exit /b 1
)

set "STUDIO_EXE=%BUILD_DIR%\runtime\avastudio\%BUILD_TYPE%\ava_studio.exe"
if not exist "%STUDIO_EXE%" set "STUDIO_EXE=%BUILD_DIR%\runtime\avastudio\ava_studio.exe"

for %%F in ("%STUDIO_EXE%") do set "STUDIO_EXE_DIR=%%~dpF"
set "PLUGINS_DIR=%STUDIO_EXE_DIR%plugins"

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

set "AVA_CLI_TOOLS_DIR=%BUILD_DIR%\runtime\avalang\%BUILD_TYPE%"
if not exist "%AVA_CLI_TOOLS_DIR%\ava_cli.exe" set "AVA_CLI_TOOLS_DIR=%BUILD_DIR%\runtime\avalang"

echo.
echo =====================================================================
echo Build succeeded.
echo ava_studio.exe: %STUDIO_EXE%
echo runtime dlls:   %STUDIO_EXE_DIR%avalang.dll
echo                 %STUDIO_EXE_DIR%avalang_ui.dll
echo plugins:        %PLUGINS_DIR%\ai_agent.dll
echo                 %PLUGINS_DIR%\hello_world.dll
echo modules:        %MODULES_DIR%
echo ava_cli.exe / avapack_gen.exe / avapack_stub.exe:
echo                 %AVA_CLI_TOOLS_DIR%\ava_cli.exe
echo =====================================================================

if "%RUN_AFTER%"=="1" (
    if exist "%STUDIO_EXE%" (
        echo.
        echo Launching ava_studio.exe ...
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
