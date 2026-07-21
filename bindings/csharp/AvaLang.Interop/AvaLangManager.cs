namespace AvaLang;

public class AvaLangManager : IDisposable
{
    private readonly AvaVM _vm;
    private readonly Dictionary<string, object> _apis = new();

    public AvaLangManager()
    {
        _vm = new AvaVM();
    }

    public AvaVM VM => _vm;

    public void LoadScript(string filePath)
    {
        if (!File.Exists(filePath))
            throw new AvaException($"Script not found: {filePath}");
        
        string source = File.ReadAllText(filePath);
        _vm.Eval(source, Path.GetFileName(filePath));
    }

    public void LoadScriptString(string source, string name = "<script>")
    {
        _vm.Eval(source, name);
    }

    public void ReloadScript(string filePath)
    {
        LoadScript(filePath);
    }

    public T GetGlobal<T>(string name)
    {
        var value = _vm.GetGlobal(name);
        return value.FromAvaValue<T>(_vm);
    }

    public AvaValue GetGlobal(string name) => _vm.GetGlobal(name);

    public void SetGlobal(string name, bool value) => _vm.SetGlobal(name, value);
    public void SetGlobal(string name, int value) => _vm.SetGlobal(name, value);
    public void SetGlobal(string name, long value) => _vm.SetGlobal(name, value);
    public void SetGlobal(string name, double value) => _vm.SetGlobal(name, value);
    public void SetGlobal(string name, string value) => _vm.SetGlobal(name, value);
    public void SetGlobal(string name, AvaValue value) => _vm.SetGlobal(name, value);

    public object? Get(string name)
    {
        var value = _vm.GetGlobal(name);
        return ValueToObject(value);
    }

    public T Get<T>(string name)
    {
        var value = _vm.GetGlobal(name);
        return value.FromAvaValue<T>(_vm);
    }

    public void Set(string name, bool value) => SetGlobal(name, value);
    public void Set(string name, int value) => SetGlobal(name, value);
    public void Set(string name, long value) => SetGlobal(name, value);
    public void Set(string name, double value) => SetGlobal(name, value);
    public void Set(string name, string value) => SetGlobal(name, value);

    private object? ValueToObject(AvaValue value)
    {
        return value.Type switch
        {
            AvaValueType.Nil => null,
            AvaValueType.Bool => value.AsBool(),
            AvaValueType.Number => value.AsNumber(),
            AvaValueType.String => _vm.GetString(value),
            _ => value
        };
    }

    public T Call<T>(string functionName, params object?[] args)
    {
        var fn = _vm.GetGlobal(functionName);
        if (fn.Type == AvaValueType.Nil)
            throw new AvaException($"Function '{functionName}' not found");
        
        var avaArgs = args.Select(a => ObjectToAvaValue(a!)).ToArray();
        var result = _vm.Call(fn, avaArgs);
        return result.FromAvaValue<T>(_vm);
    }

    public AvaValue Call(string functionName, params object?[] args)
    {
        var fn = _vm.GetGlobal(functionName);
        if (fn.Type == AvaValueType.Nil)
            throw new AvaException($"Function '{functionName}' not found");
        
        var avaArgs = args.Select(a => ObjectToAvaValue(a!)).ToArray();
        return _vm.Call(fn, avaArgs);
    }

    public object?[] CallWithArgs(string functionName, params object?[] args)
    {
        var avaArgs = args.Select(a => ObjectToAvaValue(a!)).ToArray();
        var result = Call(functionName, avaArgs);
        return new object?[] { ValueToObject(result) };
    }

    private AvaValue ObjectToAvaValue(object? obj)
    {
        if (obj == null) return AvaValue.Nil;
        if (obj is bool b) return AvaValue.FromBool(b);
        if (obj is int i) return AvaValue.FromInt(i);
        if (obj is long l) return AvaValue.FromLong(l);
        if (obj is double d) return AvaValue.FromNumber(d);
        if (obj is float f) return AvaValue.FromFloat(f);
        if (obj is string s) return _vm.CreateString(s);
        if (obj is AvaValue av) return av;
        return AvaValue.Nil;
    }

    public void RegisterFunction(string name, Func<AvaVM, AvaValue[], AvaValue> function)
    {
        _vm.RegisterNative(name, function);
    }

    public void RegisterFunction(string name, Action function)
    {
        _vm.RegisterNative(name, (vm, args) =>
        {
            function();
            return AvaValue.Nil;
        });
    }

    public void RegisterFunction<T1>(string name, Action<T1> function)
    {
        _vm.RegisterNative(name, (vm, args) =>
        {
            function(args[0].FromAvaValue<T1>(vm));
            return AvaValue.Nil;
        });
    }

    public void RegisterFunction<T1, T2>(string name, Action<T1, T2> function)
    {
        _vm.RegisterNative(name, (vm, args) =>
        {
            function(args[0].FromAvaValue<T1>(vm), args[1].FromAvaValue<T2>(vm));
            return AvaValue.Nil;
        });
    }

    public void RegisterFunction<T1, TResult>(string name, Func<T1, TResult> function)
    {
        _vm.RegisterNative(name, function);
    }

    public void RegisterFunction<T1, T2, TResult>(string name, Func<T1, T2, TResult> function)
    {
        _vm.RegisterNative(name, function);
    }

    public void RegisterObject(string name, object instance)
    {
        _apis[name] = instance;
        
        foreach (var method in instance.GetType().GetMethods())
        {
            if (method.DeclaringType == typeof(object)) continue;
            if (method.IsSpecialName && method.Name.StartsWith("get_")) continue;
            if (method.IsSpecialName && method.Name.StartsWith("set_")) continue;
            
            var methodName = method.Name.ToLower();
            var fullName = $"{name}_{methodName}";
            
            var method1 = method;
            RegisterGenericFunction(fullName, instance, method1);
        }
    }

    private void RegisterGenericFunction(string name, object instance, System.Reflection.MethodInfo method)
    {
        var parameters = method.GetParameters();
        var paramCount = parameters.Length;
        
        switch (paramCount)
        {
            case 0:
                _vm.RegisterNative(name, (vm, args) =>
                {
                    var result = method.Invoke(instance, null);
                    return ObjectToAvaValue(result);
                });
                break;
            case 1:
                _vm.RegisterNative(name, (vm, args) =>
                {
                    var p0 = ConvertParam(parameters[0].ParameterType, args[0], vm);
                    var result = method.Invoke(instance, new[] { p0 });
                    return ObjectToAvaValue(result);
                });
                break;
            case 2:
                _vm.RegisterNative(name, (vm, args) =>
                {
                    var p0 = ConvertParam(parameters[0].ParameterType, args[0], vm);
                    var p1 = ConvertParam(parameters[1].ParameterType, args[1], vm);
                    var result = method.Invoke(instance, new[] { p0, p1 });
                    return ObjectToAvaValue(result);
                });
                break;
            default:
                _vm.RegisterNative(name, (vm, args) =>
                {
                    var invokeArgs = new object[paramCount];
                    for (int i = 0; i < paramCount; i++)
                        invokeArgs[i] = ConvertParam(parameters[i].ParameterType, args[i], vm);
                    var result = method.Invoke(instance, invokeArgs);
                    return ObjectToAvaValue(result);
                });
                break;
        }
    }

    private object ConvertParam(Type type, AvaValue value, AvaVM vm)
    {
        if (type == typeof(bool)) return value.AsBool();
        if (type == typeof(int)) return value.AsInt();
        if (type == typeof(long)) return value.AsLong();
        if (type == typeof(double)) return value.AsNumber();
        if (type == typeof(float)) return value.AsFloat();
        if (type == typeof(string)) return vm.GetString(value);
        return value;
    }

    public void CreateCoroutine(string functionName)
    {
        var fn = _vm.GetGlobal(functionName);
        if (fn.Type == AvaValueType.Nil)
            throw new AvaException($"Function '{functionName}' not found");
        
        _vm.CreateCoroutine(fn);
    }

    public void Dispose()
    {
        _vm.Dispose();
        GC.SuppressFinalize(this);
    }

    ~AvaLangManager() => Dispose();
}

public class AvaScript
{
    private readonly string _source;
    private readonly string _name;

    public AvaScript(string filePath)
    {
        if (!File.Exists(filePath))
            throw new AvaException($"Script not found: {filePath}");
        
        _source = File.ReadAllText(filePath);
        _name = Path.GetFileName(filePath);
    }

    public AvaScript(string source, string name)
    {
        _source = source;
        _name = name;
    }

    public string Source => _source;
    public string Name => _name;
}