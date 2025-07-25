#include "timeout_stuff.hpp"
#include "async_aliases.hpp"
#include "log/logger.hpp"
#include "utils.hpp"
#include <memory>

using namespace std::chrono_literals;
using namespace asio::experimental::awaitable_operators;

asio::awaitable<void> something_nested(int idx)
{
    AtScopeExit dropGuard{ [idx] { LOG_INFO("nested cancellable task {} has been cancelled", idx); } };
    co_await timeout(15s);
}

asio::awaitable<void> something_that_timesout()
{
    auto exc{ co_await asio::this_coro::executor };
    int idx{ 0 };

    while (true)
    {
        LOG_INFO("starting tasks");
        idx++;

        auto cs{ std::make_shared<asio::cancellation_signal>() };
        asio::co_spawn(
            exc,
            [idx, cs] -> asio::awaitable<void>
            {
                AtScopeExit dropGuard{ [idx] { LOG_INFO("cancellable task {} has been cancelled", idx); } };

                while (true)
                {
                    LOG_INFO("cancellable task {} has not been cancelled", idx);
                    try
                    {
                        co_await timeoutWithExc(1s, [idx] -> asio::awaitable<void> { co_await something_nested(idx); });
                    }
                    catch (const boost::system::system_error&)
                    {
                    }

                    try
                    {
                        co_await timeoutWithExc(1s, something_nested, idx);
                    }
                    catch (const boost::system::system_error&)
                    {
                    }
                }
            },
            asio::bind_cancellation_slot(cs->slot(), detached_log_exception{ Sage::Logger::Level::Error })
        );

        co_await timeout(4s);

        cs->emit(asio::cancellation_type::terminal);
    }
}
