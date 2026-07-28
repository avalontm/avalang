#include "core/logger.h"

#include <chrono>
#include <ctime>
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
    std::ostream& out = (level == LogLevel::Error || level == LogLevel::Warn) ? std::cerr : std::cout;
    out << "[" << Timestamp() << "] [" << LevelTag(level) << "] " << message << std::endl;
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

} // namespace avahost
