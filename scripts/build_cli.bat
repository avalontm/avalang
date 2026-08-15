@echo off
setlocal enabledelayedexpansion

REM Este script vive en scripts\ (ver AVALAND_STRUCT.md); nos
REM movemos a la raiz del repo (un nivel arriba) para que las rutas
REM relativas (CMakeLists.txt, third_party\, build\, vcpkg\) sigan
REM funcionando sin importar desde donde se invoque este .bat.
cd /d "%~dp0.."

REM =====================================================================
REM ava_cli build script (Windows)
REM
REM Configura y pide como target explicito SOLO ava_cli.exe -- ni avahost, ni
REM ava_studio, ni avalang_ui.dll se compilan (todos OFF/no-target por
REM defecto). avalang.dll no se pide como --target aparte: es dependencia de
REM link de ava_cli, asi que CMake la compila sola si hace falta (primera
REM vez, o fuentes desactualizadas) y no la toca si ya esta al dia en
REM build_cli\ -- no se fuerza una recompilacion de avalang en cada corrida.
REM Si avalang_ui.dll ya existe en build_cli\ por otra razon, se
REM copia junto a ava_cli.exe (ver copy step mas abajo), pero este script
REM no la requiere ni la fuerza a compilar. Usa su propia carpeta de build
REM (build_cli\) para no tocar build\, build_avahost\, build_studio\ ni
REM build_avaui\.
REM
REM Uso:
REM   build_cli.bat                 build Release con el generador por defecto
REM   build_cli.bat debug           build Debug en vez de Release
REM   build_cli.bat clean           borra build_cli\ y termina
REM   build_cli.bat ninja           usa Ninja en vez de Visual Studio/MSBuild
REM   build_cli.bat run <script>    despues de compilar, corre ava_cli.exe <script>
REM
REM Los flags se pueden combinar, ej.:  build_cli.bat clean debug
REM
REM Si VCPKG_ROOT esta definida (ya corriste install.bat), su toolchain
REM se usa automaticamente para que el frontend real de ANTLR4 compile --
REM si no, ava_cli.exe igual compila y corre, solo que ava_compile()
REM cae al frontend stub (ver README.md).
REM =====================================================================

set "BUILD_DIR=build_cli"
set "BUILD_TYPE=Release"
set "CLEAN=0"
set "GENERATOR=Visual Studio 17 2022"
set "USE_NINJA=0"
set "RUN_AFTER=0"
set "RUN_SCRIPT="
set "OTHER_FLAG=0"

:parse_args
if "%~1"=="" goto args_done
if /I "%~1"=="clean"  set "CLEAN=1"
if /I "%~1"=="debug"  (set "BUILD_TYPE=Debug" & set "OTHER_FLAG=1")
if /I "%~1"=="ninja"  (set "USE_NINJA=1" & set "OTHER_FLAG=1")
if /I "%~1"=="help"   goto show_help
if /I "%~1"=="/?"     goto show_help
if /I "%~1"=="run" (
    set "RUN_AFTER=1"
    set "OTHER_FLAG=1"
    shift
    if not "%~1"=="" set "RUN_SCRIPT=%~1"
    goto parse_args_noshift
)
shift
goto parse_args
:parse_args_noshift
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

REM AVA_BUILD_CLI ya es ON por defecto (ver CMakeLists.txt raiz); lo
REM pasamos explicito igual por claridad. avahost/studio/ui se quedan
REM en su default OFF, asi que no hace falta red para FetchContent.
set "CMAKE_CONFIGURE_ARGS=-DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DAVA_BUILD_CLI=ON"

if defined VCPKG_ROOT (
    echo Using vcpkg toolchain: %VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
    set "CMAKE_CONFIGURE_ARGS=!CMAKE_CONFIGURE_ARGS! -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"
    if defined AVA_VCPKG_TRIPLET (
        echo Using vcpkg triplet: %AVA_VCPKG_TRIPLET%
        set "CMAKE_CONFIGURE_ARGS=!CMAKE_CONFIGURE_ARGS! -DVCPKG_TARGET_TRIPLET=%AVA_VCPKG_TRIPLET%"
    )
) else (
    echo [INFO] VCPKG_ROOT is not set. ava_cli.exe still builds fine, but
    echo        ava_compile^(^)/ava_run^(^) will hit the stub-frontend error
    echo        instead of really parsing your .ava files. Run install.bat
    echo        once to fix that.
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
REM Solo pedimos el target ava_cli. avalang NO se pasa como --target aparte:
REM ava_cli lo linkea (target_link_libraries PRIVATE avalang en
REM runtime/avacli/CMakeLists.txt), asi que CMake ya lo arma en el grafo de
REM dependencias y lo compila solo -- primera vez que no existe, o si sus
REM fuentes cambiaron. Si avalang.dll/.lib ya estan al dia en build_cli\, no
REM se vuelve a compilar; MSBuild/Ninja lo detectan solos sin que lo forcemos
REM con un --target separado.
REM avalang_ui tampoco se agrega como target -- ava_cli.exe no lo linkea ni
REM lo necesita para correr, asi que no forzamos su compilacion aqui.
cmake --build "%BUILD_DIR%" --config %BUILD_TYPE% --target ava_cli --parallel
if errorlevel 1 (
    echo [ERROR] Build failed. See output above.
    exit /b 1
)

REM ava_cli.exe se coloca junto a avalang.dll a proposito (ver
REM runtime/avacli/CMakeLists.txt -- RUNTIME_OUTPUT_DIRECTORY), asi que
REM un solo path sirve para ambos generadores multi-config y single-config.
set "AVA_CLI_EXE=%BUILD_DIR%\runtime\avalang\%BUILD_TYPE%\ava_cli.exe"
if not exist "%AVA_CLI_EXE%" set "AVA_CLI_EXE=%BUILD_DIR%\runtime\avalang\ava_cli.exe"

for %%F in ("%AVA_CLI_EXE%") do set "AVA_CLI_EXE_DIR=%%~dpF"

REM ava_cli.exe no linkea avalang_ui y esta build no lo compila a
REM proposito (ver arriba) -- esto solo copia la dll si ya quedo compilada
REM ahi por otra razon (ej. alguien corrio build_avaui.bat apuntando a esta
REM misma carpeta, o dejo AVA_BUILD_UI/otro target prendido antes). Si no
REM esta, no pasa nada, sin warning -- no es un requisito de este script.
set "AVAUI_DLL=%BUILD_DIR%\runtime\avaui\%BUILD_TYPE%\avalang_ui.dll"
if not exist "%AVAUI_DLL%" set "AVAUI_DLL=%BUILD_DIR%\runtime\avaui\avalang_ui.dll"
if exist "%AVAUI_DLL%" (
    echo Copying avalang_ui.dll next to ava_cli.exe ...
    copy /Y "%AVAUI_DLL%" "%AVA_CLI_EXE_DIR%" >nul
)

REM Copia las librerias empaquetadas en libraries\ (mysql, etc.) a
REM modules\<nombre>\ junto a ava_cli.exe, el mismo lugar que
REM ModulesDirNextToExecutable() (main.cpp) le pasa a SetStdlibPath.
REM AvaStudio resuelve import mysql porque tiene SU PROPIA carpeta
REM modules\ junto a AvaStudio.exe (build_studio\...) -- build_cli\
REM tiene la suya aparte, asi que hay que copiar aca tambien.
if exist "libraries" (
    echo Copying libraries\ into modules\ next to ava_cli.exe ...
    if not exist "%AVA_CLI_EXE_DIR%modules" mkdir "%AVA_CLI_EXE_DIR%modules"
    xcopy /Y /E /I /Q "libraries\*" "%AVA_CLI_EXE_DIR%modules\" >nul
)

echo.
echo =====================================================================
echo Build succeeded.
echo ava_cli.exe: %AVA_CLI_EXE%
echo =====================================================================

if "%RUN_AFTER%"=="1" (
    if exist "%AVA_CLI_EXE%" (
        echo.
        if defined RUN_SCRIPT (
            echo Running ava_cli.exe %RUN_SCRIPT% ...
            "%AVA_CLI_EXE%" "%RUN_SCRIPT%"
        ) else (
            echo No script given after "run" -- showing usage:
            "%AVA_CLI_EXE%"
        )
    ) else (
        echo [WARN] Expected ava_cli.exe at %AVA_CLI_EXE% but it's not there.
    )
)

endlocal
exit /b 0

:show_help
echo Usage: build_cli.bat [clean] [debug] [ninja] [run ^<script.ava^>]
echo   clean   alone: delete build_cli\ and exit, nothing else
echo           combined with debug/ninja/run: wipe build_cli\ first, then build
echo   debug   build Debug instead of Release
echo   ninja   use the Ninja generator instead of Visual Studio/MSBuild
echo   run ^<script.ava^>   after a successful build, run ava_cli.exe against that script
echo.
echo Once built, ava_cli.exe also has its own "build" subcommand to pack a whole
echo AvaLang project into a single .exe (avapack, Fase 2) -- run it with no args
echo or see runtime/avacli/README.md for flags:
echo   ava_cli.exe build --project ^<dir^> --entry ^<archivo.ava^> --out ^<exe^>
exit /b 0
