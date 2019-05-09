#pragma once

#include <asio/io_service.hpp>
#include <asio/strand.hpp>
#include <asio/streambuf.hpp>
#include <asio/ip/tcp.hpp>

#include <google/protobuf/message.h>

#include "RingBuffer.h"

#include <chrono>

#include <mutex>

class Connection {
public:
	typedef std::shared_ptr<Connection> Ptr;
	~Connection();
	static Ptr Create(asio::io_service & service,
	                  const std::string & host,
	                  uint16_t port,
	                  std::chrono::high_resolution_clock::duration reconnectTime = std::chrono::milliseconds(5000));
	static void PostMessage(const Ptr & connection, const google::protobuf::MessageLite & m);

private :
	Connection(asio::io_service & service,
	           const std::string & host,
	           uint16_t port,
	           std::chrono::high_resolution_clock::duration reconnectTime);

	static void ScheduleReconnect(const Ptr & self);
	static void ScheduleSend(const Ptr & self);
	static void Connect(const Ptr & self);

	asio::io_service & d_service;
	std::mutex         d_sendingMutex;

	std::string                            d_host;
	uint16_t                               d_port;
	std::shared_ptr<asio::ip::tcp::socket> d_socket;
	asio::strand                           d_strand;

	typedef RingBuffer<asio::streambuf,16> BufferPool;
	BufferPool::Consumer::Ptr d_consumer;
	BufferPool::Producer::Ptr d_producer;
	bool                      d_sending;

	std::chrono::high_resolution_clock::duration  d_reconnectTime;
};
