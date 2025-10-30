#include "Connection.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/write.hpp>

#include <fort/hermes/Header.pb.h>
#include <google/protobuf/util/delimited_message_util.h>
#include <slog++/slog++.hpp>

namespace fort {
namespace artemis {

#define Connection_LOG(level, connection)                                      \
	LOG(level) << "[connection" << (connection)->d_host << ":"                 \
	           << (connection)->d_port << "]: "

void Connection::Connect(const Ptr &self) {
	using namespace boost::asio;
	std::ostringstream oss;
	oss << self->d_port;
	try {
		ip::tcp::resolver           resolver(self->d_context);
		ip::tcp::resolver::query    q(self->d_host.c_str(), oss.str().c_str());
		ip::tcp::resolver::iterator endpoint = resolver.resolve(q);
		auto res = std::make_shared<ip::tcp::socket>(self->d_context);
		async_connect(
		    *res,
		    endpoint,
		    self->d_strand.wrap([self, res](
		                            const boost::system::error_code &ec,
		                            ip::tcp::resolver::iterator      it
		                        ) {
			    if (ec) {
				    self->d_logger.Error(
				        "Could not connect to host",
				        slog::Int("error_code", ec.value())
				    );
				    ScheduleReconnect(self);
				    return;
			    }
			    self->d_socket = res;
		    })
		);
	} catch (const std::exception &e) {
		self->d_logger.Error("could not connect to host", slog::Err(e));
		self->d_strand.post([self]() { ScheduleReconnect(self); });
	}

	// try {
	// 	fort::hermes::Header h;
	// 	h.set_type(fort::hermes::Header_Type_Network);
	// 	auto v = h.mutable_version();
	// 	v->set_vmajor(0);
	// 	v->set_vminor(2);
	// 	PostMessage(self,h);
	// } catch ( const std::exception & e) {
	// 	LOG(ERROR) << "Coould not send header to '" << self->d_host << ":" <<
	// self->d_port << "': " << e.what();
	// 	self->d_strand.post([self](){ScheduleReconnect(self);});
	// }
}

Connection::Ptr Connection::Create(
    boost::asio::io_context &context,
    const std::string       &host,
    uint16_t                 port,
    Duration                 reconnectPeriod
) {
	std::shared_ptr<Connection> res(
	    new Connection(context, host, port, reconnectPeriod)
	);
	Connect(res);
	return res;
}

Connection::Connection(
    boost::asio::io_context &context,
    const std::string       &host,
    uint16_t                 port,
    Duration                 reconnectPeriod
)
    : d_context{context}
    , d_host{host}
    , d_port{port}
    , d_strand{context}
    , d_sending{false}
    , d_bufferQueue{16}
    , d_reconnectPeriod{reconnectPeriod}
    , d_logger{slog::With(slog::Group(
          "connection", slog::String("host", host), slog::Int("port", port)
      ))} {
	if (host.empty()) {
		throw std::invalid_argument(
		    "Connection: destination host cannot be empty"
		);
	}
}

Connection::~Connection() {}

// serializes the message to a std::string, which are allowed to
// contain NULL character, but should be considered a glorified
// std::vector<uint8_t> could be improved to use an allocation arena
// instead of buffers
inline std::string SerializeMessage(const google::protobuf::MessageLite &message
) {
	std::ostringstream oss;
	google::protobuf::util::SerializeDelimitedToOstream(message, &oss);
	return oss.str();
}

void Connection::PostMessage(
    const Ptr &self, const google::protobuf::MessageLite &message
) {
	if (self->d_bufferQueue.try_enqueue(SerializeMessage(message)) == false) {
		self->d_logger.Warn("discarding message as input queue is full");
	}

	self->d_strand.post([self]() {
		if (self->d_bufferQueue.size_approx() == 0 || self->d_sending == true) {
			return;
		}
		if (!self->d_socket) {
			self->d_logger.Warn(
			    "discarding message has there is no active connection"
			);
			std::string discard;
			self->d_bufferQueue.wait_dequeue(discard);
			ScheduleReconnect(self);
			return;
		}
		ScheduleSend(self);
	});
}

void Connection::ScheduleSend(const Ptr &self) {
	self->d_sending = true;
	std::string toSend;
	self->d_bufferQueue.wait_dequeue(toSend);
	boost::asio::async_write(
	    *self->d_socket,
	    boost::asio::const_buffers_1(&((toSend)[0]), toSend.size()),
	    self->d_strand.wrap(
	        [self, toSend](const boost::system::error_code &ec, std::size_t s) {
		        if (ec == boost::asio::error::connection_reset ||
		            ec == boost::asio::error::bad_descriptor) {
			        self->d_logger.Error(
			            "disconnected",
			            slog::Int("error_code", ec.value())
			        );
			        if (!self->d_socket) {
				        return;
			        }
			        self->d_socket.reset();
			        ScheduleReconnect(self);
		        } else if (ec) {
			        self->d_logger.Error(
			            "could not send data",
			            slog::Int("error_code", ec.value())
			        );
		        }
		        if (self->d_bufferQueue.size_approx() <= 0) {
			        self->d_sending = false;
			        return;
		        }
		        ScheduleSend(self);
	        }
	    )
	);
}

std::string FormatDuration(const Duration &d) {
	std::ostringstream oss;
	oss << d;
	return oss.str();
}

void Connection::ScheduleReconnect(const Ptr &self) {
	if (self->d_socket) {
		return;
	}

	self->d_logger.Info(
	    "scheduling reconnection",
	    slog::String("period", FormatDuration(self->d_reconnectPeriod))
	);

	auto period = boost::posix_time::milliseconds(
	    size_t(self->d_reconnectPeriod.Milliseconds())
	);
	auto t =
	    std::make_shared<boost::asio::deadline_timer>(self->d_context, period);
	t->async_wait(self->d_strand.wrap([self,
	                                   t](const boost::system::error_code &) {
		self->d_logger.Info("reconnecting");
		Connect(self);
	}));
}

} // namespace artemis
} // namespace fort
