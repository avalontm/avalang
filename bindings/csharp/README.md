# AvaLang C# Binding

API para integrar scripts AvaLang en aplicaciones C#.

## Estructura

- `AvaLang.Interop/` — libreria class-library (`AvaLang.Interop.dll`)
  - `AvaVM` — VM principal
  - `AvaLangManager` — API de alto nivel
  - `AvaGameAPI` — clase base para APIs de juego
  - `MathAPI`, `DebugAPI` — APIs predefinidas
- `AvaLang.ConsoleDemo/` — demo de uso
- `scripts/` — scripts de prueba `.ava`

## Build

```bash
# 1. Compilar DLL nativa
build.bat

# 2. Copiar DLL al binding
copy build\Release\avalang.dll bindings\csharp\AvaLang.Interop\runtimes\win-x64\native\

# 3. Compilar binding
cd bindings\csharp\AvaLang.Interop
dotnet build -c Release

# 4. Ejecutar demo
cd ../AvaLang.ConsoleDemo
dotnet run
```

## Uso Básico

```csharp
using AvaLang;

using var ava = new AvaLangManager();

// Cargar script desde archivo
ava.LoadScript("scripts/player.ava");

// Leer variables
int health = ava.Get<int>("health");
string name = ava.Get<string>("playerName");

// Modificar variables
ava.Set("health", 50);
```

## Llamadas Bidireccionales

### C# llama funciones del script

```csharp
ava.LoadScriptString(@"
    func add(a, b)
        return a + b
    end
    
    func greet(name)
        return 'Hola ' + name
    end
");

int sum = ava.Call<int>("add", 5, 3);
string greeting = ava.Call<string>("greet", "Carlos");
```

### Script llama funciones de C#

```csharp
ava.RegisterFunction("log", (AvaVM vm, AvaValue[] args) =>
{
    foreach (var arg in args)
        Console.WriteLine(vm.Inspect(arg));
    return AvaValue.Nil;
});

ava.RegisterFunction("getTime", (AvaVM vm, AvaValue[] args) =>
    vm.CreateString(DateTime.Now.ToString("HH:mm:ss")));

// Ahora el script puede llamar log() y getTime()
ava.LoadScriptString(@"
    log('Iniciando juego...')
    log('Hora:', getTime())
");
```

## Registrar Objetos

```csharp
public class GameAPI
{
    public double GetScore() => 500;
    public void ResetGame() => Console.WriteLine("Game reset!");
}

var game = new GameAPI();
ava.RegisterObject("Game", game);

// En script:
// Game_getscore() -> 500
// Game_resetgame() -> imprime "Game reset!"
```

**Nota:** Los métodos se registran como `NombreObjeto_nombremetodo` en minúsculas.

## Cargar Scripts

```csharp
ava.LoadScript("scripts/player.ava");      // Desde archivo
ava.LoadScriptString("health = 100");        // Desde string
```

## APIs Predefinidas

### MathAPI
```csharp
var math = new MathAPI(ava.VM);
ava.RegisterObject("Math", math);

// En script:
// Math_abs(-5), Math_sqrt(16), Math_max(10, 20)
```

## Clase AvaLangManager

| Método | Descripción |
|--------|-------------|
| `LoadScript(path)` | Carga script desde archivo |
| `LoadScriptString(src)` | Ejecuta script desde string |
| `Get<T>(name)` | Obtiene variable global |
| `Set(name, value)` | Establece variable global |
| `Call<T>(name, args)` | Llama función del script con retorno |
| `RegisterFunction(name, fn)` | Registra función nativa |
| `RegisterObject(name, obj)` | Registra objeto con métodos |

## Estructura Recomendada

```
GameManager (C#)
    │
    ├── AvaLangManager (Script State)
    │       │
    │       ├── GameAPI      (lógica de juego)
    │       ├── MathAPI      (utilidades matemáticas)
    │       └── log()        (depuración)
    │
    └── Scripts/
            player.ava
            enemy.ava
            weapons.ava
```

## Archivos DLL

Coloca `avalang.dll` en:
- `AvaLang.Interop/runtimes/win-x64/native/avalang.dll` (Windows x64)
- `AvaLang.Interop/runtimes/linux-x64/native/libavalang.so` (Linux x64)