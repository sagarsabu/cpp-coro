#include <array>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <exception>
#include <map>
#include <memory>
#include <openssl/tls1.h>
#include <sstream>
#include <string>
#include <utility>

#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>

#include "log/logger.hpp"

namespace asio = boost::asio;
namespace ssl = boost::asio::ssl;
namespace beast = boost::beast;

using namespace std::chrono_literals;
using namespace boost::asio::experimental::awaitable_operators;

using ClientsMap = std::map<std::string, ssl::stream<asio::ip::tcp::socket>>;
using SharedClientsMap = std::shared_ptr<ClientsMap>;

asio::awaitable<void> timeout(const std::chrono::milliseconds& timeout)
{
    auto exc{ co_await asio::this_coro::executor };
    asio::steady_timer timer{ exc, timeout };
    co_return co_await timer.async_wait(asio::use_awaitable);
}

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
            strData = strData.substr(0, strData.size() - 1);
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

asio::awaitable<void> read_http_once(std::string host, std::string target, ssl::context& sslCtx)
{
    try
    {
        auto ctx{ co_await asio::this_coro::executor };
        ssl::stream<beast::tcp_stream> stream{ ctx, sslCtx };
        asio::ip::tcp::resolver resolver{ ctx };

        // required for SNI verification
        if (not SSL_set_tlsext_host_name(stream.native_handle(), host.c_str()))
        {
            throw beast::system_error(static_cast<asio::error::ssl_errors>(::ERR_get_error()));
        }

        auto resolved{ co_await resolver.async_resolve(host, "https", asio::use_awaitable) };
        auto ep{ *resolved.begin() };
        LOG_INFO("resolved host:{} target:{} to '{}:{}'", host, target, ep.host_name(), ep.service_name());

        if (auto res = co_await(beast::get_lowest_layer(stream).async_connect(ep, asio::use_awaitable) or timeout(10s));
            res.index() == 1)
        {
            LOG_ERROR("async_connect to {} timed out", host);
            co_return;
        }

        const auto& socket = stream.next_layer().socket();
        LOG_INFO("connected to {}:{}", socket.remote_endpoint().address().to_string(), socket.remote_endpoint().port());

        std::get<0>(co_await(stream.async_handshake(ssl::stream_base::client, asio::use_awaitable) or timeout(10s)));

        LOG_INFO("handshake completed {}", host);

        beast::http::request<beast::http::string_body> req{ beast::http::verb::get, target, 11 };
        req.set(beast::http::field::version, "2.0");
        req.set(beast::http::field::host, host);
        req.set(beast::http::field::user_agent, BOOST_BEAST_VERSION_STRING);

        auto nBytesWritten =
            std::get<0>(co_await(beast::http::async_write(stream, req, asio::use_awaitable) or timeout(10s)));
        LOG_INFO("wrote {} bytes to {}", nBytesWritten, host);

        beast::flat_buffer buff{};
        beast::http::response<beast::http::string_body> res;
        auto nBytesRead =
            std::get<0>(co_await(beast::http::async_read(stream, buff, res, asio::use_awaitable) or timeout(10s)));

        auto status{ res.result() };
        std::ostringstream oss;
        oss << status;
        std::string statusStr{ oss.str() };
        if (status == beast::http::status::ok)
        {
            std::string body{ res.body() };
            LOG_INFO("read {} bytes from {} status: {} res: {}", nBytesRead, host, statusStr, body);
        }
        else
        {
            LOG_ERROR("read failed with status: {}", statusStr);
        }

        boost::system::error_code ec;
        co_await stream.async_shutdown(asio::redirect_error(asio::use_awaitable, ec));
        if (ec.failed())
        {
            LOG_ERROR("read shutdown failed.e: {}", ec.what());
        }
        else
        {
            LOG_INFO("stream to {} closed", host);
        }
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("read failed {}", e.what());
    }
}

asio::awaitable<void> read_http(std::string host, std::string target, ssl::context& sslCtx)
{
    auto exc{ co_await asio::this_coro::executor };
    asio::steady_timer timer{ exc };

    while (true)
    {
        co_await read_http_once(host, target, sslCtx);
        timer.expires_after(3s);
        co_await timer.async_wait(asio::use_awaitable);
    }
}

asio::awaitable<void> accept_client(ssl::context& sslCctx)
{
    auto exc{ co_await asio::this_coro::executor };
    asio::ip::tcp::resolver resolver{ exc };
    auto resolve_res =
        co_await resolver.async_resolve("localhost", "8080", asio::ip::resolver_base::v4_mapped, asio::use_awaitable);
    asio::ip::tcp::acceptor acc{ exc, resolve_res.begin()->endpoint() };
    const auto ep{ acc.local_endpoint() };

    SharedClientsMap clients{ std::make_shared<ClientsMap>() };

    while (true)
    {
        LOG_INFO("accepting {}:{}", ep.address().to_string(), ep.port());

        auto socket{ co_await acc.async_accept(asio::use_awaitable) };
        std::string tag{ socket.remote_endpoint().address().to_string() + ":" +
                         std::to_string(socket.remote_endpoint().port()) };

        LOG_INFO("accepted {}:{} -> {}", ep.address().to_string(), ep.port(), tag);

        ssl::stream<asio::ip::tcp::socket> stream{ std::move(socket), sslCctx };
        clients->emplace(tag, std::move(stream));
        asio::co_spawn(acc.get_executor(), handle_connection(tag, clients), asio::detached);
    }
}

asio::awaitable<void> async_main(asio::io_context& ctx, ssl::context& sslCtx)
{
    try
    {
        auto cs{ co_await asio::this_coro::cancellation_state };
        LOG_INFO("running async main");

        asio::steady_timer tm{ ctx };
        asio::co_spawn(ctx, accept_client(sslCtx), asio::detached);
        asio::co_spawn(ctx, read_http("dummyjson.com", "/ip", sslCtx), asio::detached);

        while (not cs.cancelled())
        {
            tm.expires_after(100ms);
            co_await tm.async_wait(asio::use_awaitable);
        }
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("async main e: {}", e.what());
    }
    catch (...)
    {
        LOG_ERROR("unexpected exception");
    }
    ctx.stop();
    co_return;
}

int main()
{
    try
    {
        Sage::Logger::SetupLogger();

        asio::io_context ctx{ 1 };
        ssl::context sslCtx{ ssl::context::tlsv13 };
        sslCtx.set_default_verify_paths();
        sslCtx.use_certificate_file(
            "/home/sagar/workspace/cpp-coro/certs/example.com.crt", ssl::context::file_format::pem
        );
        sslCtx.use_private_key_file(
            "/home/sagar/workspace/cpp-coro/certs/example.com.key", ssl::context::file_format::pem
        );
        sslCtx.set_verify_mode(ssl::verify_peer);

        // hook signals
        asio::signal_set signals{ ctx };
        for (auto sig : { SIGINT, SIGTERM, SIGQUIT, SIGHUP })
        {
            signals.add(sig);
        }
        signals.async_wait(
            [&](auto, auto sig)
            {
                LOG_INFO("caught signal {}. stopping", strsignal(sig));
                ctx.stop();
            }
        );

        asio::co_spawn(ctx, async_main(ctx, sslCtx), asio::detached);

        while (not ctx.stopped())
        {
            size_t events{ ctx.run_for(100ms) };
            if (events)
            {
                LOG_DEBUG("handled {} events", events);
            }
        }
        return 0;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("failed with {}", e.what());
        return 1;
    }
}
