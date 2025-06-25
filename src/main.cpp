#include <array>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstring>
#include <exception>
#include <future>
#include <print>
#include <string>

#include <boost/asio.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

#include "log/logger.hpp"

using namespace std::chrono_literals;
namespace asio = boost::asio;
using namespace boost::asio::experimental::awaitable_operators;

std::atomic<bool> g_exit{ false };

bool exitRequested() noexcept { return g_exit.load(std::memory_order::acquire); }

void requestStop() noexcept { g_exit.store(true, std::memory_order_release); }

asio::awaitable<void> timeout(const std::chrono::milliseconds& timeout)
{
    auto exc{ co_await asio::this_coro::executor };
    asio::steady_timer timer{ exc, timeout };
    co_return co_await timer.async_wait(asio::use_awaitable);
}

asio::awaitable<void> waitUnitExitRequested()
{
    static constexpr std::chrono::milliseconds delta{ 100ms };
    auto exc{ co_await asio::this_coro::executor };
    asio::steady_timer timer{ exc };
    while (not exitRequested())
    {
        timer.expires_after(delta);
        co_await timer.async_wait(asio::use_awaitable);
    }
}

asio::awaitable<void> handle_connection(asio::ip::tcp::socket socket)
{
    std::string tag{ socket.remote_endpoint().address().to_string() + ":" +
                     std::to_string(socket.remote_endpoint().port()) };

    std::array<char, 1024> data;
    while (true)
    {
        auto res = co_await(socket.async_receive(asio::buffer(data), asio::use_awaitable) or timeout(10s));
        if (res.index() == 1)
        {
            LOG_INFO("timed out for {}", tag);
            co_return;
        }

        auto nBytes{ std::get<0>(res) };
        if (nBytes == 0)
        {
            LOG_INFO("connection to {} most likely closed", tag);
            co_return;
        }

        std::string_view strData{ data.begin(), data.begin() + nBytes };
        if (strData.ends_with('\n'))
        {
            strData = strData.substr(0, strData.size() - 1);
        }
        LOG_INFO("client {}: n-bytes: {} says: {}", tag, nBytes, strData);
    }
}

asio::awaitable<void> handle_accept(asio::ip::tcp::acceptor& acc)
{
    const auto ep{ acc.local_endpoint() };
    LOG_INFO("accepting {}:{}", ep.address().to_string(), ep.port());

    auto socket{ co_await acc.async_accept(asio::use_awaitable) };
    LOG_INFO(
        "accepted {}:{} -> {}:{}",
        ep.address().to_string(),
        ep.port(),
        socket.remote_endpoint().address().to_string(),
        socket.remote_endpoint().port()
    );
    asio::co_spawn(acc.get_executor(), handle_connection(std::move(socket)), asio::detached);
}

asio::awaitable<void> async_main(asio::io_context& ctx, std::promise<int> res)
{
    try
    {
        auto exc{ co_await asio::this_coro::executor };

        std::println("running async main");

        asio::ip::tcp::endpoint ep{ asio::ip::make_address("127.0.0.1"), 8080 };
        asio::ip::tcp::resolver resolver{ ctx };
        auto resolve_res = co_await resolver.async_resolve(ep);
        asio::ip::tcp::acceptor acc{ ctx, resolve_res.begin()->endpoint() };

        while (not exitRequested())
        {
            co_await(handle_accept(acc) || waitUnitExitRequested());
        }

        res.set_value(0);
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("async main e {}", e.what());
        res.set_exception(std::current_exception());
    }
    catch (...)
    {
        res.set_exception(std::current_exception());
    }
    ctx.stop();
    co_return;
}

int main()
{
    try
    {
        Sage::Logger::SetupLogger();

        asio::io_context ctx;
        // asio::cancellation_signal cs;

        std::promise<int> main_promise;
        auto future{ main_promise.get_future() };

        // hook signals
        asio::signal_set signals{ ctx };
        for (auto sig : { SIGINT, SIGTERM, SIGQUIT, SIGHUP })
        {
            signals.add(sig);
        }
        signals.async_wait(
            [](auto, auto sig)
            {
                LOG_INFO("caught signal {}. stopping", strsignal(sig));
                requestStop();
            }
        );

        asio::co_spawn(ctx, async_main(ctx, std::move(main_promise)), asio::detached);
        ctx.run();
        int res{ future.get() };
        return res;
    }
    catch (const std::exception& e)
    {
        LOG_CRITICAL("failed with {}", e.what());
        return 1;
    }
}
