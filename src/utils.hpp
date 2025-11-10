#pragma once

#include "async_aliases.hpp"
#include <chrono>
#include <concepts>

asio::awaitable<void> timeout(const std::chrono::steady_clock::duration& ms);

template<std::invocable<> Func> struct AtScopeExit final
{
    explicit AtScopeExit(Func f) : m_exitCb{ f } {}

    ~AtScopeExit() { m_exitCb(); }

    Func m_exitCb;
};
