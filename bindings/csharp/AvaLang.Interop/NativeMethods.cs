using System.Runtime.InteropServices;

namespace AvaLang;

/// <summary>
/// Bindings crudos 1:1 con include/ava.h. No usar directamente desde
/// fuera de esta libreria: usar la API managed (AvaVM, etc.) en su lugar.
/// </summary>
internal static class NativeMethods
{
    private const string Lib = "avalang"; // resuelve avalang.dll en Windows

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate AvaValue AvaNativeFn(IntPtr vm, IntPtr args, UIntPtr argCount, IntPtr userData);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr ava_vm_create();

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern void ava_vm_destroy(IntPtr vm);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern void ava_vm_register_native(IntPtr vm,
        [MarshalAs(UnmanagedType.LPStr)] string name, AvaNativeFn fn, IntPtr userData);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr ava_compile(IntPtr vm,
        [MarshalAs(UnmanagedType.LPStr)] string source,
        [MarshalAs(UnmanagedType.LPStr)] string sourceName,
        out IntPtr outError);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern void ava_module_destroy(IntPtr module);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern AvaValue ava_run(IntPtr vm, IntPtr module, out IntPtr outError);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern AvaValue ava_call(IntPtr vm, AvaValue callable,
        AvaValue[] args, UIntPtr argCount, out IntPtr outError);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern AvaValue ava_get_global(IntPtr vm,
        [MarshalAs(UnmanagedType.LPStr)] string name);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern void ava_set_global(IntPtr vm,
        [MarshalAs(UnmanagedType.LPStr)] string name, AvaValue value);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern AvaValue ava_string_create(IntPtr vm,
        [MarshalAs(UnmanagedType.LPStr)] string utf8, UIntPtr len);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr ava_string_data(IntPtr vm, AvaValue str, out UIntPtr outLen);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern void ava_value_retain(IntPtr vm, AvaValue value);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern void ava_value_release(IntPtr vm, AvaValue value);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern void ava_string_free(IntPtr s);
}
