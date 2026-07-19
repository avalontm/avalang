using System.Runtime.InteropServices;

namespace AvaLang;

public enum AvaValueType : int
{
    Nil = 0, Bool, Number, String, List, Dict,
    Function, Instance, Class, Coroutine, Native
}

/// <summary>
/// Refleja ava_value_t: struct POD (tag + union) definido en include/ava.h.
/// Se pasa por valor a traves del ABI, igual que en C.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public struct AvaValue
{
    public AvaValueType Type;
    public AvaValuePayload As;

    public static AvaValue Nil => new() { Type = AvaValueType.Nil };

    public static AvaValue FromBool(bool b) =>
        new() { Type = AvaValueType.Bool, As = new AvaValuePayload { Bool = b ? 1 : 0 } };

    public static AvaValue FromNumber(double n) =>
        new() { Type = AvaValueType.Number, As = new AvaValuePayload { Number = n } };
}

[StructLayout(LayoutKind.Explicit)]
public struct AvaValuePayload
{
    [FieldOffset(0)] public int Bool;
    [FieldOffset(0)] public double Number;
    [FieldOffset(0)] public ulong RefId; // AvaRef.id para String/List/Dict/Function/etc.
}

/// <summary>Excepcion lanzada cuando ava_compile o ava_run devuelven un error.</summary>
public class AvaException : Exception
{
    public AvaException(string message) : base(message) { }
}
