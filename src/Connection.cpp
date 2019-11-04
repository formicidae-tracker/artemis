#include "Connection.h"

#include <asio/connect.hpp>
#include <asio/write.hpp>
#include <asio/deadline_timer.hpp>

#include <glog/logging.h>

#include <google/protobuf/util/delimited_message_util.h>
#include <fort-hermes/Header.pb.h>


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
						LOG(ERROR) << "Could not connect to '" << self->d_host << ":" << self->d_port << "': " << ec;
						ScheduleReconnect(self);
						return;
					}
					self->d_socket = res;
				}));
	} catch (const std::exception & e) {
		LOG(ERROR) << "Could not connect to '" << self->d_host << ":" << self->d_port << "': " << e.what();
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
                                   std::chrono::high_resolution_clock::duration reconnectTime) {
	std::shared_ptr<Connection> res(new Connection(service,host,port,reconnectTime));
	Connect(res);
	return res;
}

Connection::Connection(asio::io_service & service,
                       const std::string & host,
                       uint16_t port,
                       std::chrono::high_resolution_clock::duration reconnectTime)
	: d_service(service)
	, d_strand(service)
	, d_host(host)
	, d_port(port)
	, d_sending(false)
	, d_reconnectTime(reconnectTime) {

	auto prodConsum = BufferPool::Create();
	d_producer = std::move(prodConsum.first);
	d_consumer = std::move(prodConsum.second);
}

Connection::~Connection() {
}

void Connection::PostMessage(const Ptr & self,const google::protobuf::MessageLite & message) {
	std::lock_guard<std::mutex> lock(self->d_sendingMutex);
	if ( self->d_producer->Full() ) {
		LOG(WARNING) << "serialization: discarding data: FIFO full";
		return;
	}

	std::ostringstream & oss = self->d_producer->Tail();
	oss.str("");
	oss.clear();
	google::protobuf::util::SerializeDelimitedToOstream(message, &oss);

	self->d_producer->Push();
	self->d_strand.post([self]{
			if(self->d_consumer->Empty() || self->d_sending == true) {
				return;
			}
			if (!self->d_socket) {
				LOG(WARNING) << "serialization: discarding data: no active connection";
				self->d_consumer->Pop();
				ScheduleReconnect(self);
				return;
			}
			ScheduleSend(self);
		});
}

void Connection::ScheduleSend(const Ptr & self) {
	self->d_sending = true;
	auto toSend = std::make_shared<std::string>(self->d_consumer->Head().str());
	asio::async_write(*self->d_socket,
	                  asio::const_buffers_1(&((*toSend)[0]),toSend->size()),
	                  self->d_strand.wrap([self,toSend](const asio::error_code & ec,
	                                             std::size_t){
		                                      self->d_consumer->Pop();
		                                      if (ec == asio::error::connection_reset || ec == asio::error::bad_descriptor ) {
			                                      LOG(ERROR) << "serialization: disconnected: " << ec;

			                                      if (!self->d_socket) {
				                                      return;
			                                      }
			                                      self->d_socket.reset();
			                                      ScheduleReconnect(self);
		                                      } else if ( ec ) {
			                                      LOG(ERROR) << "serialization: could not send data: " << ec;
		                                      }
		                                      if (self->d_consumer->Empty()) {
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
	auto d = std::chrono::duration_cast<std::chrono::milliseconds>(self->d_reconnectTime);
	LOG(INFO) << "Reconnecting in "
	          << d.count()
	          << "ms to '" << self->d_host << ":" << self->d_port << "'";
	auto t = std::make_shared<asio::deadline_timer>(self->d_service,boost::posix_time::milliseconds(d.count()));
	t->async_wait(self->d_strand.wrap([self,t](const asio::error_code & ) {
				LOG(INFO) << "Reconnecting to '" << self->d_host << ":" << self->d_port << "'";
				Connect(self);
			}));
}
