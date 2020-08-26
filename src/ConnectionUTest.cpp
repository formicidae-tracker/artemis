#include "ConnectionUTest.hpp"


#include <boost/asio/error.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <google/protobuf/util/delimited_message_util.h>
#include <fort/hermes/FrameReadout.pb.h>
#include <fort/hermes/Header.pb.h>

#include "Connection.hpp"

#include <glog/logging.h>

namespace fort {
namespace artemis {

class IncommingConnection {
public :
	IncommingConnection(boost::asio::io_context & context, Barrier & closed) : d_closed(closed), d_socket(context), d_continue(true), d_received(0) , d_headerRead(true) {
	}
	~IncommingConnection() {
	}
	Barrier &                     d_closed;
	boost::asio::ip::tcp::socket  d_socket;
	uint8_t                       d_data[8];
	bool                          d_continue;
	bool                          d_headerRead;
	size_t                        d_received;
	static void Read(const std::shared_ptr<IncommingConnection> & self) {
		boost::asio::async_read(self->d_socket,boost::asio::mutable_buffers_1(self->d_data,7),
		                        [self](const boost::system::error_code & ec,std::size_t) {
			                        if (ec == boost::asio::error::eof) {
				                        self->d_closed.SignalOne();
				                        return;
			                        }
			                        google::protobuf::io::CodedInputStream ciss(static_cast<const uint8_t*>(&(self->d_data[0])),8);
			                        if ( !self->d_headerRead) {
				                        fort::hermes::Header h;
				                        bool parsed = google::protobuf::util::ParseDelimitedFromCodedStream(&h,&ciss,NULL);
				                        if (!parsed) {
					                        ADD_FAILURE() << "Could not read header";
				                        }
				                        self->d_headerRead = true;
			                        }
			                        fort::hermes::FrameReadout m;
			                        //bug ??
			                        bool parsed = google::protobuf::util::ParseDelimitedFromCodedStream(&m,&ciss,NULL);
			                        if (!parsed) {
				                        ADD_FAILURE() << "Could not parse message";
			                        } 			                 ++self->d_received;
			                        EXPECT_EQ(m.frameid(),self->d_received);
			                        EXPECT_EQ(m.timestamp(),self->d_received*20000);
			                        Read(self);
		                        });
	}
};

ConnectionUTest::ConnectionUTest()
	: d_acceptor(d_context,boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(),12345))
	, d_rejector(d_context,boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(),12346)) {
	d_rejectLoop = [this] {
		               auto connection = std::make_shared<IncommingConnection>(d_context,d_closed);
		               d_rejector.async_accept(connection->d_socket,[this,connection](const boost::system::error_code & ec) {
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
		               auto connection = std::make_shared<IncommingConnection>(d_context,d_closed);
		               d_acceptor.async_accept(connection->d_socket,[this,connection](const boost::system::error_code & ec) {
				               EXPECT_EQ(!ec,true);
				               IncommingConnection::Read(connection);
				               d_accept.SignalOne();
				               d_acceptLoop();
			               });
	               };


}

void ConnectionUTest::SetUp() {
	d_context.reset();
	d_rejected = false;
	d_rejectLoop();
	d_acceptLoop();

	boost::asio::post(d_context,[this](){ d_running.SignalOne(); });

	d_thread = std::thread([this](){
			d_context.run();
		});
}

void ConnectionUTest::TearDown() {
	d_context.stop();
	d_thread.join();
}


TEST_F(ConnectionUTest,DoesNotMangle) {
	d_running.Wait();
	Connection::Ptr connection;

	try {
		connection = Connection::Create(d_context, "localhost", 12345);
	} catch (const std::exception & e) {
		FAIL() << "Could not connect: " << e.what();
	}
	d_accept.Wait();
	std::vector<fort::hermes::FrameReadout> messages;
	for (size_t i = 0; i < 4; ++i) {
		fort::hermes::FrameReadout m;
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
	auto connection = Connection::Create(d_context,"localhost",12346,std::chrono::milliseconds(5));

	fort::hermes::FrameReadout m;
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

} // namespace artemis
} // namespace fort
