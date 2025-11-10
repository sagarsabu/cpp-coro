#include "timeout_stuff.hpp"
#include "async_aliases.hpp"
#include "log/logger.hpp"
#include "utils.hpp"
#include <memory>

using namespace std::chrono_literals;
using namespace asio::experimental::awaitable_operators;

asio::awaitable<void> something_nested(int idx, int itr)
{
    AtScopeExit dropGuard{ [idx, itr]
                           { LOG_INFO("nested cancellable task {} has been cancelled, itr={}", idx, itr); } };
    co_await timeout(15s);
}

asio::awaitable<void> something_that_timesout()
{
    auto exc{ co_await asio::this_coro::executor };
    auto strand = asio::make_strand(exc);

    int idx{ 0 };

    while (true)
    {
        LOG_INFO("starting tasks {}", idx);
        idx++;

        auto cs{ std::make_shared<asio::cancellation_signal>() };
        asio::co_spawn(
            strand,
            [idx, cs] -> asio::awaitable<void>
            {
                AtScopeExit dropGuard{ [idx] { LOG_INFO("cancellable task {} has been cancelled", idx); } };

                int itr{ 0 };
                while (true)
                {
                    LOG_INFO("cancellable task {} has not been cancelled. itr={}", idx, itr);
                    [[maybe_unused]] auto r{ co_await (something_nested(idx, itr) or timeout(1s)) };
                    itr++;
                }
            },
            asio::bind_cancellation_slot(cs->slot(), detached_log_exception{ Sage::Logger::Level::Error })
        );

        // co_await timeout_v2(4s, strand);
        co_await timeout(4s);

        cs->emit(asio::cancellation_type::terminal);
    }
}
