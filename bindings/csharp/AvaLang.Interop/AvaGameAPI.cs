namespace AvaLang;

public abstract class AvaGameAPI
{
    protected AvaVM VM { get; }

    protected AvaGameAPI(AvaVM vm)
    {
        VM = vm;
    }

    public virtual void OnInit() { }
    public virtual void OnUpdate(double dt) { }
    public virtual void OnDestroy() { }
    public virtual void OnCollision(string other) { }
}

public class DefaultGameAPI : AvaGameAPI
{
    public DefaultGameAPI(AvaVM vm) : base(vm) { }

    public void Log(string message) => Console.WriteLine($"[AvaLang] {message}");

    public void LogWarning(string message) => Console.WriteLine($"[AvaLang Warning] {message}");

    public void LogError(string message) => Console.WriteLine($"[AvaLang Error] {message}");

    public int GetTimeMillis() => Environment.TickCount;

    public double GetTimeSeconds() => Environment.TickCount / 1000.0;

    public void Assert(bool condition, string message = "Assertion failed")
    {
        if (!condition)
            throw new AvaException($"Assertion failed: {message}");
    }
}

public class MathAPI : AvaGameAPI
{
    public MathAPI(AvaVM vm) : base(vm) { }

    public double Abs(double x) => Math.Abs(x);
    public double Floor(double x) => Math.Floor(x);
    public double Ceil(double x) => Math.Ceiling(x);
    public double Round(double x) => Math.Round(x);
    public double Sqrt(double x) => Math.Sqrt(x);
    public double Pow(double x, double y) => Math.Pow(x, y);
    public double Sin(double x) => Math.Sin(x);
    public double Cos(double x) => Math.Cos(x);
    public double Tan(double x) => Math.Tan(x);
    public double Min(double a, double b) => Math.Min(a, b);
    public double Max(double a, double b) => Math.Max(a, b);
    public double Clamp(double value, double min, double max) => Math.Clamp(value, min, max);
    public double Lerp(double a, double b, double t) => a + (b - a) * t;
    public double Random() => System.Random.Shared.NextDouble();
    public int RandomInt(int max) => System.Random.Shared.Next(max);
    public int RandomRange(int min, int max) => System.Random.Shared.Next(min, max);
}

public class DebugAPI : AvaGameAPI
{
    public DebugAPI(AvaVM vm) : base(vm) { }

    public void Print(params AvaValue[] args)
    {
        foreach (var arg in args)
            Console.Write(VM.Inspect(arg) + " ");
        Console.WriteLine();
    }

    public void Dump(AvaValue value)
    {
        Console.WriteLine(VM.Inspect(value));
    }

    public string TypeOf(AvaValue value) => value.Type.ToString();

    public bool IsNil(AvaValue value) => value.Type == AvaValueType.Nil;
    public bool IsNumber(AvaValue value) => value.Type == AvaValueType.Number;
    public bool IsString(AvaValue value) => value.Type == AvaValueType.String;
    public bool IsList(AvaValue value) => value.Type == AvaValueType.List;
    public bool IsDict(AvaValue value) => value.Type == AvaValueType.Dict;
}