#include "system_module.h"
#include "builtin_shared.h"
#include "vm/vm_platform_accessor.h"

namespace ava {

// ToCNew() (Retain() + ToC() for a freshly-created String/List/Dict
// Value) now lives in builtin_shared.h so builtin_natives.cpp,
// builtin_mem.cpp, and c_api_ui.cpp can share it too -- see the comment
// there for the full story. It used to be defined only in this file.

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

// Fase 3 - System.DateTime. The one area with real new work, since
// nothing in the codebase decomposes an epoch into a calendar date
// yet (see plan). Uses the days<->civil-date algorithm popularized by
// Howard Hinnant (http://howardhinnant.github.io/date_algorithms.html,
// public domain) -- proleptic Gregorian, valid across the entire
// int64_t range CivilFromDays takes, re-derived and re-typed here
// rather than pulled in as a dependency, and checked against known
// dates (epoch, a leap day, a year-1 and a year-9999 boundary) before
// wiring it into DateTime.Now/UtcNow below.

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
    int day_of_week;  // 0 = Sunday .. 6 = Saturday, same convention as JS/C#
};

BrokenDownTime BreakDownEpochMs(int64_t epoch_ms) {
    // Floor division toward negative infinity (plain integer division
    // truncates toward zero, which is wrong for ms before the epoch)
    // -- the -86399999 offset is the standard trick to floor a
    // negative numerator without branching on the remainder.
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
    // 1970-01-01 was a Thursday -- index 4 in a Sunday=0 week.
    t.day_of_week = static_cast<int>(((days % 7) + 7 + 4) % 7);
    return t;
}

avastd::string ZeroPad(int64_t value, int width) {
    bool negative = value < 0;
    avastd::string s = avastd::to_string(negative ? -value : value);
    while (static_cast<int>(s.size()) < width) s = "0" + s;
    return negative ? ("-" + s) : s;
}

// "Ticks" here is epoch milliseconds (same unit as
// IClock::NowMs()/this Dict's own inputs), NOT .NET's real
// DateTime.Ticks (100ns intervals since 0001-01-01) -- named to match
// the field list the plan called out (Year/.../Ticks) but documented
// here so nothing downstream assumes .NET-compatible arithmetic on
// it.
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

// DateTime.Now() and DateTime.UtcNow() are the SAME call underneath:
// IClock::NowMs() is already UTC on every backend (CLOCK_REALTIME on
// Linux, GetSystemTimeAsFileTime on Windows -- both epoch-UTC, see
// LinClock.cpp/WinClock.cpp), and there is no timezone/offset source
// anywhere in the PAL to turn that into genuine local time (gap
// flagged already in the plan for this phase). Rather than fabricate
// an offset or silently mislabel UTC as local, Now() is documented as
// UTC-in-practice here -- same "explicit gap over silent guess" call
// as Environment.Exit() in Fase 4.

ava_value_t datetime_now(AvaVM*, const ava_value_t*, size_t, void*) {
    return ToCNew(BuildDateTimeDict(VmPlatformAccessor::Get().Clock().NowMs()));
}

ava_value_t datetime_utc_now(AvaVM*, const ava_value_t*, size_t, void*) {
    return ToCNew(BuildDateTimeDict(VmPlatformAccessor::Get().Clock().NowMs()));
}

// Fixed-format ISO 8601 (YYYY-MM-DDTHH:MM:SS.mmmZ) -- not
// parametrizable yet, same first-cut scope the plan calls out.
// DateTime.ToString(dt) is a namespace-level function taking the Dict
// Now()/UtcNow() returns, rather than a method attached to the Dict
// itself -- same Fase-0 reasoning that kept CurrentDirectory/
// ForegroundColor as get/set functions instead of real properties: no
// VM-level instance-method sugar on plain Dicts yet.
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

// Fase 0's "decide in this phase, don't duplicate" call: Sleep(ms)
// lives on DateTime (thin wrap of IClock::SleepMs) instead of waiting
// for a hypothetical future System.Threading -- CKM_CAP_THREADS=0
// today (see plan's own "fuera de alcance" section), so there is no
// such namespace to put it in, and IClock is already the PAL owner of
// this call.
ava_value_t datetime_sleep(AvaVM*, const ava_value_t* args, size_t count, void*) {
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
        {"Sleep", datetime_sleep},
    });
}

// Fase 2 - System.Console.
//
// Reads a single argument as display text the same way `print`
// already does (ToDisplayString, shared via builtin_shared.h) --
// System.Console.WriteLine(42) prints "42" instead of failing, same
// as print(42) already does. Missing argument -> empty string, same
// permissive convention every other builtin here already follows.
avastd::string ArgAsDisplayString(const ava_value_t* args, size_t count, size_t index) {
    if (index >= count) return avastd::string();
    return ToDisplayString(FromC(args[index]));
}

// Console.ForegroundColor(color) / Console.Colors.* representation
// (Fase 0 decision deferred to here, see plan): a lowercase string
// name ("red", "blue", ...) rather than integer constants -- readable
// at the call site and doesn't require the VM to support any new kind
// of constant. Console.Colors.Red etc. are the same strings, spelled
// out so scripts don't have to hardcode literals or guess the exact
// spelling.
struct ConsoleColorEntry {
    const char* key;   // Console.Colors.<key>, PascalCase (matches ConsoleColor's C++ enumerator names)
    const char* value;  // what ForegroundColor(...) actually takes, lowercase
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

// Wraps IConsole 1:1 (Write/WriteLine/WriteError/ReadLine/
// SetForegroundColor/ResetColor), going through
// VmPlatformAccessor::Get() -- the same single entry point to the PAL
// that builtin_print/builtin_input already use (builtin_natives.cpp),
// so print()/input() and System.Console.* never diverge: both sit on
// top of the exact same IConsole instance.

ava_value_t console_write(AvaVM*, const ava_value_t* args, size_t count, void*) {
    VmPlatformAccessor::Get().Console().Write(ArgAsDisplayString(args, count, 0));
    return ToC(Value::Nil());
}

ava_value_t console_write_line(AvaVM*, const ava_value_t* args, size_t count, void*) {
    VmPlatformAccessor::Get().Console().WriteLine(ArgAsDisplayString(args, count, 0));
    return ToC(Value::Nil());
}

ava_value_t console_write_error(AvaVM*, const ava_value_t* args, size_t count, void*) {
    VmPlatformAccessor::Get().Console().WriteError(ArgAsDisplayString(args, count, 0));
    return ToC(Value::Nil());
}

// Returns nil on EOF (IConsole::ReadLine's own false-on-EOF signal)
// instead of a garbage/empty string that would look indistinguishable
// from an actual blank line read from input.
ava_value_t console_read_line(AvaVM*, const ava_value_t*, size_t, void*) {
    avastd::string line;
    bool ok = VmPlatformAccessor::Get().Console().ReadLine(line);
    if (!ok) return ToC(Value::Nil());
    return ToCNew(Value::String(line));
}

// Returns false (instead of raising) on an unrecognized color name --
// same permissive-on-bad-input convention as the rest of this file --
// so a script can check the return value instead of every call site
// needing a try/catch for a typo'd color name.
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

// Fase 4 - System.Environment. Wraps IEnvironment 1:1, same
// VmPlatformAccessor::Get() entry point as Console above.
//
// GetEnvironmentVariable/SetEnvironmentVariable name the C# way
// (Environment.GetEnvironmentVariable / .SetEnvironmentVariable) since
// the plan calls those out by name explicitly; CurrentDirectory (a
// property in C#) is exposed as a get/set method pair for the same
// Fase-0 reason ConsoleColor stayed a string instead of a real
// property: no VM-level getter-without-call sugar yet.
//
// Exit(code) is deliberately NOT wired here: unlike every other
// member in this file, there is no IEnvironment/IProcess member for
// "terminate the current process" -- IProcess::Execute launches a
// *different* process, it doesn't end this one. The only way to do
// that today is a raw libc exit(), which doesn't exist in the
// freestanding barekernel build this file also has to compile for
// (see ava_cstdio.h -- no exit() wrapper in avastd on purpose).
// Adding it means designing a new PAL member first, which is a
// bigger change than "wire an existing PAL call" like the rest of
// this plan -- left as a documented gap, same as the plan's own
// Fase 5.5 streaming gap, not a silent omission.

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
    return BuildNativeNamespace({
        {"GetEnvironmentVariable", environment_get_variable},
        {"SetEnvironmentVariable", environment_set_variable},
        {"GetCurrentDirectory", environment_get_current_directory},
        {"SetCurrentDirectory", environment_set_current_directory},
        {"GetCommandLineArgs", environment_get_command_line_args},
    });
}

// Fase 5 - System.IO. Wraps IFileSystem, split into File/Directory
// submodules the same way C#'s System.IO does, same
// VmPlatformAccessor::Get() entry point as Console/Environment above.
//
// Failure convention matches Environment.GetEnvironmentVariable: nil
// (not false, not a sentinel number) wherever the underlying PAL call
// can fail and the script needs to tell "worked" from "didn't" --
// File.ReadAllText and File.Size return nil instead of an empty
// string / -1 (IFileSystem::FileSize's own not-found sentinel, see
// LinFileSystem.cpp/WinFileSystem.cpp) so a typo'd path doesn't read
// as "0-byte file" or "empty file". Delete/Create/WriteAllText return
// plain bool, same as Environment.SetEnvironmentVariable.
//
// IFileSystem::Exists() alone doesn't distinguish files from
// directories (true for either) -- File.Exists/Directory.Exists are
// deliberately narrowed with IsDirectory() so they match the real
// System.IO.File.Exists/Directory.Exists contract (a directory path
// reads as false from File.Exists, and vice versa) instead of both
// namespaces sharing one file-or-dir boolean.
//
// Directory.Enumerate(path) returns nil on failure (bad/missing path,
// same convention as the rest of this section) or, on success, a List
// of Dict{Name, IsDirectory} -- one entry per DirEntry from
// IFileSystem::EnumerateDirectory, field names PascalCase to match
// Console.Colors.* above rather than mirroring the internal C++
// struct's lowercase field names.
//
// vm_file.cpp's VM::RunFile also touches IFileSystem, but only to
// load .ava module source for `import` -- an internal implementation
// detail of the compiler, not a user-facing API -- so there's no
// second surface for scripts to read/write files that could diverge
// from this one (checked per the plan's own note for this phase).

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
        {"Create", directory_create},
        {"Delete", directory_delete},
        {"Exists", directory_exists},
        {"Enumerate", directory_enumerate},
    });
}

Value BuildIONamespace() {
    Value io_ns = MakeDict();
    SetDictEntry(io_ns, "File", BuildFileNamespace());
    SetDictEntry(io_ns, "Directory", BuildDirectoryNamespace());
    return io_ns;
}

// Fase 5.5 - System.Diagnostics.Process. Wraps IProcess
// (CurrentProcessId/Execute), same VmPlatformAccessor::Get() entry
// point as the rest of this file. Nested two levels
// (Diagnostics.Process.*) to match System.Diagnostics.Process in C#,
// unlike Console/Environment/IO which sit directly under the root.
//
// Process.Start(command, args) is deliberately the *blocking* form
// only (IProcess::Execute) -- the plan's own streaming variant
// (IProcessStream::ExecuteStreaming, additive on top of IProcess, see
// IProcessStream.h) needs a callback bridged back into an AvaNativeFn
// call, which is real added complexity for a capability barekernel's
// own BareKernelProcess::Execute doesn't even fill in yet (stdout_output/
// stderr_output come back empty there -- documented gap already noted
// in the plan for Fase 5.5, not new). Left for a later pass instead of
// half-wiring a callback path against a backend that can't exercise it.
//
// Execute() returning false means the process could not even be
// launched (bad command, not just a nonzero exit code) -- Start
// returns nil in that case, same nil-on-failure convention as
// File.ReadAllText/Directory.Enumerate above, rather than a fabricated
// exit code. A process that *launches* and exits nonzero still comes
// back as a normal Dict with that exit code in it.

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
    // Console (Phase 2), Environment (Phase 4), IO (Phase 5),
    // Diagnostics.Process (Phase 5.5) and DateTime (Phase 3, above)
    // are all wired to the PAL now -- every child of "system" this
    // plan scoped in is real.
    vm.RegisterNativeModule("system", [](VM&) -> Value {
        Value root = MakeDict();

        SetDictEntry(root, "Console", BuildConsoleNamespace());
        SetDictEntry(root, "DateTime", BuildDateTimeNamespace());
        SetDictEntry(root, "Environment", BuildEnvironmentNamespace());
        SetDictEntry(root, "IO", BuildIONamespace());
        SetDictEntry(root, "Diagnostics", BuildDiagnosticsNamespace());

        return root;
    });

    // Fase 6 - dotted submodule imports (`import system.io`, etc.),
    // the sugar the plan left optional. Each is a SEPARATE
    // native_modules_ entry (not a re-derivation of "system" above)
    // returning just that one area's Dict -- same
    // BuildXNamespace() helpers "system" itself uses, so there is no
    // second copy of Console/DateTime/Environment/IO/Diagnostics that
    // could drift from the flat `import system` form.
    //
    // Left out on purpose: "system.diagnostics.process" (3 segments).
    // PlaceModuleInScope/SetNestedNamespace (this same file's
    // existing, already-relied-on placement logic for ordinary
    // file-backed dotted imports, unchanged by this plan) only nests
    // correctly for a 2-segment path -- for 3+ segments today it
    // recurses into SetNestedNamespace with the *top-level* globals
    // map at every level instead of descending into the namespace
    // Dict it just created, so the deepest segment lands back at
    // global scope instead of nested. That is pre-existing behavior
    // of the general import mechanism (reachable today with any
    // 3-level file import path, e.g. `import a.b.c`), not something
    // introduced by this plan, and fixing it means changing shared
    // import-placement logic every existing file-backed import also
    // depends on -- out of scope for "wire system.* sugar" and risks
    // a regression well beyond this plan. `import system.diagnostics`
    // (2 segments, below) already reaches Process fully via
    // `system.Process.*`, so the capability isn't missing, only the
    // 3-level spelling of it.
    vm.RegisterNativeModule("system.console", [](VM&) -> Value {
        return BuildConsoleNamespace();
    });
    vm.RegisterNativeModule("system.datetime", [](VM&) -> Value {
        return BuildDateTimeNamespace();
    });
    vm.RegisterNativeModule("system.environment", [](VM&) -> Value {
        return BuildEnvironmentNamespace();
    });
    vm.RegisterNativeModule("system.io", [](VM&) -> Value {
        return BuildIONamespace();
    });
    vm.RegisterNativeModule("system.diagnostics", [](VM&) -> Value {
        return BuildDiagnosticsNamespace();
    });
}

} // namespace ava

