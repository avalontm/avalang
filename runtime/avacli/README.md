# ava_cli

Host de referencia nativo de AvaLang: compila y corre scripts `.ava` desde línea de comandos,
sin necesitar un binding en otro lenguaje. Se compila con `scripts/build/build_cli.bat` (ver
ese script para detalles de build/vcpkg/ANTLR4).

## Uso normal

```
ava_cli <script.ava>
```

Compila y corre `script.ava`. La carpeta que lo contiene se agrega automáticamente como search
path del `ModuleResolver`, así que los `import` relativos al script funcionan sin flags
adicionales.

## `ava_cli build` — empacar un proyecto en un `.exe` (avapack)

Fase 2 de `plan_ava_pack.md` / `runtime/avapack/README.md`. Toma un proyecto AvaLang completo
(`.ava`/`.avaui` + imports) y produce un único ejecutable que lo corre, sin necesidad de que la
carpeta del proyecto quede al lado del `.exe` final.

```
ava_cli build --project <dir> --entry <archivo.ava> --out <exe> [--key-file <ruta>] [--keep-temp] [--repo-root <dir>]
```

| Flag          | Obligatorio | Descripción                                                                                   |
| ------------- | :---------: | ----------------------------------------------------------------------------------------------- |
| `--project`   |     sí      | Carpeta raíz del proyecto AvaLang a empacar.                                                    |
| `--entry`     |     sí      | Archivo `.ava` de entrada, relativo a `--project`.                                              |
| `--out`       |     sí      | Ruta del `.exe` final.                                                                          |
| `--key-file`  |     no      | Ruta a 32 bytes crudos con la clave AES-256 a usar (Fase 3). Sin esta flag, se genera una clave aleatoria por build. |
| `--keep-temp` |     no      | No borra `build_pack/` (carpeta de build intermedia) al terminar — útil para debug/rebuilds.    |
| `--repo-root` |     no      | Raíz del repo de AvaLang. Default: el directorio de trabajo actual.                             |

**Precondición:** `ava_cli build` necesita el árbol de fuentes completo de AvaLang disponible
(dispara un build de CMake real contra `runtime/avapack/`) — corré el comando desde la raíz del
repo, o pasá `--repo-root` apuntando a ella. Esto es así porque, hasta que la Fase 6 del plan
cambie de estrategia (bytecode en vez de fuente), cada proyecto empacado es un binario C++ nuevo
que hay que recompilar, no algo que se pueda generar sin un compilador a mano.

Ejemplo:

```
cd D:\_CODE_\avalang
build_cli\runtime\avalang\Release\ava_cli.exe build --project samples\web\testproj --entry app.ava --out testproj_packed.exe
```

Qué hace por dentro:

1. Configura y compila `runtime/avapack` vía `cmake -S . -B build_pack -DAVA_BUILD_PACK=ON ...`
   (usa `platform::IProcess`, no `system()` crudo — ver `src/build_command.cpp`).
2. Ese build corre `avapack_gen` como paso previo (custom command de CMake) para generar
   `embedded_project.cpp` con el contenido de tu proyecto.
3. Copia el `.exe` resultante a `--out`, junto con `avalang.dll` (y `avalang_ui.dll` si aplica)
   a su lado — igual que `ava_cli.exe` necesita esas DLLs junto a sí mismo hoy.
4. Borra `build_pack/` salvo que hayas pasado `--keep-temp`.

**Estado actual (Fase 3):** el `.exe` producido ya cifra el código embebido (AES-256-CTR, clave
aleatoria por build salvo que pases `--key-file`) — `strings` sobre el binario final ya no debe
mostrar tu `.ava`/`.avaui` en texto plano. Sigue siendo un empacador de **fuente**, no de
bytecode (eso es Fase 6), y sigue volcando todos los archivos descifrados al mismo directorio
temporal durante toda la ejecución (Fase 4 reduce esa ventana). Ver `runtime/avapack/README.md`
para el modelo de amenaza completo.
