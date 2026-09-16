#include "system_module.h"
#include "builtin_shared.h"
#include "vm/vm_platform_accessor.h"

namespace ava {

Value MakeDict() {
    Value v;
    v.type = ValueType::Dict;
    v.obj = new DictObj();
    return v;
}

Value BuildNativeNamespace(const avastd::vector<NativeNamespaceMember>& members) {
    Value ns = MakeDict();
    auto* dict = static_cast<DictObj*>(ns.obj);

    for (const auto& member : members) {
        auto* native = new NativeObj();
        native->fn = member.fn;
        native->user_data = nullptr;

        Value fn_val;
        fn_val.type = ValueType::Native;
        fn_val.obj = native;

        dict->index[member.name] = dict->entries.size();
        dict->entries.emplace_back(member.name, fn_val);
    }

    return ns;
}

void SetDictEntry(Value& dict_val, const avastd::string& key, Value entry) {
    auto* dict = static_cast<DictObj*>(dict_val.obj);
    auto it = dict->index.find(key);
    if (it != dict->index.end()) {
        dict->entries[it->second].second = entry;
    } else {
        dict->index[key] = dict->entries.size();
        dict->entries.emplace_back(key, entry);
    }
}

namespace {

struct CivilDate {
    int64_t year;
    int month;
    int day;
};

CivilDate CivilFromDays(int64_t z) {
    z += 719468;
    int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned doe = static_cast<unsigned>(z - era * 146097);
    unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int64_t y = static_cast<int64_t>(yoe) + era * 400;
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    unsigned mp = (5 * doy + 2) / 153;
    unsigned d = doy - (153 * mp + 2) / 5 + 1;
    int m = static_cast<int>(mp) + (mp < 10 ? 3 : -9);
    y += (m <= 2) ? 1 : 0;
    return {y, m, static_cast<int>(d)};
}

struct BrokenDownTime {
    int64_t year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    int millisecond;
    int day_of_week;
};

BrokenDownTime BreakDownEpochMs(int64_t epoch_ms) {

    int64_t days = epoch_ms >= 0 ? epoch_ms / 86400000 : (epoch_ms - 86399999) / 86400000;
    int64_t ms_of_day = epoch_ms - days * 86400000;

    CivilDate cd = CivilFromDays(days);

    BrokenDownTime t;
    t.year = cd.year;
    t.month = cd.month;
    t.day = cd.day;
    t.hour = static_cast<int>(ms_of_day / 3600000);
    t.minute = static_cast<int>((ms_of_day / 60000) % 60);
    t.second = static_cast<int>((ms_of_day / 1000) % 60);
    t.millisecond = static_cast<int>(ms_of_day % 1000);
    t.day_of_week = static_cast<int>(((days % 7) + 7 + 4) % 7);
    return t;
}

avastd::string ZeroPad(int64_t value, int width) {
    bool negative = value < 0;
    avastd::string s = avastd::to_string(negative ? -value : value);
    while (static_cast<int>(s.size()) < width) s = "0" + s;
    return negative ? ("-" + s) : s;
}

Value BuildDateTimeDict(int64_t epoch_ms) {
    BrokenDownTime t = BreakDownEpochMs(epoch_ms);

    Value dt = MakeDict();
    SetDictEntry(dt, "Year", Value::Number(static_cast<double>(t.year)));
    SetDictEntry(dt, "Month", Value::Number(t.month));
    SetDictEntry(dt, "Day", Value::Number(t.day));
    SetDictEntry(dt, "Hour", Value::Number(t.hour));
    SetDictEntry(dt, "Minute", Value::Number(t.minute));
    SetDictEntry(dt, "Second", Value::Number(t.second));
    SetDictEntry(dt, "Millisecond", Value::Number(t.millisecond));
    SetDictEntry(dt, "DayOfWeek", Value::Number(t.day_of_week));
    SetDictEntry(dt, "Ticks", Value::Number(static_cast<double>(epoch_ms)));
    return dt;
}

double DictGetNumber(const Value& dict_val, const avastd::string& key) {
    if (dict_val.type != ValueType::Dict) return 0.0;
    auto* dict = static_cast<DictObj*>(dict_val.obj);
    auto it = dict->index.find(key);
    if (it == dict->index.end()) return 0.0;
    return AsNumber(dict->entries[it->second].second);
}

double ArgAsNumber(const ava_value_t* args, size_t count, size_t index, double def = 0.0) {
    if (index >= count) return def;
    return AsNumber(FromC(args[index]));
}

ava_value_t datetime_now(AvaVM*, const ava_value_t*, size_t, void*) {
    return ToCNew(BuildDateTimeDict(VmPlatformAccessor::Get().Clock().NowMs()));
}

ava_value_t datetime_utc_now(AvaVM*, const ava_value_t*, size_t, void*) {
    return ToCNew(BuildDateTimeDict(VmPlatformAccessor::Get().Clock().NowMs()));
}

ava_value_t datetime_to_string(AvaVM*, const ava_value_t* args, size_t count, void*) {
    if (count == 0) return ToC(Value::Nil());
    Value dt = FromC(args[0]);
    if (dt.type != ValueType::Dict) return ToC(Value::Nil());

    int64_t year = static_cast<int64_t>(DictGetNumber(dt, "Year"));
    int month = static_cast<int>(DictGetNumber(dt, "Month"));
    int day = static_cast<int>(DictGetNumber(dt, "Day"));
    int hour = static_cast<int>(DictGetNumber(dt, "Hour"));
    int minute = static_cast<int>(DictGetNumber(dt, "Minute"));
    int second = static_cast<int>(DictGetNumber(dt, "Second"));
    int millisecond = static_cast<int>(DictGetNumber(dt, "Millisecond"));

    avastd::string s = ZeroPad(year, 4) + "-" + ZeroPad(month, 2) + "-" + ZeroPad(day, 2) +
                        "T" + ZeroPad(hour, 2) + ":" + ZeroPad(minute, 2) + ":" + ZeroPad(second, 2) +
                        "." + ZeroPad(millisecond, 3) + "Z";
    return ToCNew(Value::String(s));
}

// System.Threading.Thread.Sleep(ms) en C# real -- NUNCA existio bajo
// DateTime (el error CS0117 del usuario lo confirma). El nombre de la
// funcion C se deja neutral (no "datetime_sleep") porque ahora vive bajo
// el namespace Thread, no DateTime.
ava_value_t thread_sleep(AvaVM*, const ava_value_t* args, size_t count, void*) {
    double ms = ArgAsNumber(args, count, 0, 0.0);
    if (ms < 0) ms = 0;
    VmPlatformAccessor::Get().Clock().SleepMs(static_cast<uint32_t>(ms));
    return ToC(Value::Nil());
}

Value BuildDateTimeNamespace() {
    return BuildNativeNamespace({
        {"Now", datetime_now},
        {"UtcNow", datetime_utc_now},
        {"ToString", datetime_to_string},
    });
}

// System.Threading.Thread en C# real: Sleep(ms) vive aca, no en DateTime.
Value BuildThreadNamespace() {
    return BuildNativeNamespace({
        {"Sleep", thread_sleep},
    });
}

avastd::string ArgAsDisplayString(const ava_value_t* args, size_t count, size_t index) {
    if (index >= count) return avastd::string();
    return ToDisplayString(FromC(args[index]));
}

struct ConsoleColorEntry {
    const char* key;   
    const char* value; 
    platform::ConsoleColor color;
};

const avastd::vector<ConsoleColorEntry>& ConsoleColorTable() {
    static const avastd::vector<ConsoleColorEntry> table = {
        {"Default", "default", platform::ConsoleColor::Default},
        {"Black",   "black",   platform::ConsoleColor::Black},
        {"Red",     "red",     platform::ConsoleColor::Red},
        {"Green",   "green",   platform::ConsoleColor::Green},
        {"Yellow",  "yellow",  platform::ConsoleColor::Yellow},
        {"Blue",    "blue",    platform::ConsoleColor::Blue},
        {"Magenta", "magenta", platform::ConsoleColor::Magenta},
        {"Cyan",    "cyan",    platform::ConsoleColor::Cyan},
        {"White",   "white",   platform::ConsoleColor::White},
    };
    return table;
}

bool TryParseConsoleColor(const avastd::string& name, platform::ConsoleColor& out) {
    for (const auto& entry : ConsoleColorTable()) {
        if (name == entry.value) {
            out = entry.color;
            return true;
        }
    }
    return false;
}

Value BuildConsoleColorsConstants() {
    Value colors = MakeDict();
    for (const auto& entry : ConsoleColorTable()) {
        SetDictEntry(colors, entry.key, Value::String(entry.value));
    }
    return colors;
}

ava_value_t console_write(AvaVM* vm, const ava_value_t* args, size_t count, void*) {
    reinterpret_cast<ava::VM*>(vm)->Print(ArgAsDisplayString(args, count, 0));
    return ToC(Value::Nil());
}

ava_value_t console_write_line(AvaVM* vm, const ava_value_t* args, size_t count, void*) {
    reinterpret_cast<ava::VM*>(vm)->Print(ArgAsDisplayString(args, count, 0) + "\n");
    return ToC(Value::Nil());
}

ava_value_t console_write_error(AvaVM*, const ava_value_t* args, size_t count, void*) {
    VmPlatformAccessor::Get().Console().WriteError(ArgAsDisplayString(args, count, 0));
    return ToC(Value::Nil());
}

ava_value_t console_read_line(AvaVM* vm, const ava_value_t*, size_t, void*) {
    avastd::string line = reinterpret_cast<ava::VM*>(vm)->ReadLine(avastd::string());
    return ToCNew(Value::String(line));
}

ava_value_t console_foreground_color(AvaVM*, const ava_value_t* args, size_t count, void*) {
    avastd::string name = ArgAsDisplayString(args, count, 0);
    platform::ConsoleColor color;
    if (!TryParseConsoleColor(name, color)) {
        return ToC(Value::Bool(false));
    }
    VmPlatformAccessor::Get().Console().SetForegroundColor(color);
    return ToC(Value::Bool(true));
}

ava_value_t console_reset_color(AvaVM*, const ava_value_t*, size_t, void*) {
    VmPlatformAccessor::Get().Console().ResetColor();
    return ToC(Value::Nil());
}

Value BuildConsoleNamespace() {
    Value console_ns = BuildNativeNamespace({
        {"Write", console_write},
        {"WriteLine", console_write_line},
        {"WriteError", console_write_error},
        {"ReadLine", console_read_line},
        {"ForegroundColor", console_foreground_color},
        {"ResetColor", console_reset_color},
    });
    SetDictEntry(console_ns, "Colors", BuildConsoleColorsConstants());
    return console_ns;
}

ava_value_t environment_get_variable(AvaVM*, const ava_value_t* args, size_t count, void*) {
    avastd::string name = ArgAsDisplayString(args, count, 0);
    avastd::string value;
    if (!VmPlatformAccessor::Get().Environment().GetEnvVar(name, value)) {
        return ToC(Value::Nil());
    }
    return ToCNew(Value::String(value));
}

ava_value_t environment_set_variable(AvaVM*, const ava_value_t* args, size_t count, void*) {
    avastd::string name = ArgAsDisplayString(args, count, 0);
    avastd::string value = ArgAsDisplayString(args, count, 1);
    bool ok = VmPlatformAccessor::Get().Environment().SetEnvVar(name, value);
    return ToC(Value::Bool(ok));
}

ava_value_t environment_get_current_directory(AvaVM*, const ava_value_t*, size_t, void*) {
    return ToCNew(Value::String(VmPlatformAccessor::Get().Environment().GetCurrentDirectory()));
}

ava_value_t environment_set_current_directory(AvaVM*, const ava_value_t* args, size_t count, void*) {
    avastd::string path = ArgAsDisplayString(args, count, 0);
    bool ok = VmPlatformAccessor::Get().Environment().SetCurrentDirectory(path);
    return ToC(Value::Bool(ok));
}

ava_value_t environment_get_command_line_args(AvaVM*, const ava_value_t*, size_t, void*) {
    Value list_val;
    list_val.type = ValueType::List;
    list_val.obj = new ListObj();
    auto* list = static_cast<ListObj*>(list_val.obj);

    for (const auto& arg : VmPlatformAccessor::Get().Environment().GetCommandLineArgs()) {
        list->items.push_back(Value::String(arg));
    }

    return ToCNew(list_val);
}

Value BuildEnvironmentNamespace() {
    // Nota: "SetCurrentDirectory" se saco de aca -- en C# real no existe
    // Environment.SetCurrentDirectory (Environment.CurrentDirectory es
    // una propiedad con setter, no un metodo con ese nombre). El metodo
    // estatico SetCurrentDirectory(path) que si existe de verdad es
    // System.IO.Directory.SetCurrentDirectory -- ver BuildDirectoryNamespace.
    return BuildNativeNamespace({
        {"GetEnvironmentVariable", environment_get_variable},
        {"SetEnvironmentVariable", environment_set_variable},
        {"GetCurrentDirectory", environment_get_current_directory},
        {"GetCommandLineArgs", environment_get_command_line_args},
    });
}

ava_value_t file_read_all_text(AvaVM*, const ava_value_t* args, size_t count, void*) {
    avastd::string path = ArgAsDisplayString(args, count, 0);
    avastd::string content;
    if (!VmPlatformAccessor::Get().FileSystem().ReadFile(path, content)) {
        return ToC(Value::Nil());
    }
    return ToCNew(Value::String(content));
}

ava_value_t file_write_all_text(AvaVM*, const ava_value_t* args, size_t count, void*) {
    avastd::string path = ArgAsDisplayString(args, count, 0);
    avastd::string content = ArgAsDisplayString(args, count, 1);
    bool ok = VmPlatformAccessor::Get().FileSystem().WriteFile(path, content);
    return ToC(Value::Bool(ok));
}

ava_value_t file_delete(AvaVM*, const ava_value_t* args, size_t count, void*) {
    avastd::string path = ArgAsDisplayString(args, count, 0);
    bool ok = VmPlatformAccessor::Get().FileSystem().DeleteFile(path);
    return ToC(Value::Bool(ok));
}

ava_value_t file_exists(AvaVM*, const ava_value_t* args, size_t count, void*) {
    avastd::string path = ArgAsDisplayString(args, count, 0);
    auto& fs = VmPlatformAccessor::Get().FileSystem();
    bool ok = fs.Exists(path) && !fs.IsDirectory(path);
    return ToC(Value::Bool(ok));
}

ava_value_t file_size(AvaVM*, const ava_value_t* args, size_t count, void*) {
    avastd::string path = ArgAsDisplayString(args, count, 0);
    int64_t size = VmPlatformAccessor::Get().FileSystem().FileSize(path);
    if (size < 0) return ToC(Value::Nil());
    return ToC(Value::Number(static_cast<double>(size)));
}

Value BuildFileNamespace() {
    return BuildNativeNamespace({
        {"ReadAllText", file_read_all_text},
        {"WriteAllText", file_write_all_text},
        {"Delete", file_delete},
        {"Exists", file_exists},
        {"Size", file_size},
    });
}

ava_value_t directory_create(AvaVM*, const ava_value_t* args, size_t count, void*) {
    avastd::string path = ArgAsDisplayString(args, count, 0);
    bool ok = VmPlatformAccessor::Get().FileSystem().CreateDirectory(path);
    return ToC(Value::Bool(ok));
}

ava_value_t directory_delete(AvaVM*, const ava_value_t* args, size_t count, void*) {
    avastd::string path = ArgAsDisplayString(args, count, 0);
    bool ok = VmPlatformAccessor::Get().FileSystem().DeleteDirectory(path);
    return ToC(Value::Bool(ok));
}

ava_value_t directory_exists(AvaVM*, const ava_value_t* args, size_t count, void*) {
    avastd::string path = ArgAsDisplayString(args, count, 0);
    auto& fs = VmPlatformAccessor::Get().FileSystem();
    bool ok = fs.Exists(path) && fs.IsDirectory(path);
    return ToC(Value::Bool(ok));
}

ava_value_t directory_enumerate(AvaVM*, const ava_value_t* args, size_t count, void*) {
    avastd::string path = ArgAsDisplayString(args, count, 0);
    avastd::vector<platform::DirEntry> entries;
    if (!VmPlatformAccessor::Get().FileSystem().EnumerateDirectory(path, entries)) {
        return ToC(Value::Nil());
    }

    Value list_val;
    list_val.type = ValueType::List;
    list_val.obj = new ListObj();
    auto* list = static_cast<ListObj*>(list_val.obj);

    for (const auto& entry : entries) {
        Value item = MakeDict();
        SetDictEntry(item, "Name", Value::String(entry.name));
        SetDictEntry(item, "IsDirectory", Value::Bool(entry.is_directory));
        list->items.push_back(item);
    }

    return ToCNew(list_val);
}

Value BuildDirectoryNamespace() {
    return BuildNativeNamespace({
        // Antes decia "Create" -- en C# real ese nombre corto es de
        // File.Create (crea un archivo). El metodo real de Directory se
        // llama CreateDirectory(path).
        {"CreateDirectory", directory_create},
        {"Delete", directory_delete},
        {"Exists", directory_exists},
        {"Enumerate", directory_enumerate},
        // Movidos aca desde Environment (ver BuildEnvironmentNamespace):
        // los estaticos reales System.IO.Directory.GetCurrentDirectory()
        // y Directory.SetCurrentDirectory(path) viven en esta clase, no
        // en Environment.
        {"GetCurrentDirectory", environment_get_current_directory},
        {"SetCurrentDirectory", environment_set_current_directory},
    });
}

Value BuildIONamespace() {
    Value io_ns = MakeDict();
    SetDictEntry(io_ns, "File", BuildFileNamespace());
    SetDictEntry(io_ns, "Directory", BuildDirectoryNamespace());
    return io_ns;
}

avastd::vector<avastd::string> ArgAsStringList(const ava_value_t* args, size_t count, size_t index) {
    avastd::vector<avastd::string> out;
    if (index >= count) return out;
    Value v = FromC(args[index]);
    if (v.type != ValueType::List) return out;
    auto* list = static_cast<ListObj*>(v.obj);
    for (const auto& item : list->items) {
        out.push_back(ToDisplayString(item));
    }
    return out;
}

ava_value_t process_get_current_id(AvaVM*, const ava_value_t*, size_t, void*) {
    uint64_t pid = VmPlatformAccessor::Get().Process().CurrentProcessId();
    return ToC(Value::Number(static_cast<double>(pid)));
}

ava_value_t process_start(AvaVM*, const ava_value_t* args, size_t count, void*) {
    avastd::string command = ArgAsDisplayString(args, count, 0);
    avastd::vector<avastd::string> proc_args = ArgAsStringList(args, count, 1);

    platform::ProcessResult result;
    bool ok = VmPlatformAccessor::Get().Process().Execute(command, proc_args, result);
    if (!ok) return ToC(Value::Nil());

    Value out = MakeDict();
    SetDictEntry(out, "ExitCode", Value::Number(static_cast<double>(result.exit_code)));
    SetDictEntry(out, "Stdout", Value::String(result.stdout_output));
    SetDictEntry(out, "Stderr", Value::String(result.stderr_output));
    return ToCNew(out);
}

Value BuildProcessNamespace() {
    return BuildNativeNamespace({
        {"Start", process_start},
        {"GetCurrentId", process_get_current_id},
    });
}

Value BuildDiagnosticsNamespace() {
    Value diagnostics_ns = MakeDict();
    SetDictEntry(diagnostics_ns, "Process", BuildProcessNamespace());
    return diagnostics_ns;
}

} // namespace

void RegisterSystemModule(VM& vm) {

    // Registrado en minuscula ("system") y en mayuscula ("System",
    // como en el "using System;" real de C#) apuntando a los MISMOS
    // builders -- import System (mayuscula) es la forma "correcta"/
    // recomendada de ahora en mas, pero se deja "system" funcionando
    // igual para no romper scripts ya escritos con minuscula.
    auto register_both_cases = [&vm](const char* lower, const char* upper, VM::NativeModuleFactory fn) {
        vm.RegisterNativeModule(lower, fn);
        vm.RegisterNativeModule(upper, fn);
    };

    register_both_cases("system", "System", [](VM&) -> Value {
        Value root = MakeDict();

        SetDictEntry(root, "Console", BuildConsoleNamespace());
        SetDictEntry(root, "DateTime", BuildDateTimeNamespace());
        SetDictEntry(root, "Thread", BuildThreadNamespace());
        SetDictEntry(root, "Environment", BuildEnvironmentNamespace());
        SetDictEntry(root, "IO", BuildIONamespace());
        SetDictEntry(root, "Diagnostics", BuildDiagnosticsNamespace());

        return root;
    });

    register_both_cases("system.console", "System.Console", [](VM&) -> Value {
        return BuildConsoleNamespace();
    });
    register_both_cases("system.datetime", "System.DateTime", [](VM&) -> Value {
        return BuildDateTimeNamespace();
    });
    register_both_cases("system.thread", "System.Thread", [](VM&) -> Value {
        return BuildThreadNamespace();
    });
    register_both_cases("system.environment", "System.Environment", [](VM&) -> Value {
        return BuildEnvironmentNamespace();
    });
    register_both_cases("system.io", "System.IO", [](VM&) -> Value {
        return BuildIONamespace();
    });
    register_both_cases("system.diagnostics", "System.Diagnostics", [](VM&) -> Value {
        return BuildDiagnosticsNamespace();
    });
}

} // namespace ava

