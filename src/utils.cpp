#include "utils.hpp"

asio::awaitable<void> timeout(const std::chrono::milliseconds& timeout)
{
    auto exc{ co_await asio::this_coro::executor };
    asio::steady_timer timer{ exc };
    timer.expires_after(timeout);
    co_await timer.async_wait();
}
