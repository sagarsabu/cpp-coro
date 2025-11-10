#include "async_aliases.hpp"
#include "channel_stuff.hpp"
#include "http_stuff.hpp"
#include "log/logger.hpp"
#include "socket_stuff.hpp"
#include "timeout_stuff.hpp"
#include <csignal>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <format>
#include <latch>
#include <pthread.h>
#include <source_location>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

asio::awaitable<void> async_main(ssl::context& sslCtx)
{
    auto ctx{ co_await asio::this_coro::executor };
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

        size_t nWorkers{ std::thread::hardware_concurrency() };
        std::latch startLatch{ static_cast<ptrdiff_t>(nWorkers) + 1 };

        asio::io_context ctx{ static_cast<int>(nWorkers) };

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

        asio::co_spawn(ctx, async_main(sslCtx), asio::detached);

        sigset_t signalsToBlock{};
        sigfillset(&signalsToBlock);

        std::vector<std::jthread> workers(nWorkers);
        for (size_t idx{ 1 }; auto& worker : workers)
        {
            worker = std::jthread(
                [&startLatch, &ctx]([[maybe_unused]] std::stop_token token)
                {
                    startLatch.arrive_and_wait();
                    auto guard{ asio::make_work_guard(ctx) };

                    try
                    {
                        LOG_INFO("starting");
                        ctx.run();
                        LOG_INFO("stopping");
                    }
                    catch (const std::exception& e)
                    {
                        LOG_CRITICAL("exception raise. e='{}'", e.what());
                        ctx.stop();
                    }
                }
            );

            auto handle{ worker.native_handle() };
            std::string name{ std::string{ "worker" } + '-' + std::to_string(idx) };
            pthread_setname_np(handle, name.c_str());

            if (int err{ pthread_sigmask(SIG_BLOCK, &signalsToBlock, nullptr) }; err != 0)
            {
                LOG_CRITICAL("failed to block signals. {}", strerror(err));
                std::exit(1);
            };

            idx++;
        }

        startLatch.arrive_and_wait();
        while (not ctx.stopped())
        {
            size_t events{ ctx.run_for(100ms) };
            if (events)
            {
                LOG_DEBUG("handled {} events", events);
            }
        }

        for (auto& worker : workers)
        {
            if (worker.joinable())
            {
                worker.request_stop();
                worker.join();
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
