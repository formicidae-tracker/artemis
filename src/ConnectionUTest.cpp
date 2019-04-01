#include "ConnectionUTest.h"


#include <asio/error.hpp>
#include <asio/connect.hpp>
#include <asio/streambuf.hpp>
#include <asio/read.hpp>
#include <asio/write.hpp>
#include <google/protobuf/util/delimited_message_util.h>
#include "hermes/FrameReadout.pb.h"

#include "Connection.h"

#include <glog/logging.h>

class IncommingConnection {
public :
	IncommingConnection(asio::io_service & service, Barrier & closed) : d_closed(closed), d_socket(service), d_continue(true), d_received(0) {
	}
	~IncommingConnection() {
	}
	Barrier &              d_closed;
	asio::ip::tcp::socket  d_socket;
	uint8_t                d_data[8];
	bool                   d_continue;
	size_t                 d_received;
	static void Read(const std::shared_ptr<IncommingConnection> & self) {
		asio::async_read(self->d_socket,asio::mutable_buffers_1(self->d_data,8),
		                 [self](const asio::error_code & ec,std::size_t) {
			                 if (ec == asio::error::eof) {
				                 self->d_closed.SignalOne();
				                 return;
			                 }
			                 google::protobuf::io::CodedInputStream ciss(static_cast<const uint8_t*>(&(self->d_data[0])),8);
			                 fort::FrameReadout m;
			                 //bug ??
			                 bool parsed = google::protobuf::util::ParseDelimitedFromCodedStream(&m,&ciss,NULL);
			                 parsed = google::protobuf::util::ParseDelimitedFromCodedStream(&m,&ciss,NULL);
			                 if (!parsed) {
				                 ADD_FAILURE() << "Could not parse message";
			                 }
			                 ++self->d_received;
			                 EXPECT_EQ(m.frameid(),self->d_received);
			                 EXPECT_EQ(m.timestamp(),self->d_received*20000);
			                 Read(self);
		                 });
	}
};

ConnectionUTest::ConnectionUTest()
	: d_acceptor(d_service,asio::ip::tcp::endpoint(asio::ip::tcp::v4(),12345))
	, d_rejector(d_service,asio::ip::tcp::endpoint(asio::ip::tcp::v4(),12346)){
	d_rejectLoop = [this] {
		auto connection = std::make_shared<IncommingConnection>(d_rejector.get_io_service(),d_closed);
		d_rejector.async_accept(connection->d_socket,[this,connection](const asio::error_code & ec) {
				if (!d_rejected) {
					d_rejected = true;
					connection->d_socket.close();
					d_accept.SignalOne();
				} else {
					IncommingConnection::Read(connection);
					d_accept.SignalOne();
				}
				d_rejectLoop();
			});
	};



	d_acceptLoop = [this]{
		auto connection = std::make_shared<IncommingConnection>(d_acceptor.get_io_service(),d_closed);
		d_acceptor.async_accept(connection->d_socket,[this,connection](const asio::error_code & ec) {
				d_acceptLoop();
				EXPECT_EQ(!ec,true);
				d_accept.SignalOne();
				IncommingConnection::Read(connection);
			});
	};


}

void ConnectionUTest::SetUp() {
	d_service.reset();
	d_rejected = false;
	d_rejectLoop();
	d_acceptLoop();

	d_service.post([this](){ d_running.SignalOne(); });

	d_thread = std::thread([this](){
			d_service.run();
		});
}

void ConnectionUTest::TearDown() {
	d_service.stop();
	d_thread.join();
}


TEST_F(ConnectionUTest,DoesNotMangle) {
	d_running.Wait();
	Connection::Ptr connection;

	try {
		connection = Connection::Create(d_service, "localhost", 12345);
	} catch (const std::exception & e) {
		FAIL() << "Could not connect: " << e.what();
	}
	d_accept.Wait();
	std::vector<fort::FrameReadout> messages;
	for (size_t i = 0; i < 4; ++i) {
		fort::FrameReadout m;
		m.set_frameid(i+1);
		m.set_timestamp(20000*(i+1));
		messages.push_back(m);
	}
	for(auto &m : messages) {
		Connection::PostMessage(connection,m);

	}


	connection.reset();
	d_closed.Wait();

}

TEST_F(ConnectionUTest,CanReconnect) {
	d_running.Wait();
	auto connection = Connection::Create(d_service,"localhost",12346,std::chrono::milliseconds(5));

	fort::FrameReadout m;
	m.set_frameid(1);
	m.set_timestamp(20000);


	//this will make the data be lost.
	Connection::PostMessage(connection,m);
	d_accept.Wait();

	d_accept.Wait();
	// the data was discarded by the connection object, safe to send ID 1 again
	Connection::PostMessage(connection,m);

	connection.reset();
	d_closed.Wait();


}
