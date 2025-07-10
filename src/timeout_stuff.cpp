#include "timeout_stuff.hpp"
#include "log/logger.hpp"

using namespace std::chrono_literals;

asio::awaitable<void> something_that_timesout()
{
    auto exc{ co_await asio::this_coro::executor };
    asio::steady_timer timer{ exc };
    asio::cancellation_signal cs;

    while (true)
    {
        LOG_INFO("starting tasks");

        static int idx{ 0 };
        idx++;

        auto f = [idx = idx] -> asio::awaitable<void>
        {
            // by default awaitables only allow terminal cancellation
            // we'll enable all types here:
            co_await asio::this_coro::reset_cancellation_state(asio::enable_total_cancellation());

            auto exc{ co_await asio::this_coro::executor };
            asio::steady_timer timer{ exc };
            while (true)
            {
                auto c_state{ co_await asio::this_coro::cancellation_state };
                if (c_state.cancelled() != asio::cancellation_type::none)
                {
                    break;
                }

                LOG_INFO("cancellable task {} has not been cancelled", idx);
                timer.expires_after(1s);
                co_await timer.async_wait();
            }

            LOG_INFO("cancellable task {} has been cancelled", idx);
        };

        asio::co_spawn(exc, asio::bind_cancellation_slot(cs.slot(), f), asio::detached);

        timer.expires_after(4s);
        co_await timer.async_wait();

        LOG_INFO("cancelling tasks");
        cs.emit(asio::cancellation_type::total);
    }
}
