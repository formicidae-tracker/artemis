#include "OutputProcess.h"


#include "utils/PosixCall.h"

#include <glog/logging.h>


#include <asio/write.hpp>

OutputProcess::OutputProcess(asio::io_service & service)
	: d_service(service)
	, d_done(true)
	, d_stream(service,STDOUT_FILENO) {

	int flags = fcntl(STDOUT_FILENO, F_GETFL);
	if ( flags == -1 ) {
		throw ARTEMIS_SYSTEM_ERROR(fcntl,errno);
	}
	p_call(fcntl,STDOUT_FILENO,flags | O_NONBLOCK);

}

OutputProcess::~OutputProcess() {}

std::vector<ProcessFunction> OutputProcess::Prepare(size_t maxProcess, const cv::Size &) {
	return {[this](const Frame::Ptr &, const cv::Mat & upstream, fort::FrameReadout & readout, cv::Mat & result) {
			{
				std::lock_guard<std::mutex> lock(d_mutex);
				if (!d_done) {
					LOG(ERROR) << "Frame output: skipping as previous is not done";
					return d_done;
				}
				d_done = false;
			}
			result = upstream.clone();
			asio::async_write(d_stream,
			                  asio::const_buffers_1(result.datastart,result.dataend-result.datastart),
			                  [this](const asio::error_code & ec,
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
