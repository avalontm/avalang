using System.Runtime.InteropServices;

namespace AvaLang;

public enum AvaCoStatus
{
    Suspended = 0,
    Running = 1,
    Dead = 2
}

public static class NativeMethods
{
    private const string Lib = "avalang";

    public const CallingConvention Cdecl = CallingConvention.Cdecl;

    [UnmanagedFunctionPointer(Cdecl)]
    public delegate AvaValue AvaNativeFn(IntPtr vm, IntPtr args, UIntPtr argCount, IntPtr userData);

    [DllImport(Lib, CallingConvention = Cdecl, CharSet = CharSet.Ansi)]
    public static extern IntPtr ava_vm_create();

    [DllImport(Lib, CallingConvention = Cdecl)]
    public static extern void ava_vm_destroy(IntPtr vm);

    [DllImport(Lib, CallingConvention = Cdecl, CharSet = CharSet.Ansi)]
    public static extern void ava_vm_register_native(
        IntPtr vm,
        string name,
        AvaNativeFn fn,
        IntPtr userData);

    [DllImport(Lib, CallingConvention = Cdecl, CharSet = CharSet.Ansi)]
    public static extern IntPtr ava_compile(
        IntPtr vm,
        string source,
        string sourceName,
        out IntPtr outError);

    [DllImport(Lib, CallingConvention = Cdecl)]
    public static extern void ava_module_destroy(IntPtr module);

    [DllImport(Lib, CallingConvention = Cdecl)]
    public static extern void ava_run(IntPtr vm, IntPtr module, IntPtr outResult, out IntPtr outError);

    [DllImport(Lib, CallingConvention = Cdecl)]
    public static extern void ava_call(IntPtr vm, AvaValue callable, IntPtr args, UIntPtr argCount, IntPtr outResult, out IntPtr outError);

    [DllImport(Lib, CallingConvention = Cdecl, CharSet = CharSet.Ansi)]
    public static extern AvaValue ava_get_global(IntPtr vm, string name);

    [DllImport(Lib, CallingConvention = Cdecl, CharSet = CharSet.Ansi)]
    public static extern void ava_set_global(IntPtr vm, string name, ref AvaValue value);

    [DllImport(Lib, CallingConvention = Cdecl, CharSet = CharSet.Ansi)]
    public static extern IntPtr ava_import(
        IntPtr vm,
        string modulePath,
        string alias,
        out IntPtr outError);

    [DllImport(Lib, CallingConvention = Cdecl, CharSet = CharSet.Ansi)]
    public static extern AvaValue ava_string_create(IntPtr vm, string utf8, UIntPtr len);

    [DllImport(Lib, CallingConvention = Cdecl)]
    public static extern IntPtr ava_string_data(IntPtr vm, ref AvaValue str, out UIntPtr outLen);

    [DllImport(Lib, CallingConvention = Cdecl)]
    public static extern IntPtr ava_list_create(IntPtr vm);

    [DllImport(Lib, CallingConvention = Cdecl)]
    public static extern void ava_list_append(IntPtr vm, ref AvaValue list, ref AvaValue item);

    [DllImport(Lib, CallingConvention = Cdecl)]
    public static extern UIntPtr ava_list_length(IntPtr vm, ref AvaValue list);

    [DllImport(Lib, CallingConvention = Cdecl)]
    public static extern AvaValue ava_list_get(IntPtr vm, ref AvaValue list, UIntPtr index);

    [DllImport(Lib, CallingConvention = Cdecl)]
    public static extern void ava_list_set(IntPtr vm, ref AvaValue list, UIntPtr index, ref AvaValue value);

    [DllImport(Lib, CallingConvention = Cdecl)]
    public static extern void ava_list_insert(IntPtr vm, ref AvaValue list, UIntPtr index, ref AvaValue item);

    [DllImport(Lib, CallingConvention = Cdecl)]
    public static extern void ava_list_remove(IntPtr vm, ref AvaValue list, UIntPtr index);

    [DllImport(Lib, CallingConvention = Cdecl)]
    public static extern IntPtr ava_dict_create(IntPtr vm);

    [DllImport(Lib, CallingConvention = Cdecl, CharSet = CharSet.Ansi)]
    public static extern void ava_dict_set(IntPtr vm, ref AvaValue dict, string key, ref AvaValue value);

    [DllImport(Lib, CallingConvention = Cdecl, CharSet = CharSet.Ansi)]
    public static extern AvaValue ava_dict_get(IntPtr vm, ref AvaValue dict, string key);

    [DllImport(Lib, CallingConvention = Cdecl)]
    public static extern UIntPtr ava_dict_length(IntPtr vm, ref AvaValue dict);

    [DllImport(Lib, CallingConvention = Cdecl, CharSet = CharSet.Ansi)]
    [return: MarshalAs(UnmanagedType.I1)]
    public static extern bool ava_dict_contains(IntPtr vm, AvaValue dict, string key, UIntPtr keyLen);

    [DllImport(Lib, CallingConvention = Cdecl)]
    public static extern UIntPtr ava_dict_entries(IntPtr vm, AvaValue dict, out IntPtr outEntries);

    [DllImport(Lib, CallingConvention = Cdecl, CharSet = CharSet.Ansi)]
    public static extern void ava_dict_remove(IntPtr vm, ref AvaValue dict, string key);

    [DllImport(Lib, CallingConvention = Cdecl)]
    public static extern void ava_value_retain(IntPtr vm, ref AvaValue value);

    [DllImport(Lib, CallingConvention = Cdecl)]
    public static extern void ava_value_release(IntPtr vm, ref AvaValue value);

    [DllImport(Lib, CallingConvention = Cdecl)]
    public static extern void ava_string_free(IntPtr s);

    [DllImport(Lib, CallingConvention = Cdecl)]
    public static extern IntPtr ava_coroutine_create(IntPtr vm, ref AvaValue function);

    [DllImport(Lib, CallingConvention = Cdecl)]
    public static extern void ava_coroutine_destroy(IntPtr co);

    [DllImport(Lib, CallingConvention = Cdecl)]
    public static extern int ava_coroutine_resume(
        IntPtr vm,
        IntPtr co,
        IntPtr args,
        UIntPtr argCount,
        IntPtr outValues,
        UIntPtr outCapacity,
        out UIntPtr outCount);

    [DllImport(Lib, CallingConvention = Cdecl)]
    public static extern int ava_coroutine_status(IntPtr vm, IntPtr co);
}