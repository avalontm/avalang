# AvaLang.Interop — como buildearlo y usarlo

## 1. Copiar la DLL nativa al lugar correcto

Copia el `avalang.dll` que te genero tu build de CMake/MSVC a:

```
AvaLang.Interop/runtimes/win-x64/native/avalang.dll
```

(esa carpeta ya existe en este zip, solo falta el .dll adentro — no lo incluyo
yo porque es un binario compilado en tu maquina).

## 2. Compilar la libreria managed

```
cd AvaLang.Interop
dotnet build -c Release
```

Esto genera `AvaLang.Interop/bin/Release/net8.0/AvaLang.Interop.dll`
(managed) y copia `avalang.dll` (nativa) al lado automaticamente, gracias
al `<None ... CopyToOutputDirectory="PreserveNewest">` del .csproj.

## 3. Usarla desde otro proyecto .NET

Opcion A — referencia de proyecto (mismo repo/solution):

```
dotnet add YourProject.csproj reference path/to/AvaLang.Interop.csproj
```

Opcion B — solo el binario compilado (sin el codigo fuente):
copia estos 2 archivos al proyecto consumidor y agregalos como referencia
de ensamblado / `<Reference>`, o simplemente ponelos en el mismo output dir:
- `AvaLang.Interop.dll`
- `avalang.dll` (tiene que estar SIEMPRE al lado del .exe consumidor,
  el runtime de .NET busca DLLs nativas en el mismo directorio del
  ejecutable por defecto)

Opcion C — empaquetar como NuGet local (recomendado si lo usas en varios
proyectos/soluciones):

```
cd AvaLang.Interop
dotnet pack -c Release -o ../nupkgs
```

Esto genera `AvaLang.Interop.0.1.0.nupkg`, ya con `avalang.dll` empaquetada
bajo `runtimes/win-x64/native/` (por el `Pack="true"` en el .csproj). Despues
en cualquier otro proyecto:

```
dotnet nuget add source ./nupkgs -n avalang-local
dotnet add package AvaLang.Interop --source avalang-local
```

Y el `avalang.dll` se copia solo, sin que tengas que acordarte de nada.

## 4. Probar

```
cd ../AvaLang.ConsoleDemo
dotnet run
```

Va a fallar con "AvaLang frontend not built: ..." hasta que compiles el
proyecto C++ con antlr4-runtime disponible (ver conversacion anterior sobre
vcpkg). Eso confirma que la DLL nativa y el wrapper managed estan bien
conectados — el error viene de la logica interna de AvaLang, no de la
interop.
