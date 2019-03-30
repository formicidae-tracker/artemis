#pragma once

#include "ProcessDefinition.h"

#include <mutex>

#include <asio/io_service.hpp>
#include <asio/posix/stream_descriptor.hpp>
#include <asio/streambuf.hpp>



class OutputProcess : public ProcessDefinition {
public:
	OutputProcess(asio::io_service & service);
	virtual ~OutputProcess();

	std::vector<ProcessFunction> Prepare(size_t maxProcess, const cv::Size &);
private:

	asio::io_service & d_service;
	std::mutex         d_mutex;
	bool               d_done;

	asio::posix::stream_descriptor d_stream;
};
