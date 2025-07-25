#pragma once

#include <boost/asio.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <exception>
#include <source_location>

#include "log/log_levels.hpp"

namespace asio = boost::asio;
namespace ssl = boost::asio::ssl;
namespace beast = boost::beast;

struct detached_log_exception
{
    explicit detached_log_exception(
        Sage::Logger::Level level = Sage::Logger::Level::Info,
        const std::source_location& src = std::source_location::current()
    ) :
        m_level{ level },
        m_src{ src }
    {
    }

    Sage::Logger::Level m_level;
    std::source_location m_src;

    void operator()(std::exception_ptr e) const;
};
