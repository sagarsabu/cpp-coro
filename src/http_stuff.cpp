#include "http_stuff.hpp"
#include "log/logger.hpp"
#include "utils.hpp"
#include <openssl/tls1.h>

using namespace std::chrono_literals;
using namespace boost::asio::experimental::awaitable_operators;

asio::awaitable<void> read_http_once(const std::string& host, const std::string& target, ssl::context& sslCtx)
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

        auto resolved{ co_await resolver.async_resolve(host, "https") };
        auto ep{ *resolved.begin() };
        LOG_DEBUG("resolved host:{} target:{} to '{}:{}'", host, target, ep.host_name(), ep.service_name());

        if (auto res =
                co_await (beast::get_lowest_layer(stream).async_connect(ep, asio::use_awaitable) or timeout(10s));
            res.index() == 1)
        {
            LOG_ERROR("async_connect to {} timed out", host);
            co_return;
        }

        const auto& socket = stream.next_layer().socket();
        LOG_DEBUG(
            "connected to {}:{}", socket.remote_endpoint().address().to_string(), socket.remote_endpoint().port()
        );

        std::get<0>(co_await (stream.async_handshake(ssl::stream_base::client, asio::use_awaitable) or timeout(10s)));

        LOG_DEBUG("handshake completed {}", host);

        beast::http::request<beast::http::string_body> req{ beast::http::verb::get, target, 11 };
        req.set(beast::http::field::version, "2.0");
        req.set(beast::http::field::host, host);
        req.set(beast::http::field::user_agent, BOOST_BEAST_VERSION_STRING);

        auto nBytesWritten =
            std::get<0>(co_await (beast::http::async_write(stream, req, asio::use_awaitable) or timeout(10s)));
        LOG_INFO("wrote {} bytes to {}", nBytesWritten, host);

        beast::flat_buffer buff{};
        beast::http::response<beast::http::string_body> res;
        auto nBytesRead =
            std::get<0>(co_await (beast::http::async_read(stream, buff, res, asio::use_awaitable) or timeout(10s)));

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
        co_await stream.async_shutdown(asio::redirect_error(ec));
        if (ec.failed())
        {
            LOG_WARNING("read shutdown failed.e: {}", ec.what());
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
        timer.expires_after(10s);
        co_await timer.async_wait();
    }
}
