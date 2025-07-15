#pragma once

#include <boost/asio.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>

#include "log/log_levels.hpp"
#include "log/logger.hpp"

namespace asio = boost::asio;
namespace ssl = boost::asio::ssl;
namespace beast = boost::beast;

template<Sage::Logger::Level LEVEL> struct detached_log_exception_t
{
    void operator()(std::exception_ptr e)
    {
        if (e)
        {
            try
            {
                std::rethrow_exception(e);
            }
            catch (const std::exception& e)
            {
                Sage::Logger::Internal::LogToStream<LEVEL>(
                    std::source_location::current(), "exception caught e='{}'", e.what()
                );
            }
        }
    }
};

constexpr detached_log_exception_t<Sage::Logger::Debug> detached_log_debug_exception{};
constexpr detached_log_exception_t<Sage::Logger::Info> detached_log_info_exception{};
constexpr detached_log_exception_t<Sage::Logger::Warning> detached_log_warning_exception{};
constexpr detached_log_exception_t<Sage::Logger::Error> detached_log_error_exception{};
constexpr detached_log_exception_t<Sage::Logger::Critical> detached_log_critical_exception{};
