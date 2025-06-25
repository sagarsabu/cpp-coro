#include <ctime>

#include "log/logger.hpp"

namespace Sage
{

namespace Logger
{

void SetupLogger(const std::string& filename, Level logLevel) { Internal::GetLogStreamer().Setup(filename, logLevel); }

namespace Internal
{

// Formatter control

constexpr std::string_view FORMAT_END{ "\x1B[00m" };
constexpr std::string_view FORMAT_BOLD{ "\x1B[01m" };
constexpr std::string_view FORMAT_DISABLED{ "\x1B[02m" };
constexpr std::string_view FORMAT_ITALIC{ "\x1B[03m" };
constexpr std::string_view FORMAT_URL{ "\x1B[04m" };
constexpr std::string_view FORMAT_BLINK{ "\x1B[05m" };
constexpr std::string_view FORMAT_BLINK2{ "\x1B[06m" };
constexpr std::string_view FORMAT_SELECTED{ "\x1B[07m" };
constexpr std::string_view FORMAT_INVISIBLE{ "\x1B[08m" };
constexpr std::string_view FORMAT_STRIKE{ "\x1B[09m" };
constexpr std::string_view FORMAT_DOUBLE_UNDERLINE{ "\x1B[21m" };

// Dark Colours

constexpr std::string_view DARK_BLACK{ "\x1B[30m" };
constexpr std::string_view DARK_RED{ "\x1B[31m" };
constexpr std::string_view DARK_GREEN{ "\x1B[32m" };
constexpr std::string_view DARK_YELLOW{ "\x1B[33m" };
constexpr std::string_view DARK_BLUE{ "\x1B[34m" };
constexpr std::string_view DARK_VIOLET{ "\x1B[35m" };
constexpr std::string_view DARK_BEIGE{ "\x1B[36m" };
constexpr std::string_view DARK_WHITE{ "\x1B[37m" };

// Light Colours

constexpr std::string_view LIGHT_GREY{ "\x1B[90m" };
constexpr std::string_view LIGHT_RED{ "\x1B[91m" };
constexpr std::string_view LIGHT_GREEN{ "\x1B[92m" };
constexpr std::string_view LIGHT_YELLOW{ "\x1B[93m" };
constexpr std::string_view LIGHT_BLUE{ "\x1B[94m" };
constexpr std::string_view LIGHT_VIOLET{ "\x1B[95m" };
constexpr std::string_view LIGHT_BEIGE{ "\x1B[96m" };
constexpr std::string_view LIGHT_WHITE{ "\x1B[97m" };

// Helper classes / structs

// Global variables

constexpr std::array<std::string_view, Level::Critical + 1> LEVEL_COLOURS{
    LIGHT_GREEN,  // Level::Trace
    DARK_BLUE,    // Level::Debug
    DARK_WHITE,   // Level::Info
    LIGHT_YELLOW, // Level::Warning
    LIGHT_RED,    // Level::Error
    DARK_RED,     // Level::Critical
};

constexpr std::array<std::string_view, Level::Critical + 1> LEVEL_NAMES{
    "TRACE", // Level::Trace
    "DEBUG", // Level::Debug
    "INFO ", // Level::Info
    "WARN ", // Level::Warning
    "ERROR", // Level::Error
    "CRIT ", // Level::Critical
};

// Functions

LogTimestamp GetCurrentTimeStamp() noexcept
{
    LogTimestamp ts;
    timespec timeSpec{};
    std::tm localTime{};

    std::timespec_get(&timeSpec, TIME_UTC);
    std::strftime(ts.m_s, sizeof(ts.m_s), "%d-%m-%Y %H:%M:%S", ::localtime_r(&timeSpec.tv_sec, &localTime));
    snprintf(ts.m_ns, sizeof(ts.m_ns), ":%09lu", timeSpec.tv_nsec);

    return ts;
}

std::string_view GetLevelFormatter(Level level) noexcept { return LEVEL_COLOURS[level]; }

std::string_view GetLevelName(Level level) noexcept { return LEVEL_NAMES[level]; }

std::string_view GetFormatEnd() noexcept { return FORMAT_END; }

std::string_view CurrentThreadName() noexcept
{
    // Max allowed buffer for POSIX thread name
    using ThreadNameBuffer = char[16];

    static thread_local std::string threadName{};
    if (threadName.empty())
    {
        ThreadNameBuffer buff{};
        pthread_getname_np(pthread_self(), buff, sizeof(buff));

        // centered thread name output
        threadName = std::format("{:^17s}", buff);
    }

    return threadName;
}

} // namespace Internal

} // namespace Logger

} // namespace Sage
