#include "core/logger.h"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace avahost {

namespace {
const char* LevelTag(LogLevel level) {
    switch (level) {
        case LogLevel::Trace: return "TRC";
        case LogLevel::Debug: return "DBG";
        case LogLevel::Info:  return "INF";
        case LogLevel::Warn:  return "WRN";
        case LogLevel::Error: return "ERR";
    }
    return "???";
}

std::string Timestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tmBuf{};
#if defined(_WIN32)
    localtime_s(&tmBuf, &t);
#else
    localtime_r(&t, &tmBuf);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tmBuf, "%H:%M:%S");
    return oss.str();
}
} // namespace

void ConsoleLogger::Log(LogLevel level, const std::string& message) {
    if (static_cast<int>(level) < static_cast<int>(minLevel_)) return;
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostream& out = (level == LogLevel::Error || level == LogLevel::Warn) ? std::cerr : std::cout;
    out << "[" << Timestamp() << "] [" << LevelTag(level) << "] " << message << std::endl;
}

namespace {
// A full date+time (not just Timestamp()'s HH:MM:SS) -- a log file is
// meant to be read hours/days later, possibly across a process restart,
// so entries need to disambiguate by day too.
std::string FullTimestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tmBuf{};
#if defined(_WIN32)
    localtime_s(&tmBuf, &t);
#else
    localtime_r(&t, &tmBuf);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tmBuf, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}
} // namespace

FileLogger::FileLogger(const std::string& filePath, LogLevel minLevel) : minLevel_(minLevel) {
    // std::ios::app: every AvaHost run (and every crash-restart) appends
    // rather than truncating, so the file keeps a history of past
    // failures instead of only ever showing the most recent one.
    auto stream = std::make_shared<std::ofstream>(filePath, std::ios::out | std::ios::app);
    if (stream->is_open()) {
        stream_ = stream;
        *stream << "----- AvaHost log opened " << FullTimestamp() << " -----" << std::endl;
    }
    // If it failed to open, stream_ stays null and Log() below becomes a
    // silent no-op -- a bad log path must never stop AvaHost from
    // starting or from serving requests.
}

void FileLogger::Log(LogLevel level, const std::string& message) {
    if (!stream_) return;
    if (static_cast<int>(level) < static_cast<int>(minLevel_)) return;
    std::lock_guard<std::mutex> lock(mutex_);
    auto* stream = static_cast<std::ofstream*>(stream_.get());
    if (!stream->is_open()) return;
    (*stream) << "[" << FullTimestamp() << "] [" << LevelTag(level) << "] " << message << std::endl;
    stream->flush();
}

namespace {
std::shared_ptr<Logger>& GlobalLoggerPtr() {
    static std::shared_ptr<Logger> logger = std::make_shared<ConsoleLogger>();
    return logger;
}
} // namespace

Logger& GlobalLogger() { return *GlobalLoggerPtr(); }

void SetGlobalLogger(std::shared_ptr<Logger> logger) {
    if (logger) GlobalLoggerPtr() = std::move(logger);
}

void EnableErrorFileLogging(const std::string& logFilePath) {
    auto fileLogger = std::make_shared<FileLogger>(logFilePath, LogLevel::Error);
    auto combined = std::make_shared<TeeLogger>(GlobalLoggerPtr(), fileLogger);
    GlobalLoggerPtr() = combined;
}

} // namespace avahost
