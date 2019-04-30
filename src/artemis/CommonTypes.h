#pragma once

#include "RingBuffer.h"

#include <asio/ip/tcp.hpp>
#include <memory>




typedef std::shared_ptr<asio::ip::tcp::socket> SocketPtr;
