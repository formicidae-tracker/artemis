#pragma once

#include <asio/io_service.hpp>
#include <asio/strand.hpp>
#include <asio/streambuf.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/version.hpp>

#include <google/protobuf/message.h>

#include <tbb/concurrent_queue.h>

#include "Time.hpp"

#include <mutex>

namespace fort {
namespace artemis {

class Connection {
public:
	typedef std::shared_ptr<Connection> Ptr;
	~Connection();
	static Ptr Create(asio::io_service & service,
	                  const std::string & host,
	                  uint16_t port,
	                  Duration reconnectPeriod = 5 * Duration::Second );

	// thread-safe function
	static void PostMessage(const Ptr & connection, const google::protobuf::MessageLite & m);

private :
	Connection(asio::io_service & service,
	           const std::string & host,
	           uint16_t port,
	           Duration reconnectPeriod);

	static void ScheduleReconnect(const Ptr & self);
	static void ScheduleSend(const Ptr & self);
	static void Connect(const Ptr & self);

	asio::io_service & d_service;

	std::string                            d_host;
	uint16_t                               d_port;
	std::shared_ptr<asio::ip::tcp::socket> d_socket;

#if ASIO_VERSION >= 101200
	asio::io_context::strand               d_strand;
#else
	asio::strand                           d_strand;
#endif

	typedef tbb::concurrent_bounded_queue<std::string> BufferQueue;

	bool      d_sending;

	BufferQueue d_bufferQueue;

	Duration  d_reconnectPeriod;
};

} // namespace artemis
} // namespace fort
