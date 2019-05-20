#pragma once

#include "ProcessDefinition.h"

#include <mutex>

#include <asio/io_service.hpp>
#include <asio/posix/stream_descriptor.hpp>
#include <asio/streambuf.hpp>



class OutputProcess : public ProcessDefinition {
public:
	OutputProcess(asio::io_service & service, bool writeHeader);
	virtual ~OutputProcess();

	std::vector<ProcessFunction> Prepare(size_t maxProcess, const cv::Size &);
private:

	asio::io_service & d_service;
	std::mutex         d_mutex;
	bool               d_done;
	bool               d_writeHeader;

	asio::posix::stream_descriptor d_stream;
	std::vector <uint8_t>          d_data;
};
