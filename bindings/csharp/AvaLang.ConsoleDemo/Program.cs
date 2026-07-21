using AvaLang;

Console.WriteLine("=== Test: Script llama log() con Console.WriteLine ===\n");

using var ava = new AvaLangManager();

ava.RegisterFunction("log", (AvaVM vm, AvaValue[] args) =>
{
    foreach (var arg in args)
        Console.WriteLine(vm.Inspect(arg));
    return AvaValue.Nil;
});

Console.WriteLine("--- Ejecutando script ---");
ava.LoadScriptString(@"
    log('Hola desde el script!')
    log('El resultado es:', 42 + 8)
    log('Lista:', [1, 2, 3])
");

Console.WriteLine("\n=== Test completado! ===");