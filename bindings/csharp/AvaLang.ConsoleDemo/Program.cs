using AvaLang;

Console.WriteLine("=== AvaLang C# Binding Test ===");
Console.WriteLine();

try
{
    using var vm = new AvaVM();

    Console.WriteLine("=== Test 1: Simple expression ===");
    var result = vm.Eval("5 * 3");
    Console.WriteLine($"Result: {result.AsNumber()}");

    Console.WriteLine("\n=== Test 2: Function definition and call ===");
    vm.Eval("func add(a, b) return a + b end");
    Console.WriteLine("Function add(a, b) defined");
    
    var fn = vm.GetGlobal("add");
    Console.WriteLine($"Got function: Type={fn.Type}");
    
    result = vm.Call(fn, AvaValue.FromNumber(3), AvaValue.FromNumber(4));
    Console.WriteLine($"add(3, 4) = {result.AsNumber()}");

    Console.WriteLine("\n=== Test 3: List operations ===");
    vm.Eval("x = [1, 2, 3]");
    var list = vm.GetGlobal("x");
    Console.WriteLine($"List type: {list.Type}");
    Console.WriteLine($"List length: {vm.GetListLength(list)}");
    var first = vm.GetListItem(list, 0);
    Console.WriteLine($"First element: {first.AsNumber()}");

    Console.WriteLine("\n=== Test 4: Dict operations ===");
    vm.Eval("d = {\"key\": \"value\"}");
    var dict = vm.GetGlobal("d");
    Console.WriteLine($"Dict type: {dict.Type}");
    Console.WriteLine($"Dict contains 'key': {vm.DictContains(dict, "key")}");
    var val = vm.GetDictItem(dict, "key");
    Console.WriteLine($"Value for 'key': {vm.GetStringData(val)}");

    Console.WriteLine("\nAll tests passed!");
}
catch (Exception ex)
{
    Console.WriteLine($"Error: {ex.Message}");
    Console.WriteLine(ex.StackTrace);
}