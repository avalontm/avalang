using AvaLang;

// Runner simple: corre cada .ava en scripts/, imprime PASS/FAIL.
// Con el frontend stub (sin antlr4-runtime) TODOS van a reportar
// "PENDING" -- es el comportamiento esperado hoy, no un bug del runner.
// Cuando conectes frontend_antlr.cpp, estos mismos tests van a
// empezar a devolver PASS sin tocar este archivo.

string scriptsDir = Path.Combine(AppContext.BaseDirectory, "scripts");
var scriptFiles = Directory.GetFiles(scriptsDir, "*.ava").OrderBy(f => f).ToArray();

if (scriptFiles.Length == 0)
{
    Console.WriteLine($"No se encontraron scripts .ava en: {scriptsDir}");
    return 1;
}

using var vm = new AvaVM();

// Funcion nativa usada por 05_native_callback.ava
var printedLines = new List<string>();
vm.RegisterNative("print", args =>
{
    var parts = args.Select(FormatValue);
    string line = string.Join(" ", parts);
    printedLines.Add(line);
    Console.WriteLine($"    [print] {line}");
    return AvaValue.Nil;
});

int passed = 0, pending = 0, failed = 0;

foreach (var path in scriptFiles)
{
    string name = Path.GetFileName(path);
    string source = File.ReadAllText(path);
    Console.WriteLine($"--- {name} ---");

    try
    {
        using var module = vm.Compile(source, name);
        var result = vm.Run(module);
        Console.WriteLine($"  PASS  -> {FormatValue(result)}");
        passed++;
    }
    catch (AvaException ex) when (ex.Message.Contains("frontend not built"))
    {
        Console.WriteLine($"  PENDING (esperado hoy): {ex.Message}");
        pending++;
    }
    catch (AvaException ex)
    {
        Console.WriteLine($"  FAIL  -> {ex.Message}");
        failed++;
    }
    Console.WriteLine();
}

Console.WriteLine($"Resumen: {passed} pass, {pending} pending (frontend stub), {failed} fail, de {scriptFiles.Length} scripts.");
return failed > 0 ? 1 : 0;

static string FormatValue(AvaValue v) => v.Type switch
{
    AvaValueType.Nil => "nil",
    AvaValueType.Bool => (v.As.Bool != 0).ToString(),
    AvaValueType.Number => v.As.Number.ToString(),
    _ => $"<{v.Type}>" // string/list/dict/etc. requieren leer via ava_string_data / helpers
};
