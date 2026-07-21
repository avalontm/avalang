# AvaLang C# Binding - Progreso y Desarrollo

## Resumen

Este documento registra el desarrollo completo del binding de C# para AvaLang, incluyendo la arquitectura, errores encontrados, bugs corregidos y lecciones aprendidas.

**Última actualización:** 2026-07-20
**Estado:** Funcional - Llamadas bidireccionales C# ↔ Script

---

## 1. Arquitectura del Binding

### Estructura de Proyectos

```
bindings/csharp/
├── AvaLang.Interop/           # Librería class-library (.NET 8)
│   ├── AvaValue.cs            # Struct para representar valores de AvaLang
│   ├── AvaVM.cs               # Wrapper principal de la VM
│   ├── NativeMethods.cs       # P/Invoke declarations
│   └── AvaLang.Interop.csproj # Configuración de build
├── AvaLang.Tests/             # Proyecto de pruebas
│   ├── Program.cs             # Suite de tests
│   └── scripts/               # Scripts .ava de prueba
└── AvaLang.ConsoleDemo/       # Demo de consola
```

### Componentes Principales

#### AvaValue.cs
- Struct con `LayoutKind.Explicit` para coincidir con `ava_value_t` de C
- Campos:
  - `Type`: `AvaValueType` enum
  - `BoolValue`: `int` en offset 8
  - `NumberValue`: `double` en offset 8
  - `RefValue`: `ulong` en offset 8 (para tipos reference-counted)
- Factory methods estáticos: `FromBool`, `FromNumber`, `FromString`, `FromList`, `FromDict`, etc.
- Helper methods: `AsBool()`, `AsNumber()`, `AsInt()`, `AsLong()`, `IsRefCounted()`, `IsTruthy()`

#### AvaVM.cs
- Clase principal `AvaVM` con `IDisposable`
- Manejo del ciclo de vida: `_handle` para el puntero nativo
- **`_keepAlive`**: Lista crítica de `Delegate` para evitar GC de callbacks natives
- Métodos principales:
  - `Compile(source, sourceName)`: Compila código AvaLang
  - `Run(module)`: Ejecuta un módulo compilado
  - `Eval(source)`: Compila y ejecuta en un paso
  - `RunScript(filePath)`: Lee archivo y ejecuta
- Operaciones de contenedores:
  - `CreateList/Dict`, `GetListLength/Item/Set`, `ListAppend`
  - `GetDictItem/Set/Contains`
- Registro de funciones nativas: `RegisterNative(name, fn)`
- Conversión de tipos: `ConvertFrom<T>(value)`
- Gestión de memoria: `Retain/Release` para reference counting

#### NativeMethods.cs
- Declaraciones P/Invoke con `DllImport("avalang")`
- CallingConvention: `Cdecl`
- Delegate `AvaNativeFn` para callbacks desde C++
- ~40 funciones exportadas cubriendo toda la C API

---

## 2. Errores y Bugs Encontrados

### Bug 1: DLL Nativa No Encontrada

**Síntoma:**
```
DllNotFoundException: No se pudo cargar la DLL 'avalang'
```

**Causa:**
La DLL nativa (`avalang.dll`) no estaba en el directorio correcto o no se copiaba al output.

**Solución:**
Modificar `AvaLang.Interop.csproj` para copiar automáticamente la DLL:

```xml
<ItemGroup>
  <None Include="runtimes\win-x64\native\avalang.dll"
        Link="avalang.dll"
        CopyToOutputDirectory="PreserveNewest"
        Pack="true"
        PackagePath="runtimes/win-x64/native/avalang.dll" />
</ItemGroup>
```

**Lección:** La ubicación de la DLL nativa es crítica. .NET busca en:
1. Directorio del ejecutable
2. Runtimes específicos por plataforma
3. PATH del sistema

---

### Bug 2: GCHandle de Callbacks Nativos

**Síntoma:**
Después de registrar una función nativa, el callback podía ser GC'd, causando crashes/crashes al invocar la función.

**Causa:**
`UnmanagedFunctionPointer` delegates pueden ser recolectados por el GC de .NET incluso cuando la DLL nativa los mantiene como function pointers.

**Solución:**
Mantener referencias vivas en `AvaVM._keepAlive`:

```csharp
public void RegisterNative(string name, Func<AvaVM, AvaValue[], AvaValue> fn)
{
    var vmRef = this;
    NativeMethods.AvaNativeFn thunk = (vm, argsPtr, argCount, userData) =>
    {
        // ... marshal args ...
        return fn(vmRef, args);
    };
    _keepAlive.Add(thunk);  // CRÍTICO: mantiene el delegate vivo
    NativeMethods.ava_vm_register_native(_handle, name, thunk, IntPtr.Zero);
}
```

**Lección:** SIEMPRE mantener referencias a delegates usados en P/Invoke. `_keepAlive` debe limpiarse en `Dispose()`.

---

### Bug 3: Memory Management en ava_call

**Síntoma:**
Posibles memory leaks o corrupción al llamar funciones con argumentos.

**Causa:**
La asignación de memoria para argumentos no se liberaba correctamente en todos los paths.

**Solución:**
Uso correcto de `try/finally` para cleanup:

```csharp
IntPtr argsPtr = IntPtr.Zero;
IntPtr resultPtr = Marshal.AllocHGlobal(Marshal.SizeOf<AvaValue>());
if (args.Length > 0)
{
    argsPtr = Marshal.AllocHGlobal(args.Length * Marshal.SizeOf<AvaValue>());
    for (int i = 0; i < args.Length; i++)
        Marshal.StructureToPtr(args[i], argsPtr + i * Marshal.SizeOf<AvaValue>(), false);
}

try
{
    NativeMethods.ava_call(_handle, callable, argsPtr, (UIntPtr)args.Length, resultPtr, out IntPtr err);
    // ...
}
finally
{
    if (argsPtr != IntPtr.Zero) Marshal.FreeHGlobal(argsPtr);
    Marshal.FreeHGlobal(resultPtr);
}
```

**Lección:** Siempre usar `try/finally` para memoria nativa. Prestar atención especial a los paths de error.

---

### Bug 4: Tamaño de AvaValue

**Síntoma:**
`Marshal.SizeOf<AvaValue>()` retornaba un tamaño incorrecto.

**Causa:**
El struct `AvaValue` en C# no coincidía exactamente con la versión C.

**Verificación:**
```csharp
Console.WriteLine($"sizeof(AvaValue) = {Marshal.SizeOf<AvaValue>()}");
```

El tamaño esperado era 24 bytes (16 header + 8 data para el union en C++).

**Solución:**
Verificar que el struct en C# tenga el LayoutKind correcto:

```csharp
[StructLayout(LayoutKind.Explicit, CharSet = CharSet.Ansi, Pack = 1)]
public struct AvaValue
{
    [FieldOffset(0)]
    public AvaValueType Type;      // 8 bytes (enum)
    
    [FieldOffset(8)]
    public int BoolValue;          // 4 bytes
    
    [FieldOffset(12)]              // Padding para alignment
    public double NumberValue;     // 8 bytes
    
    // En C++, el union empieza en offset 8, pero en C# precisamos
    // FieldOffset 8 para RefValue también
}
```

**Lección:** Verificar el tamaño del struct con tests. Diferencias de padding pueden causar corrupcion.

---

### Bug 5: String Marshaling

**Síntoma:**
Strings con caracteres especiales o Unicode no se marshalaban correctamente.

**Causa:**
`CharSet.Ansi` vs `CharSet.UTF8` en DllImport.

**Solución:**
Para `ava_string_create` que acepta strings UTF-8:

```csharp
[DllImport(Lib, CallingConvention = Cdecl, CharSet = CharSet.Ansi)]
public static extern AvaValue ava_string_create(IntPtr vm, string utf8, UIntPtr len);
```

Y para obtener datos:

```csharp
public string GetStringData(AvaValue str)
{
    IntPtr data = NativeMethods.ava_string_data(_handle, ref str, out UIntPtr len);
    return Marshal.PtrToStringUTF8(data, (int)len) ?? string.Empty;
}
```

**Lección:** Verificar la codificación esperada por la DLL nativa. Usar `PtrToStringUTF8` para strings UTF-8.

---

### Bug 6: Reference Counting con Types Reference-Counted

**Síntoma:**
Memory leaks detectados con listas y diccionarios.

**Causa:**
El binding no llamaba `Retain/Release` para tipos ref-counted (List, Dict, String, etc).

**Solución:**
El método `AvaValue.IsRefCounted()` determina qué tipos necesitan management:

```csharp
public bool IsRefCounted() => Type switch
{
    AvaValueType.String or AvaValueType.List or AvaValueType.Dict
    or AvaValueType.Function or AvaValueType.Instance or AvaValueType.Class
    or AvaValueType.Coroutine or AvaValueType.Native or AvaValueType.Bound
    or AvaValueType.Exception => true,
    _ => false
};
```

**Lección:** Los tipos que cruzan el boundary deben ser explícitamente retenidos/released si la VM los所有权.

---

## 3. Verificación de Funcionalidad

### Test 1: Llamadas Bidireccionales Completas

**C# llama funciones del script:**
```csharp
ava.LoadScriptString(@"
    func add(a, b) return a + b end
    func greet(name) return 'Hola ' + name end
");

int sum = ava.Call<int>("add", 5, 3);              // 8
string greeting = ava.Call<string>("greet", "Ana"); // "Hola Ana"
```

**Script llama funciones de C#:**
```csharp
ava.RegisterFunction("log", (AvaVM vm, AvaValue[] args) =>
{
    foreach (var arg in args)
        Console.WriteLine(vm.Inspect(arg));
    return AvaValue.Nil;
});

ava.RegisterFunction("getTime", (AvaVM vm, AvaValue[] args) =>
    vm.CreateString(DateTime.Now.ToString("HH:mm:ss")));

ava.LoadScriptString("log(getTime())");
```

### Test 2: Cargar Script desde Archivo
```csharp
ava.LoadScript("scripts/player.ava");
int health = ava.Get<int>("health");  // 100
ava.Set("health", 50);
```

### Test 3: Registrar Objetos con Métodos
```csharp
public class GameAPI {
    public double GetScore() => 500;
    public void ResetGame() => Console.WriteLine("Reset!");
}

ava.RegisterObject("Game", new GameAPI());
ava.LoadScriptString("Game_resetgame()");  // Llama método C#
```

---

## 4. API C# Expuesta

### Clase AvaLangManager

```csharp
using var ava = new AvaLangManager();

// Cargar scripts
ava.LoadScript("scripts/player.ava");           // Desde archivo
ava.LoadScriptString("x = 5");                  // Desde string

// Variables globales
int x = ava.Get<int>("x");
ava.Set("health", 100);

// Llamadas bidireccionales
int sum = ava.Call<int>("add", 5, 3);          // C# → Script
ava.RegisterFunction("log", (vm, args) => ...); // Script → C#

// Registrar objetos
ava.RegisterObject("Game", gameObject);         // Métodos como Game_nombremetodo()
```

### Clase AvaVM

```csharp
using var vm = new AvaVM();

// Ejecución
AvaValue result = vm.Eval("1 + 2 * 3");

// Funciones nativas
vm.RegisterNative("print", (vm, args) => {
    Console.WriteLine(string.Join(" ", args.Select(vm.FormatValue)));
    return AvaValue.Nil;
});

// Contenedores
AvaValue list = vm.CreateList(1, 2, 3);
int len = vm.GetListLength(list);

// Cleanup
vm.Dispose();  // o usar 'using'
```

### Enum AvaValueType

```csharp
enum AvaValueType {
    Nil, Bool, Number, String, List, Dict,
    Function, Instance, Class, Coroutine,
    Native, Bound, Exception
}
```

---

## 5. Lecciones Aprendidas

### P/Invoke Best Practices

1. **Mantener delegates vivos**: Usar lista `_keepAlive` para callbacks
2. **Limpiar memoria nativa**: Siempre en `finally` blocks
3. **Verificar tamaño de structs**: `Marshal.SizeOf<T>()` debe coincidir con C
4. **CharSet correcto**: Determinar si es ANSI, Unicode, o UTF-8
5. **Calling convention**: Usar `Cdecl` para funciones variádicas o sin `extern "C"` explícito

### Memory Management

1. Los tipos reference-counted (String, List, Dict, etc.) necesitan `Retain/Release`
2. La VM puede mantener referencias internas que no deben liberarse prematuramente
3. El binding no es owner de los valores - solo hace borrow por defecto

### Estructura de Archivos

1. `AvaLang.Interop/` es una class library reutilizable
2. Puede empacarse como NuGet local con `dotnet pack`
3. La DLL nativa se copia automáticamente al output

---

## 6. Trabajo Futuro

### ✅ Completado
- [x] Llamadas bidireccionales C# ↔ Script
- [x] Cargar scripts desde archivo con `LoadScript()`
- [x] Registrar funciones nativas con `RegisterFunction()`
- [x] Registrar objetos con métodos via `RegisterObject()`

### Por Implementar
- [ ] Enumeradores para `List` y `Dict`
- [ ] Async/await support para funciones nativas
- [ ] Integración con proyectos externos (Unity, Godot)

### Mejoras Posibles
- [ ] Cache de `Marshal.SizeOf<AvaValue>()` en static field
- [ ] Pool de memoria para argumentos de función
- [ ] Source generators para reducing boilerplate

---

## 7. Archivos de Referencia

| Archivo | Descripción |
|---------|-------------|
| `include/ava.h` | API C pública de AvaLang |
| `src/api/c_api.cpp` | Implementación de la API en C++ |
| `bindings/csharp/README.md` | Instrucciones de build y uso |
| `bindings/csharp/README_INTEROP.md` | Guía detallada de interop |
| `DESIGN.md` | Diseño original del proyecto |

---

## 8. Comandos de Build y Test

```bash
# 1. Compilar la DLL nativa (desde raíz del proyecto)
build.bat

# 2. Copiar la DLL a la carpeta de runtime
copy build\Release\avalang.dll bindings\csharp\AvaLang.Interop\runtimes\win-x64\native\

# 3. Compilar el binding
cd bindings\csharp\AvaLang.Interop
dotnet build -c Release

# 4. Ejecutar tests
cd ..\AvaLang.Tests
dotnet run

# 5. Ejecutar demo
cd ..\AvaLang.ConsoleDemo
dotnet run
```

---

**Nota:** Este binding está diseñado para ser thin y eficiente. Toda la complejidad del lenguaje vive en el core C++, el binding solo traduce tipos y forward calls.