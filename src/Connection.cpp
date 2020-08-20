#include "Connection.hpp"

#include <asio/connect.hpp>
#include <asio/write.hpp>
#include <asio/deadline_timer.hpp>

#include <glog/logging.h>

#include <google/protobuf/util/delimited_message_util.h>
#include <fort-hermes/Header.pb.h>

namespace fort {
namespace artemis {

#define Connection_LOG(level,connection) LOG(level) << "[connection" \
	<< (connection)->d_host \
	<< ":" \
	<< (connection)->d_port \
	<< "]: "


void Connection::Connect(const Ptr & self) {
	std::ostringstream oss;
	oss << self->d_port;
	try {
		asio::ip::tcp::resolver resolver(self->d_service);
		asio::ip::tcp::resolver::query q(self->d_host.c_str(),oss.str().c_str());
		asio::ip::tcp::resolver::iterator endpoint = resolver.resolve(q);
		auto res = std::make_shared<asio::ip::tcp::socket>(self->d_service);
		asio::async_connect(*res,endpoint,self->d_strand.wrap([self,res](const asio::error_code & ec,
		                                                                 asio::ip::tcp::resolver::iterator it) {
					if (ec) {
						Connection_LOG(ERROR,self) << "Could not connect to host: " << ec;
						ScheduleReconnect(self);
						return;
					}
					self->d_socket = res;
				}));
	} catch (const std::exception & e) {
		Connection_LOG(ERROR,self) << "Could not connect to host: exception: " << e.what();
		self->d_strand.post([self](){ScheduleReconnect(self);});
	}

	// try {
	// 	fort::hermes::Header h;
	// 	h.set_type(fort::hermes::Header_Type_Network);
	// 	auto v = h.mutable_version();
	// 	v->set_vmajor(0);
	// 	v->set_vminor(2);
	// 	PostMessage(self,h);
	// } catch ( const std::exception & e) {
	// 	LOG(ERROR) << "Coould not send header to '" << self->d_host << ":" << self->d_port << "': " << e.what();
	// 	self->d_strand.post([self](){ScheduleReconnect(self);});
	// }

}

Connection::Ptr Connection::Create(asio::io_service & service,
                                   const std::string & host,
                                   uint16_t port,
                                   Duration reconnectPeriod ) {
	std::shared_ptr<Connection> res(new Connection(service,host,port,reconnectPeriod));
	Connect(res);
	return res;
}

Connection::Connection(asio::io_service & service,
                       const std::string & host,
                       uint16_t port,
                       Duration reconnectPeriod)
	: d_service(service)
	, d_strand(service)
	, d_host(host)
	, d_port(port)
	, d_sending(false)
	, d_reconnectPeriod(reconnectPeriod) {

	d_bufferQueue.set_capacity(16);

}

Connection::~Connection() {
}

// serializes the message to a std::string, which are allowed to
// contain NULL character, but should be considered a glorified
// std::vector<uint8_t> could be improved to use an allocation arena
// instead of buffers
inline std::string SerializeMessage(const google::protobuf::MessageLite & message ) {
	std::ostringstream oss;
	google::protobuf::util::SerializeDelimitedToOstream(message, &oss);
	return oss.str();

}

void Connection::PostMessage(const Ptr & self,const google::protobuf::MessageLite & message) {
	if ( self->d_bufferQueue.try_push(SerializeMessage(message)) == false ) {
		Connection_LOG(WARNING,self) << "discarding message as input queue is full";
	}

	self->d_strand.post([self] () {
		                    if(self->d_bufferQueue.size() <= 0  || self->d_sending == true) {
			                    return;
		                    }
		                    if (!self->d_socket) {
			                    Connection_LOG(WARNING,self) << "discarding message has there is no active connection";\
			                    std::string discard;
			                    self->d_bufferQueue.pop(discard);
			                    ScheduleReconnect(self);
			                    return;
		                    }
		                    ScheduleSend(self);
	                    });
}

void Connection::ScheduleSend(const Ptr & self) {
	self->d_sending = true;
	std::string toSend;
	self->d_bufferQueue.pop(toSend);
	asio::async_write(*self->d_socket,
	                  asio::const_buffers_1(&((toSend)[0]),toSend.size()),
	                  self->d_strand.wrap([self,toSend](const asio::error_code & ec,
	                                                    std::size_t s) {
		                                      if ( ec == asio::error::connection_reset || ec == asio::error::bad_descriptor ) {
			                                      Connection_LOG(ERROR,self) << "disconnected: " << ec;
			                                      if (!self->d_socket) {
				                                      return;
			                                      }
			                                      self->d_socket.reset();
			                                      ScheduleReconnect(self);
		                                      } else if ( ec ) {
			                                      Connection_LOG(ERROR,self) << "could not send data: " << ec;
		                                      }
		                                      if (self->d_bufferQueue.size() <= 0 ) {
			                                      self->d_sending = false;
			                                      return;
		                                      }
		                                      ScheduleSend(self);
	                                      }));
}

void Connection::ScheduleReconnect(const Ptr & self) {
	if (self->d_socket) {
		return;
	}

	Connection_LOG(INFO,self) << "reconnecting in " << self->d_reconnectPeriod;
	auto period = boost::posix_time::milliseconds(size_t(self->d_reconnectPeriod.Milliseconds()));
	auto t = std::make_shared<asio::deadline_timer>(self->d_service,
	                                                period);
	t->async_wait(self->d_strand.wrap([self,t](const asio::error_code & ) {
		                                  Connection_LOG(INFO,self) << "reconnecting now";
		                                  Connect(self);
	                                  }));
}

} // namespace artemis
} // namespace fort
