#include "ResizeProcess.h"

#include <opencv2/imgproc.hpp>

ResizeProcess::ResizeProcess(size_t width, size_t height)
	: d_resized(height,width,CV_8U) {
}

ResizeProcess::~ResizeProcess() {}


std::vector<ProcessFunction> ResizeProcess::Prepare(size_t nbProcess, const cv::Size & size) {
	return {};
}

void ResizeProcess::Finalize(const cv::Mat & upstream,fort::FrameReadout & readout, cv::Mat & result) {
	cv::resize(upstream,d_resized,d_resized.size());
	cv::cvtColor(d_resized,result,CV_GRAY2RGB);
}
