using System.Runtime.InteropServices;

namespace AvaLang;

/// <summary>
/// Modulo compilado (bytecode). Se libera automaticamente al hacer Dispose,
/// o al hacer Dispose de la AvaVM que lo creo.
/// </summary>
public sealed class AvaModule : IDisposable
{
    internal IntPtr Handle;
    internal AvaModule(IntPtr handle) => Handle = handle;

    public void Dispose()
    {
        if (Handle != IntPtr.Zero)
        {
            NativeMethods.ava_module_destroy(Handle);
            Handle = IntPtr.Zero;
        }
        GC.SuppressFinalize(this);
    }
}

/// <summary>
/// Wrapper managed de una instancia de VM de AvaLang (un "isolate").
/// Uso tipico:
/// <code>
/// using var vm = new AvaVM();
/// using var module = vm.Compile(source);
/// var result = vm.Run(module);
/// </code>
/// </summary>
public sealed class AvaVM : IDisposable
{
    private IntPtr _handle;

    // Mantiene vivos los delegates de funciones nativas registradas,
    // si no el GC los puede recolectar mientras la VM aun los usa.
    private readonly List<NativeMethods.AvaNativeFn> _keepAlive = new();

    public AvaVM()
    {
        _handle = NativeMethods.ava_vm_create();
        if (_handle == IntPtr.Zero)
            throw new AvaException("ava_vm_create failed");
    }

    public AvaModule Compile(string source, string sourceName = "<script>")
    {
        ThrowIfDisposed();
        IntPtr module = NativeMethods.ava_compile(_handle, source, sourceName, out IntPtr err);
        if (module == IntPtr.Zero)
            throw new AvaException(ConsumeError(err, "unknown compile error"));
        return new AvaModule(module);
    }

    public AvaValue Run(AvaModule module)
    {
        ThrowIfDisposed();
        AvaValue result = NativeMethods.ava_run(_handle, module.Handle, out IntPtr err);
        if (err != IntPtr.Zero)
            throw new AvaException(ConsumeError(err, "unknown runtime error"));
        return result;
    }

    public AvaValue GetGlobal(string name)
    {
        ThrowIfDisposed();
        return NativeMethods.ava_get_global(_handle, name);
    }

    public void SetGlobal(string name, AvaValue value)
    {
        ThrowIfDisposed();
        NativeMethods.ava_set_global(_handle, name, value);
    }

    public AvaValue CreateString(string s)
    {
        ThrowIfDisposed();
        var bytes = System.Text.Encoding.UTF8.GetBytes(s);
        return NativeMethods.ava_string_create(_handle, s, (UIntPtr)bytes.Length);
    }

    public string GetStringData(AvaValue str)
    {
        ThrowIfDisposed();
        IntPtr ptr = NativeMethods.ava_string_data(_handle, str, out UIntPtr len);
        return Marshal.PtrToStringUTF8(ptr, (int)len);
    }

    /// <summary>
    /// Registra una funcion host (C#) callable desde scripts AvaLang.
    /// El delegate se mantiene vivo mientras viva esta AvaVM.
    /// </summary>
    public void RegisterNative(string name, Func<AvaValue[], AvaValue> fn)
    {
        ThrowIfDisposed();
        NativeMethods.AvaNativeFn thunk = (vm, argsPtr, argCount, userData) =>
        {
            int count = (int)argCount;
            var args = new AvaValue[count];
            int size = Marshal.SizeOf<AvaValue>();
            for (int i = 0; i < count; i++)
                args[i] = Marshal.PtrToStructure<AvaValue>(argsPtr + i * size);
            return fn(args);
        };
        _keepAlive.Add(thunk); // evita que el GC lo recolecte
        NativeMethods.ava_vm_register_native(_handle, name, thunk, IntPtr.Zero);
    }

    private static string ConsumeError(IntPtr err, string fallback)
    {
        if (err == IntPtr.Zero) return fallback;
        string msg = Marshal.PtrToStringAnsi(err) ?? fallback;
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
        GC.SuppressFinalize(this);
    }
}
