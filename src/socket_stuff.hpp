#pragma once

#include "async_aliases.hpp"

asio::awaitable<void> accept_client(ssl::context& sslCctx);
