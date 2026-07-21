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

    public AvaModule Compile(string source, string sourceName = "<script>")
    {
        ThrowIfDisposed();
        IntPtr module = NativeMethods.ava_compile(_handle, source, sourceName, out IntPtr err);
        if (module == IntPtr.Zero)
        {
            string? msg = ConsumeError(err);
            throw new AvaException(msg ?? "Compile error");
        }
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

    public AvaValue GetGlobal(string name) => NativeMethods.ava_get_global(_handle, name);

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
        return result.FromAvaValue<T>(this);
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

    public AvaValue CreateNil() => AvaValue.Nil;
    public AvaValue CreateBool(bool value) => AvaValue.FromBool(value);
    public AvaValue CreateNumber(double value) => AvaValue.FromNumber(value);
    public AvaValue CreateNumber(int value) => AvaValue.FromInt(value);

    public AvaValue CreateString(string? value)
    {
        ThrowIfDisposed();
        if (value == null) return AvaValue.Nil;
        return NativeMethods.ava_string_create(_handle, value, (UIntPtr)value.Length);
    }

    public AvaList CreateList() => new(CreateListValue(), this);

    public AvaList CreateList(params AvaValue[] items)
    {
        var list = CreateList();
        foreach (var item in items)
            list.Add(item);
        return list;
    }

    private AvaValue CreateListValue()
    {
        ThrowIfDisposed();
        return AvaValue.FromList((ulong)NativeMethods.ava_list_create(_handle).ToInt64());
    }

    public AvaDict CreateDict() => new(CreateDictValue(), this);

    private AvaValue CreateDictValue()
    {
        ThrowIfDisposed();
        return AvaValue.FromDict((ulong)NativeMethods.ava_dict_create(_handle).ToInt64());
    }

    public string GetString(AvaValue str)
    {
        ThrowIfDisposed();
        IntPtr data = NativeMethods.ava_string_data(_handle, ref str, out UIntPtr len);
        return Marshal.PtrToStringUTF8(data, (int)len) ?? string.Empty;
    }

    public int GetListLength(AvaValue list) => (int)NativeMethods.ava_list_length(_handle, ref list);
    public AvaValue GetListItem(AvaValue list, int index) => NativeMethods.ava_list_get(_handle, ref list, (UIntPtr)index);
    public void SetListItem(AvaValue list, int index, AvaValue value) => NativeMethods.ava_list_set(_handle, ref list, (UIntPtr)index, ref value);
    public void ListAppend(AvaValue list, AvaValue item) => NativeMethods.ava_list_append(_handle, ref list, ref item);

    public int GetDictLength(AvaValue dict) => (int)NativeMethods.ava_dict_length(_handle, ref dict);
    public AvaValue GetDictItem(AvaValue dict, string key) => NativeMethods.ava_dict_get(_handle, ref dict, key);
    public bool DictContains(AvaValue dict, string key) => NativeMethods.ava_dict_contains(_handle, dict, key, (UIntPtr)key.Length);
    public void DictSet(AvaValue dict, string key, AvaValue value) => NativeMethods.ava_dict_set(_handle, ref dict, key, ref value);

    public void RegisterNative(string name, Func<AvaVM, AvaValue[], AvaValue> fn)
    {
        ThrowIfDisposed();
        var vmRef = this;
        NativeMethods.AvaNativeFn thunk = (vm, argsPtr, argCount, userData) =>
        {
            int count = (int)argCount;
            AvaValue[] args = count > 0 ? new AvaValue[count] : Array.Empty<AvaValue>();
            for (int i = 0; i < count; i++)
                args[i] = Marshal.PtrToStructure<AvaValue>(argsPtr + i * Marshal.SizeOf<AvaValue>());
            return fn(vmRef, args);
        };
        _keepAlive.Add(thunk);
        NativeMethods.ava_vm_register_native(_handle, name, thunk, IntPtr.Zero);
    }

    public void RegisterNative(string name, Action<AvaVM, AvaValue[]> fn) =>
        RegisterNative(name, (vm, args) => { fn(vm, args); return AvaValue.Nil; });

    public void RegisterNative<T1, TResult>(string name, Func<T1, TResult> fn)
    {
        RegisterNative(name, (vm, args) => AvaConversions.ObjectToAva(fn(args[0].FromAvaValue<T1>(vm)), vm));
    }

    public void RegisterNative<T1, T2, TResult>(string name, Func<T1, T2, TResult> fn)
    {
        RegisterNative(name, (vm, args) =>
        {
            var a1 = args[0].FromAvaValue<T1>(vm);
            var a2 = args[1].FromAvaValue<T2>(vm);
            return AvaConversions.ObjectToAva(fn(a1, a2), vm);
        });
    }

    public T ConvertFrom<T>(AvaValue value) => value.FromAvaValue<T>(this);

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

    public string Inspect(AvaValue v) => FormatValue(v);

    private string FormatValue(AvaValue v) => v.Type switch
    {
        AvaValueType.Nil => "nil",
        AvaValueType.Bool => v.AsBool().ToString().ToLower(),
        AvaValueType.Number => v.AsNumber().ToString(System.Globalization.CultureInfo.InvariantCulture),
        AvaValueType.String => $"\"{GetString(v)}\"",
        AvaValueType.List => FormatList(v),
        AvaValueType.Dict => FormatDict(v),
        _ => v.ToString()
    };

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
        return "{}";
    }

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

    internal AvaModule(IntPtr handle) => Handle = handle;

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

    internal AvaCoroutine(IntPtr handle) => Handle = handle;

    public AvaCoStatus Status(IntPtr vm) => (AvaCoStatus)NativeMethods.ava_coroutine_status(vm, Handle);

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

            int status = NativeMethods.ava_coroutine_resume(
                vm, Handle, argsPtr, (UIntPtr)args.Length,
                outPtr, (UIntPtr)1, out UIntPtr outCount);

            if (status != 0)
                throw new AvaException("Coroutine error");

            return outCount > 0 ? Marshal.PtrToStructure<AvaValue>(outPtr) : AvaValue.Nil;
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

internal static class PipeExtensions
{
    public static R Pipe<T, R>(this T value, Func<T, R> func) => func(value);
}