@echo off
REM clean_avaui.bat -- borra SOLO los artefactos de compilacion de avaui /
REM avalang_ui (obj + dll/lib intermedios) sin tocar el resto de
REM build_studio\ (evita recompilar antlr4/glfw/pugixml/imgui, etc).
REM
REM Uso: cuando el link de avalang_ui.vcxproj falla con LNK2001/LNK2019
REM en simbolos que SI existen en el .cpp (p.ej. BaseRenderer::DrawButton,
REM RenderCommandSink::DrawButton...), es casi siempre una cache
REM incremental de MSBuild desincronizada: algunos .obj de avaui quedaron
REM viejos (de antes de un cambio de firma) y el linker mezcla objetos
REM viejos con nuevos. Este script fuerza que TODO avaui se recompile
REM en el siguiente build_studio.bat, sin tocar el resto del arbol.

setlocal
cd /d "%~dp0.."
set "BUILD_DIR=build_studio"

if not exist "%BUILD_DIR%" (
    echo Nothing to clean -- %BUILD_DIR%\ doesn't exist.
    exit /b 0
)

echo Cleaning avaui build artifacts under %BUILD_DIR% ...

REM Carpeta de intermediate objects de avalang_ui.vcxproj (avaui.dir\...)
if exist "%BUILD_DIR%\runtime\avaui" (
    for /d %%D in ("%BUILD_DIR%\runtime\avaui\avalang_ui.dir") do rmdir /s /q "%%D" 2>nul
)

REM DLL/LIB/EXP/PDB finales de avalang_ui, que viven junto a avalang.dll
if exist "%BUILD_DIR%\runtime\avalang" (
    del /q "%BUILD_DIR%\runtime\avalang\Release\avalang_ui.dll" 2>nul
    del /q "%BUILD_DIR%\runtime\avalang\Release\avalang_ui.lib" 2>nul
    del /q "%BUILD_DIR%\runtime\avalang\Release\avalang_ui.exp" 2>nul
    del /q "%BUILD_DIR%\runtime\avalang\Release\avalang_ui.pdb" 2>nul
    del /q "%BUILD_DIR%\runtime\avalang\Debug\avalang_ui.dll" 2>nul
    del /q "%BUILD_DIR%\runtime\avalang\Debug\avalang_ui.lib" 2>nul
    del /q "%BUILD_DIR%\runtime\avalang\Debug\avalang_ui.exp" 2>nul
    del /q "%BUILD_DIR%\runtime\avalang\Debug\avalang_ui.pdb" 2>nul
)

REM .tlog de avalang_ui, para que MSBuild no crea que sigue "up to date"
if exist "%BUILD_DIR%\runtime\avaui\avalang_ui.dir" rmdir /s /q "%BUILD_DIR%\runtime\avaui\avalang_ui.dir" 2>nul

echo Done. Run scripts\build_studio.bat again to force a full avaui rebuild.
endlocal
exit /b 0
