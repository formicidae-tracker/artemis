#include "OutputProcess.h"


#include "utils/PosixCall.h"

#include <glog/logging.h>


#include <asio/write.hpp>

OutputProcess::OutputProcess(asio::io_service & service, bool writeHeader)
	: d_service(service)
	, d_done(true)
	, d_writeHeader(writeHeader)
	, d_stream(service,STDOUT_FILENO) {

	int flags = fcntl(STDOUT_FILENO, F_GETFL);
	if ( flags == -1 ) {
		throw ARTEMIS_SYSTEM_ERROR(fcntl-get,errno);
	}
	p_call(fcntl,STDOUT_FILENO,F_SETFL,flags | O_NONBLOCK);



}

OutputProcess::~OutputProcess() {}

std::vector<ProcessFunction> OutputProcess::Prepare(size_t maxProcess, const cv::Size &) {
	return {[this](const Frame::Ptr & frame, const cv::Mat & upstream, fort::hermes::FrameReadout & readout, cv::Mat & result) {
			{
				std::lock_guard<std::mutex> lock(d_mutex);
				if (d_done == false) {
					LOG(ERROR) << "Frame output: skipping as previous is not done";
					return;
				}
				d_done = false;
			}
			size_t totalSize = upstream.dataend - upstream.datastart;
			if ( d_writeHeader == true ) {
				totalSize += 3*sizeof(uint64_t);
			}
			if (totalSize != d_data.size() ) {
				d_data.resize(totalSize);
			}
			uint8_t * dataStart = &(d_data[0]);
			if (d_writeHeader ) {
				uint64_t * frameID = (uint64_t*)dataStart;
				uint64_t * width = frameID + 1;
				uint64_t * height = frameID + 2;
				dataStart += 3*sizeof(uint64_t);
				*frameID  = frame->ID();
				*width = upstream.cols;
				*height = upstream.rows;
			}

			cv::Mat out = cv::Mat(upstream.rows, upstream.cols,upstream.type(),dataStart);
			upstream.copyTo(out);
			asio::async_write(d_stream,
			                  asio::const_buffers_1(&(d_data[0]),d_data.size()),
			                  [this,out](const asio::error_code & ec,
			                         std::size_t) {
				                  if (ec) {
					                  LOG(ERROR) << "Could not output data: " << ec;
				                  }
				                  std::lock_guard<std::mutex> lock(d_mutex);
				                  d_done = true;
			                  });
		}
	};
}
