#include <csignal>
#include <filesystem>
#include <source_location>

#include "async_aliases.hpp"
#include "channel_stuff.hpp"
#include "http_stuff.hpp"
#include "log/logger.hpp"
#include "socket_stuff.hpp"
#include "timeout_stuff.hpp"

using namespace std::chrono_literals;

asio::awaitable<void> async_main(asio::io_context& ctx, ssl::context& sslCtx)
{
    try
    {
        LOG_INFO("running async main");

        asio::steady_timer tm{ ctx };
        asio::co_spawn(ctx, accept_client(sslCtx), asio::detached);
        asio::co_spawn(ctx, read_http("dummyjson.com", "/ip", sslCtx), asio::detached);
        asio::co_spawn(ctx, something_that_timesout(), asio::detached);
        asio::co_spawn(ctx, start_channel_work(), asio::detached);

        while (true)
        {
            tm.expires_after(100ms);
            co_await tm.async_wait();
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
    namespace fs = std::filesystem;
    constexpr auto main_cpp{ std::source_location::current() };
    const fs::path certsDir{ fs::path{ main_cpp.file_name() }.parent_path().parent_path() / "certs" };

    try
    {
        Sage::Logger::SetupLogger();

        asio::io_context ctx{ 1 };
        ssl::context sslCtx{ ssl::context::tlsv13 };
        sslCtx.set_default_verify_paths();
        sslCtx.use_certificate_file(certsDir / "example.com.crt", ssl::context::file_format::pem);
        sslCtx.use_private_key_file(certsDir / "example.com.key", ssl::context::file_format::pem);
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
    catch (const boost::system::system_error& e)
    {
        LOG_ERROR("failed with {}", e.what());
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("failed with {}", e.what());
        return 1;
    }
}
