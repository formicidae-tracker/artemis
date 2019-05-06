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
		throw ARTEMIS_SYSTEM_ERROR(fcntl-get,errno);
	}
	p_call(fcntl,STDOUT_FILENO,F_SETFL,flags | O_NONBLOCK);

}

OutputProcess::~OutputProcess() {}

std::vector<ProcessFunction> OutputProcess::Prepare(size_t maxProcess, const cv::Size &) {
	return {[this](const Frame::Ptr &, const cv::Mat & upstream, fort::FrameReadout & readout, cv::Mat & result) {
			{
				std::lock_guard<std::mutex> lock(d_mutex);
				if (d_done == false) {
					LOG(ERROR) << "Frame output: skipping as previous is not done";
					return;
				}
				d_done = false;
			}
			cv::Mat out = upstream.clone();
			asio::async_write(d_stream,
			                  asio::const_buffers_1(out.datastart,out.dataend-out.datastart),
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
