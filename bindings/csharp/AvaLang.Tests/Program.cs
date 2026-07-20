using System.Runtime.InteropServices;
using AvaLang;

Console.WriteLine("=== AvaLang C# Binding Test ===");
Console.WriteLine($"sizeof(AvaValue) = {Marshal.SizeOf<AvaValue>()}");

using var vm = new AvaVM();

Console.WriteLine("\n=== Test 1: Basic arithmetic (1 + 2 * 3 = 7) ===");
var r1 = vm.Eval("1 + 2 * 3");
Console.WriteLine($"Result: {vm.FormatValue(r1)} (expected: 7)");
Console.WriteLine(r1.AsNumber() == 7 ? "PASS" : "FAIL");

Console.WriteLine("\n=== Test 2: String creation ===");
var r2 = vm.Eval("\"hello\"");
Console.WriteLine($"Result: {vm.FormatValue(r2)}");
Console.WriteLine(r2.Type == AvaValueType.String ? "PASS" : "FAIL");

Console.WriteLine("\n=== Test 3: List creation ===");
var r3 = vm.Eval("[1, 2, 3]");
Console.WriteLine($"List length: {vm.GetListLength(r3)}");
Console.WriteLine(vm.GetListLength(r3) == 3 ? "PASS" : "FAIL");

Console.WriteLine("\n=== Summary ===");
Console.WriteLine("All basic tests completed.");