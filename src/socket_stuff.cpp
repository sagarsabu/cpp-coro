#include <map>
#include <memory>

#include "log/logger.hpp"
#include "socket_stuff.hpp"
#include "utils.hpp"

using namespace std::chrono_literals;
using namespace boost::asio::experimental::awaitable_operators;

using ClientsMap = std::map<std::string, ssl::stream<asio::ip::tcp::socket>>;
using SharedClientsMap = std::shared_ptr<ClientsMap>;

asio::awaitable<void> handle_connection(std::string tag, SharedClientsMap clients)
{
    struct ClientDropper
    {
        const std::string& m_tag;
        ClientsMap& m_clients;

        ~ClientDropper() { m_clients.erase(m_tag); }
    } dropper{ .m_tag = tag, .m_clients = *clients };

    auto& socket{ clients->at(tag) };
    auto shake_res =
        co_await(socket.async_handshake(ssl::stream_base::handshake_type::server, asio::use_awaitable) or timeout(10s));
    if (shake_res.index() == 1)
    {
        LOG_INFO("handshake timed out for {}", tag);
        co_await(socket.async_shutdown(asio::use_awaitable) or timeout(100ms));
        co_return;
    }

    std::array<char, 1024> data{};
    while (true)
    {
        data.fill(0);
        auto res = co_await(socket.async_read_some(asio::buffer(data), asio::use_awaitable) or timeout(1min));
        if (res.index() == 1)
        {
            LOG_INFO("timed out for {}", tag);
            break;
        }

        size_t nBytes{ std::get<0>(res) };
        if (nBytes == 0)
        {
            LOG_INFO("connection to {} most likely closed", tag);
            break;
        }

        std::string strData{ data.begin(), nBytes };
        if (strData.ends_with('\n'))
        {
            strData.resize(strData.size() - 1);
        }

        LOG_INFO("client {}: n-bytes: {} says: '{}'. sending it all other clients", tag, nBytes, strData);

        for (auto& [clientTag, client] : *clients)
        {
            if (tag != clientTag)
            {
                asio::co_spawn(
                    client.get_executor(),
                    client.async_write_some(asio::buffer(data, nBytes), asio::use_awaitable) or timeout(1s),
                    asio::detached
                );
            }
        }
    }

    co_await(socket.async_shutdown(asio::use_awaitable) or timeout(100ms));
}

asio::awaitable<void> accept_client(ssl::context& sslCctx)
{
    auto exc{ co_await asio::this_coro::executor };
    asio::ip::tcp::resolver resolver{ exc };
    auto resolve_res = co_await resolver.async_resolve("localhost", "8080", asio::ip::resolver_base::v4_mapped);
    asio::ip::tcp::acceptor acc{ exc, resolve_res.begin()->endpoint() };
    const auto ep{ acc.local_endpoint() };

    SharedClientsMap clients{ std::make_shared<ClientsMap>() };

    while (true)
    {
        LOG_INFO("accepting {}:{}", ep.address().to_string(), ep.port());

        auto socket{ co_await acc.async_accept() };
        std::string tag{ socket.remote_endpoint().address().to_string() + ":" +
                         std::to_string(socket.remote_endpoint().port()) };

        LOG_INFO("accepted {}:{} -> {}", ep.address().to_string(), ep.port(), tag);

        ssl::stream<asio::ip::tcp::socket> stream{ std::move(socket), sslCctx };
        clients->emplace(tag, std::move(stream));
        asio::co_spawn(acc.get_executor(), handle_connection(tag, clients), asio::detached);
    }
}
