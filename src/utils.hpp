#pragma once

#include <chrono>

#include "async_aliases.hpp"

asio::awaitable<void> timeout(const std::chrono::milliseconds& timeout);
