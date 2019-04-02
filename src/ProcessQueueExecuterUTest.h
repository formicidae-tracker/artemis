#include <gtest/gtest.h>

#include <asio/io_service.hpp>

#include <vector>
#include <thread>
#include <memory>

class ProcessQueueExecuterUTest : public ::testing::Test {
protected:
	void SetUp();
	void TearDown();

	asio::io_service                        d_service;
	std::shared_ptr<asio::io_service::work> d_work;
	std::vector<std::thread>                d_threads;
};
