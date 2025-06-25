#include <filesystem>

#include "log/log_stream.hpp"
#include "log/logger.hpp"

namespace Sage::Logger::Internal
{

void LogStreamer::Setup(const std::string& filename, Level level)
{
    m_logFilename = filename;
    m_logLevel = level;

    if (m_logFilename.empty())
    {
        SetStreamToConsole();
        return;
    }

    // try setup a file logger if specified
    try
    {
        if (std::filesystem::exists(m_logFilename) and not std::filesystem::is_regular_file(m_logFilename))
        {
            throw std::runtime_error("cannot write to non regular file '" + m_logFilename + "'");
        }

        std::ofstream file{ m_logFilename, std::ios::out | std::ios::ate | std::ios::app };
        std::filesystem::permissions(
            m_logFilename,
            std::filesystem::perms::owner_write | std::filesystem::perms::group_read,
            std::filesystem::perm_options::add
        );

        if (file.fail())
        {
            throw std::runtime_error("unable to open file '" + m_logFilename + "' for writing");
        }

        SetStreamToFile(std::move(file));
    }
    catch (const std::exception& e)
    {
        LOG_CRITICAL("==== failed to setup file logger. what: {} ====", e.what());
    }
}

void LogStreamer::EnsureLogFileWriteable()
{
    if (m_logFilename.empty())
    {
        return;
    }

    try
    {
        // everything is okay
        if (std::filesystem::exists(m_logFilename) and std::filesystem::is_regular_file(m_logFilename))
        {
            return;
        }

        // No longer writing to log file
        if (std::filesystem::exists(m_logFilename) and not std::filesystem::is_regular_file(m_logFilename))
        {
            std::println(std::cerr, "cannot write to non regular file '{}'", m_logFilename);
            return;
        }

        std::ofstream file{ m_logFilename, std::ios::out | std::ios::ate | std::ios::app };
        std::filesystem::permissions(
            m_logFilename,
            std::filesystem::perms::owner_write | std::filesystem::perms::group_read,
            std::filesystem::perm_options::add
        );

        if (file.fail())
        {
            std::println(std::cerr, "unable to open file '{}' for writing", m_logFilename);
            return;
        }

        SetStreamToFile(std::move(file));
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        std::println(std::cerr, "file system error when attempting to recreate file logger. e: {}", e.what());
    }
}

void LogStreamer::SetStreamToConsole()
{
    std::lock_guard lk{ m_streamSetMutex };
    m_logFileStream = {};
    m_streamRef = s_consoleStream;
}

void LogStreamer::SetStreamToFile(std::ofstream fileStream)
{
    std::lock_guard lk{ m_streamSetMutex };
    m_logFileStream = std::move(fileStream);
    m_streamRef = m_logFileStream;
}

/**
 * Intentionally leaking here.
 * Logging in global object destructor's may cause issue if we destroy the logger
 */
LogStreamer* const g_logStreamer{ new LogStreamer };

LogStreamer& GetLogStreamer() noexcept { return *g_logStreamer; }

} // namespace Sage::Logger::Internal
