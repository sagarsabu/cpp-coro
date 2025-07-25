#pragma once

#include <chrono>
#include <concepts>
#include <type_traits>
#include <utility>

#include "async_aliases.hpp"

asio::awaitable<void> timeout(const std::chrono::steady_clock::duration& ms);

template<
    typename AsyncFunc, typename... Args,
    typename ReturnT = decltype(std::declval<AsyncFunc>()(std::forward<Args...>(std::declval<Args...>())))>
auto timeoutWithExc(const std::chrono::steady_clock::duration& dur, AsyncFunc&& func, Args... args) -> ReturnT
{
    using namespace asio::experimental::awaitable_operators;

    auto res{ co_await(func(std::forward<Args...>(args...)) or timeout(dur)) };
    if (res.index() == 1)
    {
        throw boost::system::system_error{ asio::error::timed_out };
    }

    if constexpr (std::is_same_v<ReturnT, asio::awaitable<void>>)
    {
        co_return;
    }
    else
    {
        if constexpr (std::is_move_constructible_v<ReturnT>)
        {
            co_return std::move(std::get<0>(res));
        }
        else
        {
            // assume move copyable
            co_return std::get<0>(res);
        }
    }
}

template<typename AsyncFunc, typename ReturnT = decltype(std::declval<AsyncFunc>()())>
auto timeoutWithExc(const std::chrono::steady_clock::duration& dur, AsyncFunc&& func) -> ReturnT
{
    using namespace asio::experimental::awaitable_operators;

    auto res{ co_await(func() or timeout(dur)) };
    if (res.index() == 1)
    {
        throw boost::system::system_error{ asio::error::timed_out };
    }

    if constexpr (std::is_same_v<ReturnT, asio::awaitable<void>>)
    {
        co_return;
    }
    else
    {
        if constexpr (std::is_move_constructible_v<ReturnT>)
        {
            co_return std::move(std::get<0>(res));
        }
        else
        {
            // assume move copyable
            co_return std::get<0>(res);
        }
    }
}

template<std::invocable<> Func> struct AtScopeExit final
{
    explicit AtScopeExit(Func f) : m_exitCb{ f } {}

    ~AtScopeExit() { m_exitCb(); }

    Func m_exitCb;
};
