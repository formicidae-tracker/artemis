#pragma once

#include <asio/io_service.hpp>
#include <asio/strand.hpp>
#include <asio/streambuf.hpp>
#include <asio/ip/tcp.hpp>

#include <google/protobuf/message_lite.h>

#include "RingBuffer.h"

#include <mutex>

class Connection {
public:
	typedef std::shared_ptr<Connection> Ptr;
	Connection(asio::io_service & service, const std::string & address);

	void PostMessage(const google::protobuf::MessageLite & m);


private :
	void ScheduleReconnect();
	void ScheduleSend();

	asio::io_service & d_service;
	std::mutex         d_sendingMutex;

	std::string                            d_address;
	std::shared_ptr<asio::ip::tcp::socket> d_socket;
	asio::strand                           d_strand;

	typedef RingBuffer<asio::streambuf,16> BufferPool;
	BufferPool::Consumer::Ptr d_consumer;
	BufferPool::Producer::Ptr d_producer;
	bool                      d_sending;

};
