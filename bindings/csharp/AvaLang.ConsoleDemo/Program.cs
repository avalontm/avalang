using AvaLang;

// Desde el punto de vista de este proyecto, AvaLang es una libreria
// managed comun y corriente -- no hay P/Invoke a la vista.

using var vm = new AvaVM();

// Funcion host expuesta a los scripts AvaLang, ej: llamable como print(...)
vm.RegisterNative("print", args =>
{
    foreach (var a in args)
        Console.Write(a.Type == AvaValueType.Number ? a.As.Number.ToString() : a.Type.ToString());
    Console.WriteLine();
    return AvaValue.Nil;
});

try
{
    using var module = vm.Compile("print(1 + 2)");
    var result = vm.Run(module);
    Console.WriteLine($"Resultado: {result.Type}");
}
catch (AvaException ex)
{
    // Hoy esto va a saltar con "AvaLang frontend not built: ..."
    // porque el .dll se compilo con el frontend stub (sin antlr4-runtime).
    Console.WriteLine($"Error esperado con el frontend stub: {ex.Message}");
}
