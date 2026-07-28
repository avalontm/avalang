#pragma once
// AvaHost.Core -- logging abstraction. No HTTP/UI/compiler knowledge here.
#include <memory>
#include <string>

namespace avahost {

enum class LogLevel { Trace, Debug, Info, Warn, Error };

class Logger {
public:
    virtual ~Logger() = default;
    virtual void Log(LogLevel level, const std::string& message) = 0;

    void Trace(const std::string& m) { Log(LogLevel::Trace, m); }
    void Debug(const std::string& m) { Log(LogLevel::Debug, m); }
    void Info(const std::string& m)  { Log(LogLevel::Info, m); }
    void Warn(const std::string& m)  { Log(LogLevel::Warn, m); }
    void Error(const std::string& m) { Log(LogLevel::Error, m); }
};

// Default logger: writes to stdout/stderr with a level tag and timestamp.
class ConsoleLogger : public Logger {
public:
    explicit ConsoleLogger(LogLevel minLevel = LogLevel::Info) : minLevel_(minLevel) {}
    void Log(LogLevel level, const std::string& message) override;

private:
    LogLevel minLevel_;
};

// Process-wide logger instance used by subsystems that don't have one
// injected explicitly (CLI commands, static helpers). Host code that
// wants a testable logger should still prefer taking a Logger& directly.
Logger& GlobalLogger();
void SetGlobalLogger(std::shared_ptr<Logger> logger);

} // namespace avahost
