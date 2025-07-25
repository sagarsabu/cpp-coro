#include "channel_stuff.hpp"
#include "async_aliases.hpp"
#include "log/logger.hpp"
#include <boost/asio/experimental/channel.hpp>
#include <memory>

using namespace std::chrono_literals;
using namespace asio::experimental::awaitable_operators;

struct AsyncEvent
{
    enum Type
    {
        A,
        B
    };

    Type type;
};

template<typename T> using Channel = asio::experimental::channel<void(boost::system::error_code, T)>;

using EventChannel = Channel<std::unique_ptr<AsyncEvent>>;

asio::awaitable<void> do_send(EventChannel& ch)
{
    asio::steady_timer tm{ co_await asio::this_coro::executor };
    int cntr{ 0 };
    while (true)
    {
        tm.expires_after(1s);
        co_await tm.async_wait();

        auto event{ std::make_unique<AsyncEvent>(cntr % 2 == 0 ? AsyncEvent::A : AsyncEvent::B) };
        cntr++;
        LOG_INFO("tx event {}", static_cast<int>(event->type));
        co_await ch.async_send(boost::system::error_code{}, std::move(event));
    }
}

asio::awaitable<void> do_recv(EventChannel& ch)
{
    while (true)
    {
        auto event{ co_await ch.async_receive() };
        LOG_INFO("rx event {}", static_cast<int>(event->type));
    }
}

asio::awaitable<void> start_channel_work()
{
    LOG_INFO("starting");
    EventChannel ch{ co_await asio::this_coro::executor };
    co_await(do_send(ch) and do_recv(ch));
}
