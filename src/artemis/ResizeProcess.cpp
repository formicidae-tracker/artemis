#include "ResizeProcess.h"

#include <opencv2/imgproc.hpp>

#include <glog/logging.h>

ResizeProcess::ResizeProcess(size_t height)
	: d_height(height)
	, d_initialized(false) {
}

ResizeProcess::~ResizeProcess() {}


void ResizeProcess::operator()(const Frame::Ptr & frame, const cv::Mat & upstream, cv::Mat & result) {
	if (d_initialized == false) {
		size_t width = (upstream.cols * d_height) / upstream.rows;
		d_resized = cv::Mat(d_height,width,CV_8U);
		d_initialized = true;
	}

	cv::resize(upstream,d_resized,d_resized.size());

	cv::cvtColor(d_resized,result,CV_GRAY2RGB);
}
