#include "ConnectionUTest.h"


#include <asio/error.hpp>
#include <asio/connect.hpp>
#include <asio/streambuf.hpp>
#include <asio/read.hpp>
#include <asio/write.hpp>
#include <google/protobuf/util/delimited_message_util.h>
#include "hermes/FrameReadout.pb.h"

#include "Connection.h"

class IncommingConnection {
public :
	IncommingConnection(asio::io_service & service) : d_socket(service), d_continue(true) {}

	asio::ip::tcp::socket  d_socket;
	asio::streambuf        d_streambuf;
	bool                   d_continue;
};

ConnectionUTest::ConnectionUTest()
	: d_acceptor(d_service,asio::ip::tcp::endpoint(asio::ip::tcp::v4(),12345)) {
	d_acceptLoop = [this]{
		auto connection = std::make_shared<IncommingConnection>(d_acceptor.get_io_service());
		d_acceptor.async_accept(connection->d_socket,[this,connection](const asio::error_code & ec) {
				d_acceptLoop();
				EXPECT_EQ(!ec,true);
				d_accept.SignalOne();
				asio::async_read(connection->d_socket,connection->d_streambuf,[this,connection](const asio::error_code & ec,std::size_t) {
						EXPECT_EQ(ec,asio::error::eof);
						std::istream is(&connection->d_streambuf);
						google::protobuf::io::IstreamInputStream iss(&is);
						bool stop = false;
						fort::FrameReadout m;
						uint64_t i = 1;
						while(!stop) {
							bool clean_eof;
							bool parsed = google::protobuf::util::ParseDelimitedFromZeroCopyStream(&m,&iss,&clean_eof);
							if(parsed == false && clean_eof == true) {
								stop = true;
								continue;
							}
							if (parsed = false ) {
								stop = true;
							} else {
								EXPECT_EQ(m.frameid(),i);
								++i;
							}
							EXPECT_EQ(parsed,true);
							EXPECT_EQ(clean_eof,false);
						}

						d_closed.SignalOne();
					});
			});
	};

}




void ConnectionUTest::SetUp() {
	d_service.reset();

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

	auto socket = std::make_shared<asio::ip::tcp::socket>(d_service);

	try {
		using namespace asio::ip;

		tcp::resolver resolver(d_service);
		tcp::resolver::query query("localhost","12345");
		tcp::resolver::iterator ep = resolver.resolve(query);
		asio::connect(*socket,ep);

	} catch (const std::exception & e) {
		FAIL() << "Could not connect: " << e.what();
	}
	d_accept.Wait();

	socket->close();
	d_closed.Wait();

}
