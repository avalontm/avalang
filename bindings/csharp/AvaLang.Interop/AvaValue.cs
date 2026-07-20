using System.Runtime.InteropServices;

namespace AvaLang;

public enum AvaValueType
{
    Nil = 0,
    Bool,
    Number,
    String,
    List,
    Dict,
    Function,
    Instance,
    Class,
    Coroutine,
    Native,
    Bound,
    Exception
}

[StructLayout(LayoutKind.Explicit, CharSet = CharSet.Ansi, Pack = 1)]
public struct AvaValue
{
    [FieldOffset(0)]
    public AvaValueType Type;
    
    [FieldOffset(8)]
    public int BoolValue;
    
    [FieldOffset(8)]
    public double NumberValue;
    
    [FieldOffset(8)]
    public ulong RefValue;

    public static AvaValue Nil => new() { Type = AvaValueType.Nil };

    public static AvaValue FromBool(bool b) =>
        new() { Type = AvaValueType.Bool, BoolValue = b ? 1 : 0 };

    public static AvaValue FromNumber(double n) =>
        new() { Type = AvaValueType.Number, NumberValue = n };

    public static AvaValue FromInt(int i) => FromNumber(i);
    public static AvaValue FromLong(long l) => FromNumber(l);
    public static AvaValue FromFloat(float f) => FromNumber(f);

    public static AvaValue FromString(ulong id) =>
        new() { Type = AvaValueType.String, RefValue = id };

    public static AvaValue FromList(ulong id) =>
        new() { Type = AvaValueType.List, RefValue = id };

    public static AvaValue FromDict(ulong id) =>
        new() { Type = AvaValueType.Dict, RefValue = id };

    public static AvaValue FromFunction(ulong id) =>
        new() { Type = AvaValueType.Function, RefValue = id };

    public static AvaValue FromNative(ulong id) =>
        new() { Type = AvaValueType.Native, RefValue = id };

    public static AvaValue FromInstance(ulong id) =>
        new() { Type = AvaValueType.Instance, RefValue = id };

    public static AvaValue FromClass(ulong id) =>
        new() { Type = AvaValueType.Class, RefValue = id };

    public static AvaValue FromCoroutine(ulong id) =>
        new() { Type = AvaValueType.Coroutine, RefValue = id };

    public static AvaValue FromBound(ulong id) =>
        new() { Type = AvaValueType.Bound, RefValue = id };

    public static AvaValue FromException(ulong id) =>
        new() { Type = AvaValueType.Exception, RefValue = id };

    public bool AsBool() => Type == AvaValueType.Bool && BoolValue != 0;
    public double AsNumber() => Type == AvaValueType.Number ? NumberValue : 0;
    public int AsInt() => (int)AsNumber();
    public long AsLong() => (long)AsNumber();

    public bool IsRefCounted() => Type switch
    {
        AvaValueType.String or AvaValueType.List or AvaValueType.Dict
        or AvaValueType.Function or AvaValueType.Instance or AvaValueType.Class
        or AvaValueType.Coroutine or AvaValueType.Native or AvaValueType.Bound
        or AvaValueType.Exception => true,
        _ => false
    };

    public ulong GetRefId() => IsRefCounted() ? RefValue : 0;

    public bool IsTruthy() => Type switch
    {
        AvaValueType.Nil => false,
        AvaValueType.Bool => BoolValue != 0,
        AvaValueType.Number => NumberValue != 0,
        _ => true
    };

    public override string ToString() => Type switch
    {
        AvaValueType.Nil => "nil",
        AvaValueType.Bool => (BoolValue != 0).ToString().ToLower(),
        AvaValueType.Number => NumberValue.ToString(System.Globalization.CultureInfo.InvariantCulture),
        _ => $"<{Type.ToString().ToLower()}:{RefValue}>"
    };
}

public class AvaException : Exception
{
    public AvaException(string message) : base(message) { }
    public AvaException(string message, Exception inner) : base(message, inner) { }
}