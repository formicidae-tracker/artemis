#pragma once

#include "ProcessDefinition.h"

#include <mutex>

#include <asio/io_service.hpp>
#include <asio/posix/stream_descriptor.hpp>
#include <asio/streambuf.hpp>



class OutputProcess {
public:
	OutputProcess(asio::io_service & service);
	virtual ~OutputProcess();


	void operator()(const Frame::Ptr & frame,
	                const cv::Mat & upstream,
	                const fort::FrameReadout & readout,
	                cv::Mat & result);
private:

	asio::io_service & d_service;
	std::mutex         d_mutex;
	bool               d_done;

	asio::posix::stream_descriptor d_stream;
};
