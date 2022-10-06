#pragma once

#include <boost/asio/io_service.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/version.hpp>

#include <google/protobuf/message.h>

#include <tbb/concurrent_queue.h>

#include <fort/time/Time.hpp>

#include <mutex>

#if(BOOST_ASIO_VERSION != 101800 )
#error "Wrong version of boost asio " # BOOST_ASIO_VERSION
#endif

namespace fort {
namespace artemis {

class Connection {
public:
	typedef std::shared_ptr<Connection> Ptr;
	~Connection();
	static Ptr Create(boost::asio::io_context & context,
	                  const std::string & host,
	                  uint16_t port,
	                  Duration reconnectPeriod = 5 * Duration::Second );

	// thread-safe function
	static void PostMessage(const Ptr & connection, const google::protobuf::MessageLite & m);

private :
	Connection(boost::asio::io_context & context,
	           const std::string & host,
	           uint16_t port,
	           Duration reconnectPeriod);

	static void ScheduleReconnect(const Ptr & self);
	static void ScheduleSend(const Ptr & self);
	static void Connect(const Ptr & self);

	boost::asio::io_context                     & d_context;

	std::string                                   d_host;
	uint16_t                                      d_port;
	std::shared_ptr<boost::asio::ip::tcp::socket> d_socket;

	boost::asio::io_context::strand               d_strand;

	typedef tbb::concurrent_bounded_queue<std::string> BufferQueue;

	bool      d_sending;

	BufferQueue d_bufferQueue;

	Duration  d_reconnectPeriod;
};

} // namespace artemis
} // namespace fort
