#include "timeout_stuff.hpp"
#include "async_aliases.hpp"
#include "log/logger.hpp"
#include "utils.hpp"

using namespace std::chrono_literals;
using namespace asio::experimental::awaitable_operators;



asio::awaitable<void> something_that_timesout()
{
    auto exc{ co_await asio::this_coro::executor };
    asio::steady_timer timer{ exc };

    while (true)
    {
        LOG_INFO("starting tasks");

        static int idx{ 0 };
        idx++;

        auto f = [idx = idx] -> asio::awaitable<void>
        {
            struct LogOnDrop
            {
                int m_id;

                ~LogOnDrop() { LOG_INFO("cancellable task {} has been cancelled", m_id); }
            };

            LogOnDrop _logOnDrop{ .m_id = idx };
            auto exc{ co_await asio::this_coro::executor };
            asio::steady_timer timer{ exc };
            while (true)
            {
                LOG_INFO("cancellable task {} has not been cancelled", idx);
                timer.expires_after(1s);
                co_await timer.async_wait();
            }
        };

        asio::co_spawn(exc, timeoutWithExc(f, 4s), detached_log_info_exception);

        timer.expires_after(4s);
        co_await timer.async_wait();
    }
}
