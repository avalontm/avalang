using System.Globalization;

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

[System.Runtime.InteropServices.StructLayout(
    System.Runtime.InteropServices.LayoutKind.Explicit, 
    CharSet = System.Runtime.InteropServices.CharSet.Ansi, 
    Pack = 1)]
public struct AvaValue
{
    [System.Runtime.InteropServices.FieldOffset(0)]
    public AvaValueType Type;
    
    [System.Runtime.InteropServices.FieldOffset(8)]
    public int BoolValue;
    
    [System.Runtime.InteropServices.FieldOffset(8)]
    public double NumberValue;
    
    [System.Runtime.InteropServices.FieldOffset(8)]
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
    public float AsFloat() => (float)AsNumber();

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
        AvaValueType.Number => NumberValue.ToString(CultureInfo.InvariantCulture),
        _ => $"<{Type.ToString().ToLower()}:{RefValue}>"
    };
}

public class AvaException : Exception
{
    public AvaException(string message) : base(message) { }
    public AvaException(string message, Exception inner) : base(message, inner) { }
}

public static class AvaValueExtensions
{
    public static bool IsNil(this AvaValue v) => v.Type == AvaValueType.Nil;
    public static bool IsBool(this AvaValue v) => v.Type == AvaValueType.Bool;
    public static bool IsNumber(this AvaValue v) => v.Type == AvaValueType.Number;
    public static bool IsString(this AvaValue v) => v.Type == AvaValueType.String;
    public static bool IsList(this AvaValue v) => v.Type == AvaValueType.List;
    public static bool IsDict(this AvaValue v) => v.Type == AvaValueType.Dict;
    public static bool IsFunction(this AvaValue v) => v.Type == AvaValueType.Function;
    public static bool IsInstance(this AvaValue v) => v.Type == AvaValueType.Instance;
    public static bool IsClass(this AvaValue v) => v.Type == AvaValueType.Class;
    public static bool IsNative(this AvaValue v) => v.Type == AvaValueType.Native;

    public static string? AsString(this AvaValue v, AvaVM vm)
    {
        if (v.Type != AvaValueType.String) return null;
        return vm.GetString(v);
    }

    public static AvaList AsAvaList(this AvaValue v, AvaVM vm)
    {
        if (v.Type != AvaValueType.List)
            throw new AvaException("Value is not a list");
        return new AvaList(v, vm);
    }

    public static AvaDict AsAvaDict(this AvaValue v, AvaVM vm)
    {
        if (v.Type != AvaValueType.Dict)
            throw new AvaException("Value is not a dict");
        return new AvaDict(v, vm);
    }
}

public struct AvaList
{
    internal AvaValue _value;
    private AvaVM _vm;

    internal AvaList(AvaValue value, AvaVM vm)
    {
        _value = value;
        _vm = vm;
    }

    public int Count => _vm.GetListLength(_value);

    public AvaValue this[int index] => _vm.GetListItem(_value, index);
    
    public void Set(int index, AvaValue value) => _vm.SetListItem(_value, index, value);

    public void Add(AvaValue value) => _vm.ListAppend(_value, value);

    public List<AvaValue> ToList()
    {
        var result = new List<AvaValue>(Count);
        for (int i = 0; i < Count; i++)
            result.Add(this[i]);
        return result;
    }

    public List<T> ToList<T>(Func<AvaValue, T> convert)
    {
        var result = new List<T>(Count);
        for (int i = 0; i < Count; i++)
            result.Add(convert(this[i]));
        return result;
    }
}

public readonly struct AvaDict
{
    private readonly AvaValue _value;
    private readonly AvaVM _vm;

    internal AvaDict(AvaValue value, AvaVM vm)
    {
        _value = value;
        _vm = vm;
    }

    public int Count => _vm.GetDictLength(_value);

    public bool Contains(string key) => _vm.DictContains(_value, key);

    public AvaValue this[string key]
    {
        get => _vm.GetDictItem(_value, key);
        set => _vm.DictSet(_value, key, value);
    }

    public AvaValue Get(string key) => _vm.GetDictItem(_value, key);

    public void Set(string key, AvaValue value) => _vm.DictSet(_value, key, value);

    public bool TryGet(string key, out AvaValue value)
    {
        if (Contains(key))
        {
            value = Get(key);
            return true;
        }
        value = AvaValue.Nil;
        return false;
    }
}

public static class AvaConversions
{
    public static AvaValue ToAvaValue(this bool v) => AvaValue.FromBool(v);
    public static AvaValue ToAvaValue(this int v) => AvaValue.FromInt(v);
    public static AvaValue ToAvaValue(this long v) => AvaValue.FromLong(v);
    public static AvaValue ToAvaValue(this double v) => AvaValue.FromNumber(v);
    public static AvaValue ToAvaValue(this float v) => AvaValue.FromFloat(v);
    public static AvaValue ToAvaValue(this AvaValue v) => v;

    public static AvaValue ObjectToAva(object? value, AvaVM vm)
    {
        if (value == null) return AvaValue.Nil;
        
        if (value is bool b) return b.ToAvaValue();
        if (value is int i) return i.ToAvaValue();
        if (value is long l) return l.ToAvaValue();
        if (value is double d) return d.ToAvaValue();
        if (value is float f) return f.ToAvaValue();
        if (value is string s) return vm.CreateString(s);
        if (value is AvaValue av) return av;
        
        throw new AvaException($"Cannot convert {value.GetType().Name} to AvaValue");
    }

    public static T FromAvaValue<T>(this AvaValue v, AvaVM vm)
    {
        var type = typeof(T);

        if (type == typeof(bool)) return (T)(object)v.AsBool();
        if (type == typeof(int)) return (T)(object)v.AsInt();
        if (type == typeof(long)) return (T)(object)v.AsLong();
        if (type == typeof(double)) return (T)(object)v.AsNumber();
        if (type == typeof(float)) return (T)(object)v.AsFloat();
        if (type == typeof(string)) return (T)(object)vm.GetString(v);
        if (type == typeof(AvaValue)) return (T)(object)v;
        
        throw new AvaException($"Cannot convert {v.Type} to {type.Name}");
    }
}