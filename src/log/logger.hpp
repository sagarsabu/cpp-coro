#pragma once

#include <format>
#include <ostream>
#include <print>
#include <source_location>
#include <string>
#include <syncstream>

#include "log/log_levels.hpp"
#include "log/log_stream.hpp"

namespace Sage
{

namespace Logger
{

void SetupLogger(const std::string& filename = "", Level logLevel = Level::Info);

namespace Internal
{

constexpr std::string_view GetFilenameStem(std::string_view fileName) noexcept
{
    std::string_view fnameStem{ fileName };
    const size_t pos{ fnameStem.find_last_of('/') };

    if (pos == std::string_view::npos)
    {
        return fnameStem;
    }

    return fnameStem.substr(pos + 1);
}

std::string_view GetLevelFormatter(Level level) noexcept;

std::string_view GetLevelName(Level level) noexcept;

std::string_view GetFormatEnd() noexcept;

struct LogTimestamp
{
    // e.g "01 - 09 - 2023 00:42 : 19"
    using SecondsBuffer = char[26];
    // :%09lu requires 22 bytes max
    using NanoSecBuffer = char[22];

    SecondsBuffer m_s;
    NanoSecBuffer m_ns;
};

LogTimestamp GetCurrentTimeStamp() noexcept;

std::string_view CurrentThreadName() noexcept;

inline bool ShouldLog(Level level) noexcept { return level >= GetLogStreamer().GetLogLevel(); }

template<Level LOG_LEVEl, typename... Args>
inline void LogToStream(const std::source_location& loc, std::format_string<Args...> fmt, Args&&... args)
{
    if (not Internal::ShouldLog(LOG_LEVEl))
        return;

    LogTimestamp ts{ GetCurrentTimeStamp() };

    std::osyncstream stream{ GetLogStreamer().GetStream() };
    std::println(
        stream,
        "{}[{}{}] [{}] [{}] [{}:{}] {}{}",
        GetLevelFormatter(LOG_LEVEl),
        ts.m_s,
        ts.m_ns,
        CurrentThreadName(),
        GetLevelName(LOG_LEVEl),
        GetFilenameStem(loc.file_name()),
        loc.line(),
        std::format(fmt, std::forward_like<Args>(args)...),
        GetFormatEnd()
    );
    std::flush(stream);
}

constexpr void Noop() {}

} // namespace Internal

} // namespace Logger

} // namespace Sage

// Log marcos for lazy va args evaluation

#define LOG_TRACE(fmt, ...)                                                                                            \
    if (Sage::Logger::Internal::ShouldLog(Sage::Logger::Trace)) [[unlikely]]                                           \
    Sage::Logger::Internal::LogToStream<Sage::Logger::Trace>(std::source_location::current(), fmt, ##__VA_ARGS__)

#define LOG_DEBUG(fmt, ...)                                                                                            \
    if (Sage::Logger::Internal::ShouldLog(Sage::Logger::Debug)) [[unlikely]]                                           \
    Sage::Logger::Internal::LogToStream<Sage::Logger::Debug>(std::source_location::current(), fmt, ##__VA_ARGS__)

#define LOG_INFO(fmt, ...)                                                                                             \
    Sage::Logger::Internal::LogToStream<Sage::Logger::Info>(std::source_location::current(), fmt, ##__VA_ARGS__)

#define LOG_WARNING(fmt, ...)                                                                                          \
    Sage::Logger::Internal::LogToStream<Sage::Logger::Warning>(std::source_location::current(), fmt, ##__VA_ARGS__)

#define LOG_ERROR(fmt, ...)                                                                                            \
    Sage::Logger::Internal::LogToStream<Sage::Logger::Error>(std::source_location::current(), fmt, ##__VA_ARGS__)

#define LOG_CRITICAL(fmt, ...)                                                                                         \
    Sage::Logger::Internal::LogToStream<Sage::Logger::Critical>(std::source_location::current(), fmt, ##__VA_ARGS__)

#define LOG_IF(check, logMacro)                                                                                        \
    if ((check)) [[unlikely]]                                                                                          \
    logMacro("#### " #check " #### - check failed")

#define LOG_RETURN_IF(check, logMacro)                                                                                 \
    if ((check)) [[unlikely]]                                                                                          \
    {                                                                                                                  \
        logMacro("#### " #check " #### - check failed");                                                               \
        return;                                                                                                        \
    }                                                                                                                  \
    Sage::Logger::Internal::Noop()

#define LOG_RETURN_FALSE_IF(check, logMacro)                                                                           \
    if ((check)) [[unlikely]]                                                                                          \
    {                                                                                                                  \
        logMacro("#### " #check " #### - check failed");                                                               \
        return false;                                                                                                  \
    }                                                                                                                  \
    Sage::Logger::Internal::Noop()

#define LOG_RETURN_NULL_IF(check, logMacro)                                                                            \
    if ((check)) [[unlikely]]                                                                                          \
    {                                                                                                                  \
        logMacro("#### " #check " #### - check failed");                                                               \
        return nullptr;                                                                                                \
    }                                                                                                                  \
    Sage::Logger::Internal::Noop()
