#include "Connection.h"

#include <asio/connect.hpp>
#include <asio/write.hpp>

#include <glog/logging.h>

#include <google/protobuf/util/delimited_message_util.h>



std::shared_ptr<asio::ip::tcp::socket> Connect(asio::io_service & io, const std::string & address) {
	asio::ip::tcp::resolver resolver(io);
	asio::ip::tcp::resolver::query q(address.c_str(),"artemis");
	asio::ip::tcp::resolver::iterator endpoint = resolver.resolve(q);
	auto res = std::make_shared<asio::ip::tcp::socket>(io);
	asio::connect(*res,endpoint);
}


Connection::Connection(asio::io_service & service, const std::string & address)
	: d_service(service)
	, d_strand(service)
	, d_sending(false) {

	auto prodConsum = BufferPool::Create();
	d_producer = std::move(prodConsum.first);
	d_consumer = std::move(prodConsum.second);

	try {
		d_socket = Connect(d_service,d_address);
	} catch ( const std::exception & e) {
		LOG(ERROR) << "Could not connect to '" << d_address << "': " << e.what();
		ScheduleReconnect();
	}

}

void Connection::PostMessage(const google::protobuf::MessageLite & message) {
	{
		std::lock_guard<std::mutex> lock(d_sendingMutex);
		if ( d_producer->Full() ) {
			LOG(WARNING) << "serialization: discarding data: FIFO full";
		}

		asio::streambuf & buf = d_producer->Tail();
		buf.consume(buf.max_size());
		std::ostream output(&buf);
		google::protobuf::util::SerializeDelimitedToOstream(message, &output);
		d_producer->Push();
	}

	d_strand.post([this]{
			if(d_consumer->Empty() || d_sending == true) {
				return;
			}
			if (!d_socket) {
				LOG(WARNING) << "serialization: discarding data: no active connection";
			}
			ScheduleSend();
		});

}

void Connection::ScheduleSend() {
	d_sending = true;
	asio::async_write(*d_socket,
	                  d_consumer->Head().data(),
	                  d_strand.wrap([this](const asio::error_code & ec,
	                                       std::size_t){
		                                d_consumer->Pop();
		                                if (ec == asio::error::connection_reset || ec == asio::error::bad_descriptor ) {
			                                if (!d_socket) {
				                                return;
			                                }
			                                LOG(ERROR) << "serialization: disconnected: " << ec;
			                                d_socket.reset();
			                                ScheduleReconnect();
		                                } else if ( ec ) {
			                                LOG(ERROR) << "serialization: could not send data: " << ec;
		                                }

		                                if (d_consumer->Empty()) {
			                                d_sending = false;
			                                return;
		                                }
		                                ScheduleSend();
	                                }));
}

void Connection::ScheduleReconnect() {
	if (d_socket) {
		return;
	}
	LOG(INFO) << "Reconnecting in 5s to '" << d_address << "'";
	auto t = std::make_shared<asio::deadline_timer>(d_service,boost::posix_time::seconds(5));
	t->async_wait(d_strand.wrap([this,t](const asio::error_code & ) {
				LOG(INFO) << "Reconnecting to '" << d_address << "'";
				try {
					d_socket = Connect(d_service,d_address);
				} catch ( const std::exception & e) {
					LOG(ERROR) << "Could not connect to '" << d_address << "':  " << e.what();
					ScheduleReconnect();
				}
			}));
}
