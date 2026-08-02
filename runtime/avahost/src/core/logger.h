#pragma once
// AvaHost.Core -- logging abstraction. No HTTP/UI/compiler knowledge here.
#include <memory>
#include <mutex>
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
// Thread-safe: uses mutex to serialize output.
class ConsoleLogger : public Logger {
public:
    explicit ConsoleLogger(LogLevel minLevel = LogLevel::Info) : minLevel_(minLevel) {}
    void Log(LogLevel level, const std::string& message) override;

private:
    LogLevel minLevel_;
    mutable std::mutex mutex_;
};

// Appends to a log file on disk, flushing after every line. Exists so a
// crash (hard native crash, not a catchable std::exception) still leaves
// a record on disk even if the console window/process is gone by the
// time anyone looks -- ConsoleLogger alone only helps if someone was
// watching the terminal at the exact moment it happened.
// Thread-safe: uses mutex to serialize file writes.
class FileLogger : public Logger {
public:
    // `filePath` is opened in append mode immediately; if it can't be
    // opened (bad path, no permissions) this degrades to a no-op logger
    // rather than throwing, so a broken log path never prevents AvaHost
    // itself from starting.
    explicit FileLogger(const std::string& filePath, LogLevel minLevel = LogLevel::Error);
    void Log(LogLevel level, const std::string& message) override;

private:
    LogLevel minLevel_;
    std::shared_ptr<void> stream_; // opaque std::ofstream, kept out of the header
    mutable std::mutex mutex_;
};

// Forwards every log call to two loggers. Used to keep the existing
// console output (useful while the process is alive and the terminal is
// visible) while also durably persisting to a FileLogger.
class TeeLogger : public Logger {
public:
    TeeLogger(std::shared_ptr<Logger> first, std::shared_ptr<Logger> second)
        : first_(std::move(first)), second_(std::move(second)) {}
    void Log(LogLevel level, const std::string& message) override {
        if (first_) first_->Log(level, message);
        if (second_) second_->Log(level, message);
    }

private:
    std::shared_ptr<Logger> first_;
    std::shared_ptr<Logger> second_;
};

// Process-wide logger instance used by subsystems that don't have one
// injected explicitly (CLI commands, static helpers). Host code that
// wants a testable logger should still prefer taking a Logger& directly.
Logger& GlobalLogger();
void SetGlobalLogger(std::shared_ptr<Logger> logger);

// Wraps whatever logger is currently global in a TeeLogger that also
// writes to `logFilePath` (append mode, Error level and above only --
// this is meant to capture "what went wrong", not a full activity log).
// Safe to call more than once; each call re-wraps the *current* global
// logger, so call it exactly once at startup, before anything else logs.
void EnableErrorFileLogging(const std::string& logFilePath);

} // namespace avahost
