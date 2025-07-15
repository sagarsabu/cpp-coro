#pragma once

#include <chrono>
#include <stdexcept>
#include <type_traits>

#include "async_aliases.hpp"

asio::awaitable<void> timeout(const std::chrono::milliseconds& ms);

template<typename AsyncFunc>
auto timeoutWithExc(AsyncFunc&& func, const std::chrono::milliseconds& ms) -> decltype(func())
{
    using namespace asio::experimental::awaitable_operators;

    auto res{ co_await(func() or timeout(ms)) };
    if (res.index() == 1)
    {
        throw std::runtime_error{ "aborted" };
    }

    if constexpr (std::is_same_v<decltype(func()), asio::awaitable<void>>)
    {
        co_return;
    }
    else
    {
        auto val{ std::get<0>(res) };
        co_return val;
    }
}
