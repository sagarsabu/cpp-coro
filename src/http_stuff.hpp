#pragma once

#include "async_aliases.hpp"

asio::awaitable<void> read_http(std::string host, std::string target, ssl::context& sslCtx);
