#include "async_aliases.hpp"
#include "log/logger.hpp"

void handleBoostError(Sage::Logger::Level level, const boost::system::system_error& e, const std::source_location& src)
{
    // task cancelled
    if (e.code() == boost::system::errc::operation_canceled)
    {
        return;
    }

    Sage::Logger::Internal::LogToStream(
        level,
        std::source_location::current(),
        "boost exception caught src='{}:{}' e='{}'",
        src.file_name(),
        src.line(),
        e.what()
    );
}

void detached_log_exception::operator()(std::exception_ptr e) const
{
    if (not e)
    {
        return;
    }

    try
    {
        std::rethrow_exception(e);
    }
    catch (const boost::asio::multiple_exceptions& multiExc)
    {
        if (not multiExc.first_exception())
        {
            return;
        }

        try
        {
            std::rethrow_exception(multiExc.first_exception());
        }
        catch (const boost::system::system_error& boostExc)
        {
            handleBoostError(m_level, boostExc, m_src);
        }
    }
    catch (const boost::system::system_error& boostExc)
    {
        handleBoostError(m_level, boostExc, m_src);
    }
    catch (const std::exception& stdExc)
    {
        Sage::Logger::Internal::LogToStream(
            m_level,
            std::source_location::current(),
            "exception caught src='{}:{}' e='{}'",
            m_src.file_name(),
            m_src.line(),
            stdExc.what()
        );
    }
}
