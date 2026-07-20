using System.Runtime.InteropServices;

namespace AvaLang;

public sealed class AvaVM : IDisposable
{
    private IntPtr _handle;
    private readonly List<Delegate> _keepAlive = new();

    public AvaVM()
    {
        _handle = NativeMethods.ava_vm_create();
        if (_handle == IntPtr.Zero)
            throw new AvaException("Failed to create VM instance");
    }

    internal IntPtr Handle => _handle;

public IntPtr GetHandle() => _handle;
    
    public AvaModule Compile(string source, string sourceName = "<script>")
    {
        ThrowIfDisposed();
        Console.WriteLine($"[DEBUG] Compiling source: '{source}', name: '{sourceName}'");
        IntPtr module = NativeMethods.ava_compile(_handle, source, sourceName, out IntPtr err);
        if (module == IntPtr.Zero)
        {
            string? msg = ConsumeError(err);
            throw new AvaException(msg != null ? msg : "Compile error");
        }
        Console.WriteLine($"[DEBUG] Compile succeeded, module handle: {module}");
        return new AvaModule(module);
    }

    public AvaValue Run(AvaModule module)
    {
        ThrowIfDisposed();
        IntPtr resultPtr = Marshal.AllocHGlobal(Marshal.SizeOf<AvaValue>());
        try
        {
            NativeMethods.ava_run(_handle, module.Handle, resultPtr, out IntPtr err);
            AvaValue result = Marshal.PtrToStructure<AvaValue>(resultPtr);
            Console.WriteLine($"[DEBUG] ava_run returned: Type={result.Type}, Number={result.AsNumber()}");
            if (err != IntPtr.Zero)
            {
                string? msg = ConsumeError(err);
                throw new AvaException(msg ?? "Runtime error");
            }
            return result;
        }
        finally
        {
            Marshal.FreeHGlobal(resultPtr);
        }
    }

    public AvaValue Eval(string source, string sourceName = "<eval>")
    {
        using var module = Compile(source, sourceName);
        return Run(module);
    }

    public AvaValue RunScript(string filePath)
    {
        if (!File.Exists(filePath))
            throw new AvaException($"Script file not found: {filePath}");
        string source = File.ReadAllText(filePath);
        return Eval(source, Path.GetFileName(filePath));
    }

    public AvaValue GetGlobal(string name)
    {
        ThrowIfDisposed();
        return NativeMethods.ava_get_global(_handle, name);
    }

    public void SetGlobal(string name, AvaValue value)
    {
        ThrowIfDisposed();
        NativeMethods.ava_set_global(_handle, name, ref value);
    }

    public void SetGlobal(string name, bool value) => SetGlobal(name, AvaValue.FromBool(value));
    public void SetGlobal(string name, int value) => SetGlobal(name, AvaValue.FromInt(value));
    public void SetGlobal(string name, long value) => SetGlobal(name, AvaValue.FromLong(value));
    public void SetGlobal(string name, double value) => SetGlobal(name, AvaValue.FromNumber(value));
    public void SetGlobal(string name, string value) => SetGlobal(name, CreateString(value));

public AvaValue Call(AvaValue callable, params AvaValue[] args)
    {
        ThrowIfDisposed();
        if (callable.Type == AvaValueType.Nil)
            throw new AvaException("Cannot call nil");

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
            AvaValue result = Marshal.PtrToStructure<AvaValue>(resultPtr);
            if (err != IntPtr.Zero)
            {
                string msg = ConsumeError(err);
                throw new AvaException(msg ?? "Call error");
            }
            return result;
        }
        finally
        {
            if (argsPtr != IntPtr.Zero) Marshal.FreeHGlobal(argsPtr);
            Marshal.FreeHGlobal(resultPtr);
        }
    }

        public T Call<T>(string functionName, params AvaValue[] args)
    {
        var fn = GetGlobal(functionName);
        var result = Call(fn, args);
        return ConvertFrom<T>(result);
    }

    public AvaValue Import(string modulePath, string? alias = null)
    {
        ThrowIfDisposed();
        IntPtr result = NativeMethods.ava_import(_handle, modulePath, alias ?? "", out IntPtr err);
        if (err != IntPtr.Zero)
        {
            string msg = ConsumeError(err);
            throw new AvaException(msg ?? "Import error");
        }
        if (result == IntPtr.Zero)
            return AvaValue.Nil;
        return AvaValue.FromNative((ulong)result.ToInt64());
    }

    public AvaCoroutine CreateCoroutine(AvaValue function)
    {
        ThrowIfDisposed();
        IntPtr co = NativeMethods.ava_coroutine_create(_handle, ref function);
        if (co == IntPtr.Zero)
            throw new AvaException("Failed to create coroutine");
        return new AvaCoroutine(co);
    }

    #region Value Creation

    public AvaValue CreateNil() => AvaValue.Nil;

    public AvaValue CreateBool(bool value) => AvaValue.FromBool(value);

    public AvaValue CreateNumber(double value) => AvaValue.FromNumber(value);
    public AvaValue CreateNumber(int value) => AvaValue.FromInt(value);

    public AvaValue CreateString(string value)
    {
        ThrowIfDisposed();
        if (value == null) return AvaValue.Nil;
        return NativeMethods.ava_string_create(_handle, value, (UIntPtr)value.Length);
    }

    public AvaValue CreateList()
    {
        ThrowIfDisposed();
        IntPtr list = NativeMethods.ava_list_create(_handle);
        return AvaValue.FromList((ulong)list.ToInt64());
    }

    public AvaValue CreateList(params AvaValue[] items)
    {
        var list = CreateList();
        foreach (var item in items)
            ListAppend(list, item);
        return list;
    }

    public AvaValue CreateDict()
    {
        ThrowIfDisposed();
        IntPtr dict = NativeMethods.ava_dict_create(_handle);
        return AvaValue.FromDict((ulong)dict.ToInt64());
    }

    #endregion

    #region String Operations

    public string GetStringData(AvaValue str)
    {
        ThrowIfDisposed();
        IntPtr data = NativeMethods.ava_string_data(_handle, ref str, out UIntPtr len);
        return Marshal.PtrToStringUTF8(data, (int)len) ?? string.Empty;
    }

    #endregion

    #region List Operations

    public int GetListLength(AvaValue list)
    {
        ThrowIfDisposed();
        return (int)NativeMethods.ava_list_length(_handle, ref list);
    }

    public AvaValue GetListItem(AvaValue list, int index)
    {
        ThrowIfDisposed();
        return NativeMethods.ava_list_get(_handle, ref list, (UIntPtr)index);
    }

    public void SetListItem(AvaValue list, int index, AvaValue value)
    {
        ThrowIfDisposed();
        NativeMethods.ava_list_set(_handle, ref list, (UIntPtr)index, ref value);
    }

    public void ListAppend(AvaValue list, AvaValue item)
    {
        ThrowIfDisposed();
        NativeMethods.ava_list_append(_handle, ref list, ref item);
    }

    public List<AvaValue> GetListItems(AvaValue list)
    {
        int len = GetListLength(list);
        var items = new List<AvaValue>(len);
        for (int i = 0; i < len; i++)
            items.Add(GetListItem(list, i));
        return items;
    }

    #endregion

    #region Dict Operations

    public int GetDictLength(AvaValue dict)
    {
        ThrowIfDisposed();
        return (int)NativeMethods.ava_dict_length(_handle, ref dict);
    }

    public AvaValue GetDictItem(AvaValue dict, string key)
    {
        ThrowIfDisposed();
        return NativeMethods.ava_dict_get(_handle, ref dict, key);
    }

    public bool DictContains(AvaValue dict, string key)
    {
        ThrowIfDisposed();
        return NativeMethods.ava_dict_contains(_handle, dict, key, (UIntPtr)key.Length);
    }

    public void DictSet(AvaValue dict, string key, AvaValue value)
    {
        ThrowIfDisposed();
        NativeMethods.ava_dict_set(_handle, ref dict, key, ref value);
    }

    public Dictionary<string, AvaValue> GetDictEntries(AvaValue dict)
    {
        ThrowIfDisposed();
        return new Dictionary<string, AvaValue>();
    }

    #endregion

    #region Native Function Registration

    public void RegisterNative(string name, Func<AvaVM, AvaValue[], AvaValue> fn)
    {
        ThrowIfDisposed();
        var vmRef = this;
        NativeMethods.AvaNativeFn thunk = (vm, argsPtr, argCount, userData) =>
        {
            int count = (int)argCount;
            AvaValue[] args = count > 0
                ? new AvaValue[count]
                : Array.Empty<AvaValue>();
            for (int i = 0; i < count; i++)
                args[i] = Marshal.PtrToStructure<AvaValue>(argsPtr + i * Marshal.SizeOf<AvaValue>());
            return fn(vmRef, args);
        };
        _keepAlive.Add(thunk);
        NativeMethods.ava_vm_register_native(_handle, name, thunk, IntPtr.Zero);
    }

    public void RegisterNative(string name, Action<AvaVM, AvaValue[]> fn)
    {
        RegisterNative(name, (vm, args) => { fn(vm, args); return AvaValue.Nil; });
    }

    public void RegisterNative<T1, TResult>(string name, Func<T1, TResult> fn)
    {
        RegisterNative(name, (vm, args) =>
        {
            var a1 = ConvertFrom<T1>(args[0]);
            return ValueExtensions.FromObject(fn(a1));
        });
    }

    public void RegisterNative<T1, T2, TResult>(string name, Func<T1, T2, TResult> fn)
    {
        RegisterNative(name, (vm, args) =>
        {
            var a1 = ConvertFrom<T1>(args[0]);
            var a2 = ConvertFrom<T2>(args[1]);
            return ValueExtensions.FromObject(fn(a1, a2));
        });
    }

    #endregion

    #region Type Conversion

    public T ConvertFrom<T>(AvaValue value)
    {
        var type = typeof(T);

        if (type == typeof(object) || type == typeof(AvaValue))
            return (T)(object)value;

        if (type == typeof(bool))
            return (T)(object)value.AsBool();

        if (type == typeof(int))
            return (T)(object)value.AsInt();

        if (type == typeof(long))
            return (T)(object)value.AsLong();

        if (type == typeof(double))
            return (T)(object)value.AsNumber();

        if (type == typeof(string))
            return (T)(object)GetStringData(value);

        throw new AvaException($"Cannot convert {value.Type} to {type.Name}");
    }

    #endregion

    #region Value Lifecycle

    public void Retain(AvaValue value)
    {
        ThrowIfDisposed();
        if (value.IsRefCounted())
            NativeMethods.ava_value_retain(_handle, ref value);
    }

    public void Release(AvaValue value)
    {
        ThrowIfDisposed();
        if (value.IsRefCounted())
            NativeMethods.ava_value_release(_handle, ref value);
    }

    #endregion

    #region Formatting

    public string FormatValue(AvaValue v)
    {
        return v.Type switch
        {
            AvaValueType.Nil => "nil",
            AvaValueType.Bool => v.AsBool().ToString().ToLower(),
            AvaValueType.Number => v.AsNumber().ToString(System.Globalization.CultureInfo.InvariantCulture),
            AvaValueType.String => $"\"{GetStringData(v)}\"",
            AvaValueType.List => FormatList(v),
            AvaValueType.Dict => FormatDict(v),
            _ => v.ToString()
        };
    }

    private string FormatList(AvaValue v)
    {
        var items = new List<string>();
        int len = GetListLength(v);
        for (int i = 0; i < len; i++)
            items.Add(FormatValue(GetListItem(v, i)));
        return $"[{string.Join(", ", items)}]";
    }

    private string FormatDict(AvaValue v)
    {
        var pairs = new List<string>();
        return $"{{}}";
    }

    #endregion

    private static string? ConsumeError(IntPtr err)
    {
        if (err == IntPtr.Zero) return null;
        string? msg = Marshal.PtrToStringAnsi(err);
        NativeMethods.ava_string_free(err);
        return msg;
    }

    private void ThrowIfDisposed()
    {
        if (_handle == IntPtr.Zero)
            throw new ObjectDisposedException(nameof(AvaVM));
    }

    public void Dispose()
    {
        if (_handle != IntPtr.Zero)
        {
            NativeMethods.ava_vm_destroy(_handle);
            _handle = IntPtr.Zero;
        }
        _keepAlive.Clear();
        GC.SuppressFinalize(this);
    }

    ~AvaVM() => Dispose();
}

public sealed class AvaModule : IDisposable
{
    internal IntPtr Handle { get; }

    internal AvaModule(IntPtr handle)
    {
        Handle = handle;
    }

    public void Dispose()
    {
        NativeMethods.ava_module_destroy(Handle);
        GC.SuppressFinalize(this);
    }

    ~AvaModule() => Dispose();
}

public sealed class AvaCoroutine : IDisposable
{
    internal IntPtr Handle { get; }

    internal AvaCoroutine(IntPtr handle)
    {
        Handle = handle;
    }

    public AvaCoStatus Status(IntPtr vm)
    {
        return (AvaCoStatus)NativeMethods.ava_coroutine_status(vm, Handle);
    }

    public AvaValue Resume(IntPtr vm, params AvaValue[] args)
    {
        IntPtr argsPtr = args.Length > 0
            ? Marshal.AllocHGlobal(args.Length * Marshal.SizeOf<AvaValue>())
            : IntPtr.Zero;
        IntPtr outPtr = Marshal.AllocHGlobal(Marshal.SizeOf<AvaValue>());

        try
        {
            if (argsPtr != IntPtr.Zero)
            {
                for (int i = 0; i < args.Length; i++)
                    Marshal.StructureToPtr(args[i], argsPtr + i * Marshal.SizeOf<AvaValue>(), false);
            }

            UIntPtr outCount;
            int status = NativeMethods.ava_coroutine_resume(
                vm, Handle, argsPtr, (UIntPtr)args.Length,
                outPtr, (UIntPtr)1, out outCount);

            if (status != 0)
                throw new AvaException("Coroutine error");

            if (outCount > 0)
                return Marshal.PtrToStructure<AvaValue>(outPtr);
            return AvaValue.Nil;
        }
        finally
        {
            if (argsPtr != IntPtr.Zero) Marshal.FreeHGlobal(argsPtr);
            Marshal.FreeHGlobal(outPtr);
        }
    }

    public void Dispose()
    {
        NativeMethods.ava_coroutine_destroy(Handle);
        GC.SuppressFinalize(this);
    }

    ~AvaCoroutine() => Dispose();
}

internal static class ValueExtensions
{
    public static AvaValue FromObject(object? obj)
    {
        if (obj == null) return AvaValue.Nil;
        if (obj is bool b) return AvaValue.FromBool(b);
        if (obj is int i) return AvaValue.FromInt(i);
        if (obj is long l) return AvaValue.FromLong(l);
        if (obj is double d) return AvaValue.FromNumber(d);
        if (obj is float f) return AvaValue.FromNumber(f);
        return AvaValue.Nil;
    }
}