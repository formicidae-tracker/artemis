#pragma once

#include <gtest/gtest.h>


#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <thread>
#include <condition_variable>

namespace fort {
namespace artemis {

class Barrier {
public:
	Barrier() : d_active(false) {}

	void Wait() {
		std::unique_lock<std::mutex> lock(d_mutex);
		d_signal.wait(lock,[this]()-> bool {
				if (!d_active) {
					return false;
				}
				d_active = false;
				return true;
			});
	}

	void SignalOne() {
		std::lock_guard<std::mutex> lock(d_mutex);
		d_active = true;
		d_signal.notify_one();
	}

private:
	bool d_active;
	std::mutex d_mutex;
	std::condition_variable d_signal;
};


class ConnectionUTest :  public testing::Test {
protected:
	ConnectionUTest();

	void SetUp();
	void TearDown();


	boost::asio::io_context        d_context;
	boost::asio::ip::tcp::acceptor d_acceptor,d_rejector;
	std::function<void ()>         d_acceptLoop,d_rejectLoop;
	std::thread                    d_thread;
	Barrier                        d_running,d_accept,d_closed,d_read;
	bool                           d_rejected;
};

} // namespace artemis
} // namespace fort
